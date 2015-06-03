#include <StdInc.h>

#include "txdread.d3d.hxx"

#include "streamutil.hxx"

#include "txdread.common.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility d3dNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider );

    texNativeImageBlock.EnterContext();

    try
    {
        if ( texNativeImageBlock.getBlockID() == CHUNK_STRUCT )
        {
            // For now, simply check the platform descriptor.
            // It can either be Direct3D 8 or Direct3D 9.
            uint32 platformDescriptor = texNativeImageBlock.readUInt32();

            if ( platformDescriptor == PLATFORM_D3D8 )
            {
                // This is a sure guess.
                texCompat = RWTEXCOMPAT_ABSOLUTE;
            }
            else if ( platformDescriptor == PLATFORM_D3D9 )
            {
                // Since Wardrum Studios has broken the platform descriptor rules, we can only say "maybe".
                texCompat = RWTEXCOMPAT_MAYBE;
            }
        }
    }
    catch( ... )
    {
        texNativeImageBlock.LeaveContext();

        throw;
    }

    texNativeImageBlock.LeaveContext();

    return texCompat;
}

void d3dNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    NativeTextureD3D *platformTex = (NativeTextureD3D*)nativeTex;

    // Make sure the texture has some qualities before it can even be written.
    ePaletteType paletteType = platformTex->paletteType;

    uint32 compressionType = platformTex->dxtCompression;

    // Get the platform of our texture.
    NativeTextureD3D::ePlatformType platformType = platformTex->platformType;

    if ( platformType == NativeTextureD3D::PLATFORM_D3D8 )
    {
        // Check the color order if we are not compressed.
        if ( compressionType == 0 )
        {
            eColorOrdering requiredColorOrder = COLOR_BGRA;

            if ( paletteType != PALETTE_NONE )
            {
                requiredColorOrder = COLOR_RGBA;
            }

            if ( platformTex->colorOrdering != requiredColorOrder )
            {
                throw RwException( "texture " + theTexture->GetName() + " has an invalid color order for writing" );
            }
        }
    }
    else if ( platformType == NativeTextureD3D::PLATFORM_D3D9 )
    {
        if ( !platformTex->hasD3DFormat )
        {
            throw RwException( "texture " + theTexture->GetName() + " has no representation in Direct3D 9" );
        }
    }
    else
    {
        throw RwException( "unknown Direct3D platform for serialization" );
    }

    uint32 serializePlatform;

    if ( platformType == NativeTextureD3D::PLATFORM_D3D8 )
    {
        serializePlatform = 8;
    }
    else if ( platformType == NativeTextureD3D::PLATFORM_D3D9 )
    {
        serializePlatform = 9;
    }
    else
    {
        assert( 0 );
    }

	// Struct
	{
		BlockProvider texNativeImageStruct( &outputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            d3d::textureMetaHeaderStructGeneric metaHeader;
            metaHeader.platformDescriptor = serializePlatform;
            metaHeader.texFormat.set( *theTexture );

            // Correctly write the name strings (for safety).
            // Even though we can read those name fields with zero-termination safety,
            // the engines are not guarranteed to do so.
            // Also, print a warning if the name is changed this way.
            writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture->GetName(), "name" );
            writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture->GetName(), "mask name" );

            // Construct raster flags.
            uint32 mipmapCount = platformTex->mipmaps.size();

            metaHeader.rasterFormat = generateRasterFormatFlags( platformTex->rasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps );

            texNativeImageStruct.write( &metaHeader, sizeof(metaHeader) );

            if (platformType == NativeTextureD3D::PLATFORM_D3D8)
            {
			    texNativeImageStruct.writeUInt32( platformTex->hasAlpha );
		    }
            else if (platformType == NativeTextureD3D::PLATFORM_D3D9)
            {
		        texNativeImageStruct.writeUInt32( (uint32)platformTex->d3dFormat );
		    }
            else
            {
                assert( 0 );
            }

            d3d::textureMetaHeaderStructDimInfo dimInfo;
            dimInfo.width = platformTex->mipmaps[ 0 ].layerWidth;
            dimInfo.height = platformTex->mipmaps[ 0 ].layerHeight;
            dimInfo.depth = platformTex->depth;
            dimInfo.mipmapCount = mipmapCount;
            dimInfo.rasterType = platformTex->rasterType;
            dimInfo.pad1 = 0;

            texNativeImageStruct.write( &dimInfo, sizeof(dimInfo) );

		    if (platformType == NativeTextureD3D::PLATFORM_D3D8)
            {
			    texNativeImageStruct.writeUInt8( compressionType );
            }
            else if (platformType == NativeTextureD3D::PLATFORM_D3D9)
            {
                d3d::textureContentInfoStruct contentInfo;
                contentInfo.hasAlpha = platformTex->hasAlpha;
                contentInfo.isCubeTexture = platformTex->isCubeTexture;
                contentInfo.autoMipMaps = platformTex->autoMipmaps;
                contentInfo.isCompressed = ( compressionType != 0 );
                contentInfo.pad = 0;

                texNativeImageStruct.write( (const char*)&contentInfo, sizeof(contentInfo) );
            }

		    /* Palette */
		    if (paletteType != PALETTE_NONE)
            {
                // Make sure we write as much data as the system expects.
                uint32 reqPalCount = getD3DPaletteCount(paletteType);

                uint32 palItemCount = platformTex->paletteSize;

                // Get the real data size of the palette.
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(platformTex->rasterFormat);

                uint32 paletteDataSize = getRasterDataSize( palItemCount, palRasterDepth );

                uint32 palByteWriteCount = writePartialBlockSafe(texNativeImageStruct, platformTex->palette, paletteDataSize, getRasterDataSize(reqPalCount, palRasterDepth));
        
                assert( palByteWriteCount * 8 / palRasterDepth == reqPalCount );
		    }

		    /* Texels */
		    for (uint32 i = 0; i < mipmapCount; i++)
            {
                const NativeTextureD3D::mipmapLayer& mipLayer = platformTex->mipmaps[ i ];

			    uint32 texDataSize = mipLayer.dataSize;

			    texNativeImageStruct.writeUInt32( texDataSize );

                const void *texelData = mipLayer.texels;

			    texNativeImageStruct.write( texelData, texDataSize );
		    }
        }
        catch( ... )
        {
            texNativeImageStruct.LeaveContext();

            throw;
        }

        texNativeImageStruct.LeaveContext();
	}

	// Extension
	engineInterface->SerializeExtensions( theTexture, outputProvider );
}

inline eCompressionType getD3DCompressionType( const NativeTextureD3D *nativeTex )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    uint32 dxtType = nativeTex->dxtCompression;

    if ( dxtType != 0 )
    {
        if ( dxtType == 1 )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( dxtType == 2 )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( dxtType == 3 )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( dxtType == 4 )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else if ( dxtType == 5 )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else
        {
            throw RwException( "unsupported DXT compression" );
        }
    }

    return rwCompressionType;
}

// Pixel movement functions.
void d3dNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native texture type.
    NativeTextureD3D *platformTex = (NativeTextureD3D*)objMem;

    // The pixel capabilities system has been mainly designed around PC texture optimization.
    // This means that we should be able to directly copy the Direct3D surface data into pixelsOut.
    // If not, we need to adjust, make a new library version.

    // We need to decide how to traverse palette runtime optimization data.

    // Determine the compression type.
    eCompressionType rwCompressionType = getD3DCompressionType( platformTex );

    // Put ourselves into the pixelsOut struct!
    pixelsOut.rasterFormat = platformTex->rasterFormat;
    pixelsOut.depth = platformTex->depth;
    pixelsOut.colorOrder = platformTex->colorOrdering;
    pixelsOut.paletteType = platformTex->paletteType;
    pixelsOut.paletteData = platformTex->palette;
    pixelsOut.paletteSize = platformTex->paletteSize;
    pixelsOut.compressionType = rwCompressionType;
    pixelsOut.hasAlpha = platformTex->hasAlpha;

    pixelsOut.autoMipmaps = platformTex->autoMipmaps;
    pixelsOut.cubeTexture = platformTex->isCubeTexture;
    pixelsOut.rasterType = platformTex->rasterType;

    // Now, the texels.
    uint32 mipmapCount = platformTex->mipmaps.size();

    pixelsOut.mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureD3D::mipmapLayer& srcLayer = platformTex->mipmaps[ n ];

        pixelDataTraversal::mipmapResource newLayer;

        newLayer.width = srcLayer.width;
        newLayer.height = srcLayer.height;
        newLayer.mipWidth = srcLayer.layerWidth;
        newLayer.mipHeight = srcLayer.layerHeight;

        newLayer.texels = srcLayer.texels;
        newLayer.dataSize = srcLayer.dataSize;

        // Put this layer.
        pixelsOut.mipmaps[ n ] = newLayer;
    }

    // We never allocate new texels, actually.
    pixelsOut.isNewlyAllocated = false;
}

inline void convertCompatibleRasterFormat(
    NativeTextureD3D::ePlatformType platformType,
    eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, ePaletteType paletteType
)
{
    eRasterFormat srcRasterFormat = rasterFormat;
    eColorOrdering srcColorOrder = colorOrder;
    uint32 srcDepth = depth;

    if ( paletteType != PALETTE_NONE )
    {
        if ( paletteType == PALETTE_4BIT || paletteType == PALETTE_8BIT )
        {
            // We only support 8bit depth palette.
            depth = 8;
        }
        else
        {
            assert( 0 );
        }

        // All palettes have RGBA color order.
        colorOrder = COLOR_RGBA;
    }
    else
    {
        if ( srcRasterFormat == RASTER_1555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_565 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_4444 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_8888 )
        {
            depth = 32;

            if ( platformType == NativeTextureD3D::PLATFORM_D3D9 )
            {
                // Can be both RGBA and BGRA.
                if ( srcColorOrder != COLOR_RGBA ||
                     srcColorOrder != COLOR_BGRA )
                {
                    colorOrder = COLOR_BGRA;
                }
            }
            else
            {
                colorOrder = COLOR_BGRA;
            }
        }
        else if ( srcRasterFormat == RASTER_888 )
        {
            if ( platformType == NativeTextureD3D::PLATFORM_D3D9 )
            {
                if ( srcColorOrder == COLOR_BGRA )
                {
                    if ( srcDepth != 24 && srcDepth != 32 )
                    {
                        depth = 32;
                    }
                }
                else if ( srcColorOrder == COLOR_RGBA )
                {
                    depth = 32;
                }
                else
                {
                    depth = 32;
                    colorOrder = COLOR_BGRA;
                }
            }
            else
            {
                depth = 32;
                colorOrder = COLOR_BGRA;
            }
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else
        {
            // Any unknown raster formats need conversion to full quality.
            rasterFormat = RASTER_8888;
            depth = 32;
            colorOrder = COLOR_BGRA;
        }
    }
}

void d3dNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // We want to remove our current texels and put the new ones into us instead.
    // By chance, the runtime makes sure the texture is already empty.
    // So optimize your routine to that.

    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    // Remove our own texels first, since the runtime wants to overwrite them.
    //nativeTex->clearTexelData();

    // We need to ensure that the pixels we set to us are compatible.
    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;
    ePaletteType srcPaletteType = pixelsIn.paletteType;
    void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    eColorOrdering dstColorOrder = srcColorOrder;

    // Determine the compression type.
    uint32 dxtType;

    eCompressionType rwCompressionType = pixelsIn.compressionType;

    bool hasAlpha = pixelsIn.hasAlpha;

    if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // TODO: actually, before we can acquire texels, we MUST make sure they are in
        // a compatible format. If they are not, then we will most likely allocate
        // new pixel information, instead in a compatible format. The same has to be
        // made for the XBOX implementation.

        // Make sure this texture is writable.
        // If we are on D3D, we have to avoid typical configurations that may come from
        // other hardware.
        NativeTextureD3D::ePlatformType platformType = nativeTex->platformType;

        convertCompatibleRasterFormat(
            platformType,
            dstRasterFormat, dstColorOrder, dstDepth, srcPaletteType
        );
 
        dxtType = 0;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
        // TODO: do we properly handle DXT1 with alpha...?

        dxtType = 1;

        dstRasterFormat = ( hasAlpha ) ? ( RASTER_1555 ) : ( RASTER_565 );
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
    {
        dxtType = 2;

        dstRasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
    {
        dxtType = 3;

        dstRasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        dxtType = 4;
        
        dstRasterFormat = RASTER_4444;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        dxtType = 5;

        dstRasterFormat = RASTER_4444;
    }
    else
    {
        throw RwException( "unknown pixel compression type" );
    }

    // Check whether we can directly acquire or have to allocate a new copy.
    bool canDirectlyAcquire;

    if ( rwCompressionType != RWCOMPRESS_NONE || srcRasterFormat == dstRasterFormat && srcDepth == dstDepth && srcColorOrder == dstColorOrder )
    {
        canDirectlyAcquire = true;
    }
    else
    {
        canDirectlyAcquire = false;
    }

    // If we have a palette, we must convert it aswell.
    void *dstPaletteData = srcPaletteData;

    if ( canDirectlyAcquire == false )
    {
        if ( srcPaletteType != PALETTE_NONE )
        {
            // Allocate a new palette.
            uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
            uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

            uint32 palDataSize = getRasterDataSize( srcPaletteSize, dstPalRasterDepth );

            dstPaletteData = engineInterface->PixelAllocate( palDataSize );

            // Convert the colors.
            ConvertPaletteData(
                srcPaletteData, dstPaletteData,
                srcPaletteSize, srcPaletteSize,
                srcRasterFormat, srcColorOrder, srcPalRasterDepth,
                dstRasterFormat, dstColorOrder, dstPalRasterDepth
            );
        }
    }

    // Update our own texels.
    nativeTex->rasterFormat = dstRasterFormat;
    nativeTex->depth = dstDepth;
    nativeTex->palette = dstPaletteData;
    nativeTex->paletteSize = srcPaletteSize;
    nativeTex->paletteType = srcPaletteType;
    nativeTex->colorOrdering = dstColorOrder;
    nativeTex->hasAlpha = hasAlpha;

    uint32 mipmapCount = pixelsIn.mipmaps.size();

    // Properly set the automipmaps field.
    bool autoMipmaps = pixelsIn.autoMipmaps;

    if ( mipmapCount > 1 )
    {
        autoMipmaps = false;
    }

    nativeTex->autoMipmaps = autoMipmaps;
    nativeTex->isCubeTexture = pixelsIn.cubeTexture;
    nativeTex->rasterType = pixelsIn.rasterType;

    nativeTex->dxtCompression = dxtType;

    // Apply the pixel data.
    nativeTex->mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const pixelDataTraversal::mipmapResource& srcLayer = pixelsIn.mipmaps[ n ];

        // Get the mipmap properties on the stac.
        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;

        uint32 layerWidth = srcLayer.mipWidth;
        uint32 layerHeight = srcLayer.mipHeight;

        void *srcTexels = srcLayer.texels;
        uint32 srcDataSize = srcLayer.dataSize;

        void *dstTexels = srcTexels;
        uint32 dstDataSize = srcDataSize;

        // If we cannot directly acquire, allocate a new copy and convert.
        if ( canDirectlyAcquire == false )
        {
            assert( rwCompressionType == RWCOMPRESS_NONE );

            // Call the core's helper function.
            ConvertMipmapLayer(
                engineInterface,
                srcLayer,
                srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstColorOrder, srcPaletteType,
                true,
                dstTexels, dstDataSize
            );
        }

        NativeTextureD3D::mipmapLayer newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // We need to set the Direct3D format field.
    nativeTex->updateD3DFormat();

    // For now, we can always directly acquire pixels.
    feedbackOut.hasDirectlyAcquired = canDirectlyAcquire;
}

void d3dNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Just remove the pixels from us.
    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    if ( deallocate )
    {
        // Delete all pixels.
        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );

        // Delete palette.
        if ( nativeTex->paletteType != PALETTE_NONE )
        {
            if ( void *paletteData = nativeTex->palette )
            {
                engineInterface->PixelFree( paletteData );

                nativeTex->palette = NULL;
            }

            nativeTex->paletteType = PALETTE_NONE;
        }
    }

    // Unset the pixels.
    nativeTex->mipmaps.clear();

    nativeTex->palette = NULL;
    nativeTex->paletteType = PALETTE_NONE;
    nativeTex->paletteSize = 0;

    // Reset general properties for cleanliness.
    nativeTex->rasterFormat = RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->hasD3DFormat = false;
    nativeTex->dxtCompression = 0;
    nativeTex->hasAlpha = false;
    nativeTex->colorOrdering = COLOR_BGRA;
}

struct d3dMipmapManager
{
    NativeTextureD3D *nativeTex;

    inline d3dMipmapManager( NativeTextureD3D *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // We just return stuff.
        widthOut = mipLayer.width;
        heightOut = mipLayer.height;

        layerWidthOut = mipLayer.layerWidth;
        layerHeightOut = mipLayer.layerHeight;
        
        dstRasterFormat = nativeTex->rasterFormat;
        dstColorOrder = nativeTex->colorOrdering;
        dstDepth = nativeTex->depth;

        dstPaletteType = nativeTex->paletteType;
        dstPaletteData = nativeTex->palette;
        dstPaletteSize = nativeTex->paletteSize;

        dstCompressionType = getD3DCompressionType( nativeTex );

        hasAlpha = nativeTex->hasAlpha;

        dstTexelsOut = mipLayer.texels;
        dstDataSizeOut = mipLayer.dataSize;

        isNewlyAllocatedOut = false;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Convert to our format.
        bool hasChanged =
            ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                nativeTex->rasterFormat, nativeTex->depth, nativeTex->colorOrdering,
                nativeTex->paletteType, nativeTex->palette, nativeTex->paletteSize,
                getD3DCompressionType( nativeTex ),
                false,
                width, height,
                srcTexels, dataSize
            );

        // We have no more auto mipmaps.
        nativeTex->autoMipmaps = false;

        // Store the data.
        mipLayer.width = width;
        mipLayer.height = height;

        mipLayer.layerWidth = layerWidth;
        mipLayer.layerHeight = layerHeight;

        mipLayer.texels = srcTexels;
        mipLayer.dataSize = dataSize;

        hasDirectlyAcquiredOut = ( hasChanged == false );
    }
};

bool d3dNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    d3dMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureD3D::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool d3dNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    d3dMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureD3D::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void d3dNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    virtualClearMipmaps <NativeTextureD3D::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void d3dNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

    uint32 mipmapCount = nativeTex->mipmaps.size();

    infoOut.mipmapCount = mipmapCount;

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount > 0 )
    {
        baseWidth = nativeTex->mipmaps[ 0 ].layerWidth;
        baseHeight = nativeTex->mipmaps[ 0 ].layerHeight;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
}

// Platform update API function.
void NativeTextureD3D::NativeSetPlatformType( ePlatformType newPlatform )
{
    Interface *engineInterface = this->engineInterface;

    // Make sure that our pixels are compatible.
    // This can only be a problem for uncompressed rasters.
    uint32 dxtType = this->dxtCompression;

    if ( dxtType == 0 )
    {
        eRasterFormat srcRasterFormat = this->rasterFormat;
        eColorOrdering srcColorOrder = this->colorOrdering;
        uint32 srcDepth = this->depth;
        
        ePaletteType srcPaletteType = this->paletteType;
        void *srcPaletteData = this->palette;
        uint32 srcPaletteSize = this->paletteSize;

        eRasterFormat dstRasterFormat = srcRasterFormat;
        eColorOrdering dstColorOrder = srcColorOrder;
        uint32 dstDepth = srcDepth;

        ePaletteType dstPaletteType = srcPaletteType;
        void *dstPaletteData = srcPaletteData;
        uint32 dstPaletteSize = srcPaletteSize;

        convertCompatibleRasterFormat(
            newPlatform,
            dstRasterFormat, dstColorOrder, dstDepth, srcPaletteType
        );

        // Check whether we have to convert the pixels.
        if ( srcRasterFormat != dstRasterFormat || srcColorOrder != dstColorOrder || srcDepth != dstDepth )
        {
            // Do it.
            if ( srcPaletteType != PALETTE_NONE )
            {
                // We have to update the palette.
                dstPaletteData = srcPaletteData;

                uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
                uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

                if ( srcPalRasterDepth != dstPalRasterDepth )
                {
                    uint32 newPalDataSize = getRasterDataSize( srcPaletteSize, dstPalRasterDepth );

                    dstPaletteData = engineInterface->PixelAllocate( newPalDataSize );
                }

                ConvertPaletteData(
                    srcPaletteData, dstPaletteData,
                    srcPaletteSize, srcPaletteSize,
                    srcRasterFormat, srcColorOrder, srcPalRasterDepth,
                    dstRasterFormat, dstColorOrder, dstPalRasterDepth
                );

                if ( dstPaletteData != srcPaletteData )
                {
                    engineInterface->PixelFree( srcPaletteData );
                }
            }

            // Now do all mipmap layers.
            if ( srcPaletteType == PALETTE_NONE || srcDepth != dstDepth )
            {
                uint32 mipmapCount = this->mipmaps.size();

                for ( uint32 n = 0; n < mipmapCount; n++ )
                {
                    NativeTextureD3D::mipmapLayer& mipLayer = this->mipmaps[ n ];

                    uint32 mipWidth = mipLayer.width;
                    uint32 mipHeight = mipLayer.height;

                    uint32 layerWidth = mipLayer.layerWidth;
                    uint32 layerHeight = mipLayer.layerHeight;

                    void *srcTexels = mipLayer.texels;
                    uint32 dataSize = mipLayer.dataSize;

                    void *dstTexels = NULL;
                    uint32 dstDataSize = 0;

                    bool hasConverted =
                        ConvertMipmapLayerNative(
                            engineInterface,
                            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, dataSize,
                            srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, dstPaletteData, srcPaletteSize, RWCOMPRESS_NONE,
                            dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType, dstPaletteData, dstPaletteSize, RWCOMPRESS_NONE,
                            false,
                            mipWidth, mipHeight,
                            dstTexels, dstDataSize
                        );

                    if ( hasConverted )
                    {
                        engineInterface->PixelFree( srcTexels );

                        mipLayer.width = mipWidth;
                        mipLayer.height = mipHeight;

                        mipLayer.texels = dstTexels;
                        mipLayer.dataSize = dstDataSize;
                    }
                }
            }

            // Update fields.
            if ( srcRasterFormat != dstRasterFormat )
            {
                this->rasterFormat = dstRasterFormat;
            }

            if ( srcColorOrder != dstColorOrder )
            {
                this->colorOrdering = dstColorOrder;
            }

            if ( srcDepth != dstDepth )
            {
                this->depth = dstDepth;
            }

            if ( srcPaletteType != dstPaletteType )
            {
                this->paletteType = dstPaletteType;
            }

            if ( srcPaletteData != dstPaletteData )
            {
                // Remember to release old resources!
                engineInterface->PixelFree( srcPaletteData );

                this->palette = dstPaletteData;
            }

            if ( srcPaletteSize != dstPaletteSize )
            {
                this->paletteSize = dstPaletteSize;
            }

            // We need to update our D3DFORMAT field.
            this->updateD3DFormat();
        }
    }

    this->platformType = newPlatform;
}

}