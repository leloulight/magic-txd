#include <StdInc.h>

#include "pixelformat.hxx"

#include "txdread.d3d8.hxx"

#include "streamutil.hxx"

#include "txdread.common.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility d3d8NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
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

void d3d8NativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    NativeTextureD3D8 *platformTex = (NativeTextureD3D8*)nativeTex;

    // Make sure the texture has some qualities before it can even be written.
    ePaletteType paletteType = platformTex->paletteType;

    uint32 compressionType = platformTex->dxtCompression;

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

	// Struct
	{
		BlockProvider texNativeImageStruct( &outputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            d3d8::textureMetaHeaderStructGeneric metaHeader;
            metaHeader.platformDescriptor = PLATFORM_D3D8;

            texFormatInfo texFormat;
            texFormat.set( *theTexture );

            metaHeader.texFormat = texFormat;

            // Correctly write the name strings (for safety).
            // Even though we can read those name fields with zero-termination safety,
            // the engines are not guarranteed to do so.
            // Also, print a warning if the name is changed this way.
            writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture->GetName(), "name" );
            writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture->GetName(), "mask name" );

            // Construct raster flags.
            uint32 mipmapCount = platformTex->mipmaps.size();

            metaHeader.rasterFormat = generateRasterFormatFlags( platformTex->rasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps );

			metaHeader.hasAlpha = platformTex->hasAlpha;
            metaHeader.width = platformTex->mipmaps[ 0 ].layerWidth;
            metaHeader.height = platformTex->mipmaps[ 0 ].layerHeight;
            metaHeader.depth = platformTex->depth;
            metaHeader.mipmapCount = mipmapCount;
            metaHeader.rasterType = platformTex->rasterType;
            metaHeader.pad1 = 0;
            metaHeader.dxtCompression = compressionType;

            texNativeImageStruct.write( &metaHeader, sizeof(metaHeader) );

		    /* Palette */
		    if (paletteType != PALETTE_NONE)
            {
                // Make sure we write as much data as the system expects.
                uint32 reqPalCount = getD3DPaletteCount(paletteType);

                uint32 palItemCount = platformTex->paletteSize;

                // Get the real data size of the palette.
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(platformTex->rasterFormat);

                uint32 paletteDataSize = getPaletteDataSize( palItemCount, palRasterDepth );

                uint32 palByteWriteCount = writePartialBlockSafe(texNativeImageStruct, platformTex->palette, paletteDataSize, getPaletteDataSize(reqPalCount, palRasterDepth));
        
                assert( palByteWriteCount * 8 / palRasterDepth == reqPalCount );
		    }

		    /* Texels */
		    for (uint32 i = 0; i < mipmapCount; i++)
            {
                const NativeTextureD3D8::mipmapLayer& mipLayer = platformTex->mipmaps[ i ];

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

inline eCompressionType getD3DCompressionType( const NativeTextureD3D8 *nativeTex )
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
void d3d8NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native texture type.
    NativeTextureD3D8 *platformTex = (NativeTextureD3D8*)objMem;

    // The pixel capabilities system has been mainly designed around PC texture optimization.
    // This means that we should be able to directly copy the Direct3D surface data into pixelsOut.
    // If not, we need to adjust, make a new library version.

    // We need to decide how to traverse palette runtime optimization data.

    // Determine the compression type.
    eCompressionType rwCompressionType = getD3DCompressionType( platformTex );

    // Put ourselves into the pixelsOut struct!
    pixelsOut.rasterFormat = platformTex->rasterFormat;
    pixelsOut.depth = platformTex->depth;
    pixelsOut.rowAlignment = getD3DTextureDataRowAlignment();
    pixelsOut.colorOrder = platformTex->colorOrdering;
    pixelsOut.paletteType = platformTex->paletteType;
    pixelsOut.paletteData = platformTex->palette;
    pixelsOut.paletteSize = platformTex->paletteSize;
    pixelsOut.compressionType = rwCompressionType;
    pixelsOut.hasAlpha = platformTex->hasAlpha;

    pixelsOut.autoMipmaps = platformTex->autoMipmaps;
    pixelsOut.cubeTexture = false;
    pixelsOut.rasterType = platformTex->rasterType;

    // Now, the texels.
    uint32 mipmapCount = platformTex->mipmaps.size();

    pixelsOut.mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureD3D8::mipmapLayer& srcLayer = platformTex->mipmaps[ n ];

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
    eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, ePaletteType& paletteType
)
{
    eRasterFormat srcRasterFormat = rasterFormat;
    eColorOrdering srcColorOrder = colorOrder;
    uint32 srcDepth = depth;
    ePaletteType srcPaletteType = paletteType;

    if ( srcPaletteType != PALETTE_NONE )
    {
        if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
        {
            // We only support 8bit depth palette.
            depth = 8;
        }
        else if ( srcPaletteType == PALETTE_4BIT_LSB )
        {
            depth = 8;
            
            // We must reorder the palette.
            paletteType = PALETTE_4BIT;
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

            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_888 )
        {
            depth = 32;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
        }
        else if ( srcRasterFormat == RASTER_LUM )
        {
            // We only support 8bit LUM, actually.
            depth = 8;
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

void d3d8NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // We want to remove our current texels and put the new ones into us instead.
    // By chance, the runtime makes sure the texture is already empty.
    // So optimize your routine to that.

    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    // Remove our own texels first, since the runtime wants to overwrite them.
    //nativeTex->clearTexelData();

    // We need to ensure that the pixels we set to us are compatible.
    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    uint32 srcRowAlignment = pixelsIn.rowAlignment;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;
    ePaletteType srcPaletteType = pixelsIn.paletteType;
    void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    uint32 dstRowAlignment = getD3DTextureDataRowAlignment();
    eColorOrdering dstColorOrder = srcColorOrder;

    ePaletteType dstPaletteType = srcPaletteType;

    // Determine the compression type.
    uint32 dxtType;

    eCompressionType rwCompressionType = pixelsIn.compressionType;

    bool hasAlpha = pixelsIn.hasAlpha;

    // Check whether we can directly acquire or have to allocate a new copy.
    bool canDirectlyAcquire;

    if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // TODO: actually, before we can acquire texels, we MUST make sure they are in
        // a compatible format. If they are not, then we will most likely allocate
        // new pixel information, instead in a compatible format. The same has to be
        // made for the XBOX implementation.

        // Make sure this texture is writable.
        // If we are on D3D, we have to avoid typical configurations that may come from
        // other hardware.
        convertCompatibleRasterFormat(
            dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType
        );
 
        dxtType = 0;

        canDirectlyAcquire =
            !doesPixelDataNeedConversion(
                pixelsIn,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType
            );
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
        dxtType = 1;

        dstRasterFormat = ( hasAlpha ) ? ( RASTER_1555 ) : ( RASTER_565 );

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
    {
        dxtType = 2;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
    {
        dxtType = 3;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        dxtType = 4;
        
        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        dxtType = 5;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else
    {
        throw RwException( "unknown pixel compression type" );
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

            uint32 palDataSize = getPaletteDataSize( srcPaletteSize, dstPalRasterDepth );

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
    nativeTex->paletteType = dstPaletteType;
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
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType,
                true,
                dstTexels, dstDataSize
            );
        }

        NativeTextureD3D8::mipmapLayer newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // For now, we can always directly acquire pixels.
    feedbackOut.hasDirectlyAcquired = canDirectlyAcquire;
}

void d3d8NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Just remove the pixels from us.
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

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
    nativeTex->dxtCompression = 0;
    nativeTex->hasAlpha = false;
    nativeTex->colorOrdering = COLOR_BGRA;
}

struct d3d8MipmapManager
{
    NativeTextureD3D8 *nativeTex;

    inline d3d8MipmapManager( NativeTextureD3D8 *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
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
        dstRowAlignment = getD3DTextureDataRowAlignment();

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
        NativeTextureD3D8::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
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
                rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                nativeTex->rasterFormat, nativeTex->depth, getD3DTextureDataRowAlignment(), nativeTex->colorOrdering,
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

bool d3d8NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    d3d8MipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureD3D8::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool d3d8NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    d3d8MipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureD3D8::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void d3d8NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

    virtualClearMipmaps <NativeTextureD3D8::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void d3d8NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

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

void d3d8NativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureD3D8 *nativeTexture = (NativeTextureD3D8*)objMem;

    std::string formatString;

    int dxtType = nativeTexture->dxtCompression;

    if ( dxtType != 0 )
    {
        if ( dxtType == 1 )
        {
            formatString = "DXT1";
        }
        else if ( dxtType == 2 )
        {
            formatString = "DXT2";
        }
        else if ( dxtType == 3 )
        {
            formatString = "DXT3";
        }
        else if ( dxtType == 4 )
        {
            formatString = "DXT4";
        }
        else if ( dxtType == 5 )
        {
            formatString = "DXT5";
        }
        else
        {
            formatString = "unknown";
        }
    }
    else
    {
        // We are a default raster.
        // Share functionality here.
        getDefaultRasterFormatString( nativeTexture->rasterFormat, nativeTexture->depth, nativeTexture->paletteType, nativeTexture->colorOrdering, formatString );
    }

    if ( buf )
    {
        strncpy( buf, formatString.c_str(), bufLen );
    }

    lengthOut += formatString.length();
}

}