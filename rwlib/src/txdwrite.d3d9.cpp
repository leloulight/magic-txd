#include <StdInc.h>

#include "pixelformat.hxx"

#include "txdread.d3d9.hxx"

#include "streamutil.hxx"

#include "txdread.common.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility d3d9NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
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

            if ( platformDescriptor == PLATFORM_D3D9 )
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

void d3d9NativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)nativeTex;

    // Make sure the texture has some qualities before it can even be written.
    ePaletteType paletteType = platformTex->paletteType;

    uint32 compressionType = platformTex->dxtCompression;

    // Luckily, we figured out the problem that textures existed with no valid representation in Direct3D 9.
    // This means that finally each texture has a valid D3DFORMAT field!

	// Struct
	{
		BlockProvider texNativeImageStruct( &outputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            d3d9::textureMetaHeaderStructGeneric metaHeader;
            metaHeader.platformDescriptor = PLATFORM_D3D9;

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
            size_t mipmapCount = platformTex->mipmaps.size();

            metaHeader.rasterFormat = generateRasterFormatFlags( platformTex->rasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps );

		    metaHeader.d3dFormat = platformTex->d3dFormat;
            metaHeader.width = platformTex->mipmaps[ 0 ].layerWidth;
            metaHeader.height = platformTex->mipmaps[ 0 ].layerHeight;
            metaHeader.depth = platformTex->depth;
            metaHeader.mipmapCount = (uint8)mipmapCount;
            metaHeader.rasterType = platformTex->rasterType;
            metaHeader.pad1 = 0;

            metaHeader.hasAlpha = platformTex->hasAlpha;
            metaHeader.isCubeTexture = platformTex->isCubeTexture;
            metaHeader.autoMipMaps = platformTex->autoMipmaps;
            metaHeader.isNotRwCompatible = ( platformTex->isOriginalRWCompatible == false );   // no valid link to RW original types.
            metaHeader.pad2 = 0;

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
                const NativeTextureD3D9::mipmapLayer& mipLayer = platformTex->mipmaps[ i ];

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

// Pixel movement functions.
void d3d9NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native texture type.
    NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)objMem;

    // The pixel capabilities system has been mainly designed around PC texture optimization.
    // This means that we should be able to directly copy the Direct3D surface data into pixelsOut.
    // If not, we need to adjust, make a new library version.

    // We need to decide how to traverse palette runtime optimization data.

    // Determine the compression type.
    eCompressionType rwCompressionType = getD3DCompressionType( platformTex );

    // We always have a D3DFORMAT. If we have no link to original RW types, then we cannot be pushed to RW.
    // An exception case is the compression.
    d3dpublic::nativeTextureFormatHandler *useFormatHandler = NULL;

    bool isNewlyAllocated = false;

    eRasterFormat dstRasterFormat = platformTex->rasterFormat;
    uint32 dstDepth = platformTex->depth;
    eColorOrdering dstColorOrder = platformTex->colorOrdering;
    ePaletteType dstPaletteType = platformTex->paletteType;
    void *dstPaletteData = platformTex->palette;
    uint32 dstPaletteSize = platformTex->paletteSize;

    if ( platformTex->d3dRasterFormatLink == false && rwCompressionType == RWCOMPRESS_NONE )
    {
        // We could have a native format handler that converts us into an appropriate format.
        // This would be amazing!
        useFormatHandler = platformTex->anonymousFormatLink;

        if ( useFormatHandler )
        {
            // We kinda travel in another format.
            useFormatHandler->GetTextureRWFormat( dstRasterFormat, dstDepth, dstColorOrder );

            // No more palette.
            dstPaletteType = PALETTE_NONE;
            dstPaletteData = NULL;
            dstPaletteSize = 0;

            // We always newly allocate if there is a native format plugin.
            // For simplicity sake.
            isNewlyAllocated = true;
        }
        else
        {
            throw RwException( "cannot fetch pixels from Direct3D 9 raster as it has no representation in RenderWare" );
        }
    }

    // Put ourselves into the pixelsOut struct!
    pixelsOut.rasterFormat = dstRasterFormat;
    pixelsOut.depth = dstDepth;
    pixelsOut.rowAlignment = getD3DTextureDataRowAlignment();
    pixelsOut.colorOrder = dstColorOrder;
    pixelsOut.paletteType = dstPaletteType;
    pixelsOut.paletteData = dstPaletteData;
    pixelsOut.paletteSize = dstPaletteSize;
    pixelsOut.compressionType = rwCompressionType;
    pixelsOut.hasAlpha = platformTex->hasAlpha;

    pixelsOut.autoMipmaps = platformTex->autoMipmaps;
    pixelsOut.cubeTexture = platformTex->isCubeTexture;
    pixelsOut.rasterType = platformTex->rasterType;

    // Now, the texels.
    size_t mipmapCount = platformTex->mipmaps.size();

    pixelsOut.mipmaps.resize( mipmapCount );

    try
    {
        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const NativeTextureD3D9::mipmapLayer& srcLayer = platformTex->mipmaps[ n ];

            pixelDataTraversal::mipmapResource newLayer;

            uint32 mipWidth = srcLayer.width;
            uint32 mipHeight = srcLayer.height;
            uint32 layerWidth = srcLayer.layerWidth;
            uint32 layerHeight = srcLayer.layerHeight;

            void *srcTexels = srcLayer.texels;
            uint32 texelDataSize = srcLayer.dataSize;

            if ( useFormatHandler != NULL )
            {
                // We will always convert to native RW original types, which are raw pixels.
                // This means that layer dimensions match plane dimensions.
                mipWidth = layerWidth;
                mipHeight = layerHeight;

                // Create a new storage pointer.
                uint32 dstRowSize = getD3DRasterDataRowSize( mipWidth, dstDepth );

                texelDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

                void *newtexels = engineInterface->PixelAllocate( texelDataSize );

                try
                {
                    // Ask the format handler to convert it to something useful.
                    useFormatHandler->ConvertToRW(
                        srcTexels, mipWidth, mipHeight, dstRowSize, texelDataSize,
                        newtexels
                    );
                }
                catch( ... )
                {
                    engineInterface->PixelFree( newtexels );

                    throw;
                }

                srcTexels = newtexels;
            }
        
            newLayer.width = mipWidth;
            newLayer.height = mipHeight;
            newLayer.mipWidth = layerWidth;
            newLayer.mipHeight = layerHeight;

            newLayer.texels = srcTexels;
            newLayer.dataSize = texelDataSize;

            // Put this layer.
            pixelsOut.mipmaps[ n ] = newLayer;
        }
    }
    catch( ... )
    {
        pixelsOut.FreePixels( engineInterface );

        throw;
    }

    // We never allocate new texels, actually.
    // Well, we do if we own a complicated D3DFORMAT.
    pixelsOut.isNewlyAllocated = isNewlyAllocated;
}

inline void convertCompatibleRasterFormat(
    eRasterFormat& rasterFormat, eColorOrdering& colorOrder, uint32& depth, ePaletteType& paletteType, D3DFORMAT& d3dFormatOut
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

            // Make sure we reorder the palette.
            paletteType = PALETTE_4BIT;
        }
        else
        {
            assert( 0 );
        }

        // All palettes have RGBA color order.
        colorOrder = COLOR_RGBA;

        d3dFormatOut = D3DFMT_P8;
    }
    else
    {
        if ( srcRasterFormat == RASTER_1555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_A1R5G5B5;
        }
        else if ( srcRasterFormat == RASTER_565 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_R5G6B5;
        }
        else if ( srcRasterFormat == RASTER_4444 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_A4R4G4B4;
        }
        else if ( srcRasterFormat == RASTER_8888 )
        {
            depth = 32;

            // Can be both RGBA and BGRA.
            if ( srcColorOrder == COLOR_RGBA )
            {
                d3dFormatOut = D3DFMT_A8B8G8R8;
            }
            else if ( srcColorOrder == COLOR_BGRA )
            {
                d3dFormatOut = D3DFMT_A8R8G8B8;
            }
            else
            {
                // We kinda force a color reordering here.
                colorOrder = COLOR_BGRA;
                d3dFormatOut = D3DFMT_A8R8G8B8;
            }
        }
        else if ( srcRasterFormat == RASTER_888 )
        {
            if ( srcColorOrder == COLOR_BGRA )
            {
                if ( srcDepth == 24 )
                {
                    d3dFormatOut = D3DFMT_R8G8B8;
                }
                else if ( srcDepth == 32 )
                {
                    d3dFormatOut = D3DFMT_X8R8G8B8;
                }
                else
                {
                    // We force to adjust the depth.
                    depth = 32;
                    d3dFormatOut = D3DFMT_X8R8G8B8;
                }
            }
            else if ( srcColorOrder == COLOR_RGBA )
            {
                depth = 32;
                d3dFormatOut = D3DFMT_X8B8G8R8;
            }
            else
            {
                // We force expanding to standard format.
                depth = 32;
                colorOrder = COLOR_BGRA;
                d3dFormatOut = D3DFMT_X8R8G8B8;
            }
        }
        else if ( srcRasterFormat == RASTER_555 )
        {
            depth = 16;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_X1R5G5B5;
        }
        else if ( srcRasterFormat == RASTER_LUM )
        {
            // We only support 8bit LUM, obviously.
            depth = 8;
            d3dFormatOut = D3DFMT_L8;
        }
        // NEW formats :3
        else if ( srcRasterFormat == RASTER_LUM_ALPHA )
        {
            if ( srcDepth == 8 )
            {
                d3dFormatOut = D3DFMT_A4L4;
            }
            else if ( srcDepth == 16 )
            {
                d3dFormatOut = D3DFMT_A8L8;
            }
            else
            {
                // Be safe and convert to maximum quality.
                rasterFormat = RASTER_8888;
                depth = 32;
                colorOrder = COLOR_BGRA;
                d3dFormatOut = D3DFMT_A8R8G8B8;
            }
        }
        else
        {
            // Any unknown raster formats need conversion to full quality.
            rasterFormat = RASTER_8888;
            depth = 32;
            colorOrder = COLOR_BGRA;
            d3dFormatOut = D3DFMT_A8R8G8B8;
        }
    }
}

void d3d9NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // We want to remove our current texels and put the new ones into us instead.
    // By chance, the runtime makes sure the texture is already empty.
    // So optimize your routine to that.

    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    // We want to simply assign stuff to this native texture.

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

    D3DFORMAT d3dTargetFormat = D3DFMT_UNKNOWN;

    // Check whether we can directly acquire or have to allocate a new copy.
    bool canDirectlyAcquire;

    bool hasRasterFormatLink = false;

    bool hasOriginalRWCompat = false;

    if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // Actually, before we can acquire texels, we MUST make sure they are in
        // a compatible format. If they are not, then we will most likely allocate
        // new pixel information, instead in a compatible format. The same has to be
        // made for the XBOX implementation.

        // Make sure this texture is writable.
        // If we are on D3D, we have to avoid typical configurations that may come from
        // other hardware.
        convertCompatibleRasterFormat(
            dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType, d3dTargetFormat
        );

        dxtType = 0;

        canDirectlyAcquire =
            !doesPixelDataNeedConversion(
                pixelsIn,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType
            );

        // If we are uncompressed, then we know how to deal with the texture data.
        // Hence we have a d3dRasterFormatLink!
        hasRasterFormatLink = true;

        // We could have original RW compatibility here.
        // This is of course only possible if we are not compressed/are a raw raster.
        hasOriginalRWCompat =
            isRasterFormatOriginalRWCompatible(
                dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType
            );
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT1 )
    {
        // TODO: do we properly handle DXT1 with alpha...?

        d3dTargetFormat = D3DFMT_DXT1;

        dxtType = 1;

        dstRasterFormat = ( hasAlpha ) ? ( RASTER_1555 ) : ( RASTER_565 );

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT2 )
    {
        d3dTargetFormat = D3DFMT_DXT2;

        dxtType = 2;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT3 )
    {
        d3dTargetFormat = D3DFMT_DXT3;

        dxtType = 3;

        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT4 )
    {
        d3dTargetFormat = D3DFMT_DXT4;

        dxtType = 4;
        
        dstRasterFormat = RASTER_4444;

        canDirectlyAcquire = true;
    }
    else if ( rwCompressionType == RWCOMPRESS_DXT5 )
    {
        d3dTargetFormat = D3DFMT_DXT5;

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

    size_t mipmapCount = pixelsIn.mipmaps.size();

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
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType,
                true,
                dstTexels, dstDataSize
            );
        }

        NativeTextureD3D9::mipmapLayer newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;
        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        nativeTex->mipmaps[ n ] = newLayer;
    }

    // We need to set the Direct3D9 format field.
    nativeTex->d3dFormat = d3dTargetFormat;

    // Store whether we can directly deal with the data we just got.
    nativeTex->d3dRasterFormatLink = hasRasterFormatLink;

    // Store whether the pixels have a meaning in RW3 terms.
    nativeTex->isOriginalRWCompatible = hasOriginalRWCompat;

    // We cannot create a texture using an anonymous raster link this way.
    nativeTex->anonymousFormatLink = NULL;

    // For now, we can always directly acquire pixels.
    feedbackOut.hasDirectlyAcquired = canDirectlyAcquire;
}

void d3d9NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    // Just remove the pixels from us.
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

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
    nativeTex->d3dFormat = D3DFMT_UNKNOWN;
    nativeTex->dxtCompression = 0;
    nativeTex->hasAlpha = false;
    nativeTex->colorOrdering = COLOR_BGRA;
}

struct d3d9MipmapManager
{
    NativeTextureD3D9 *nativeTex;

    inline d3d9MipmapManager( NativeTextureD3D9 *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // If we are a texture that does not directly represent rw compatible types, 
        // we need to have a native format plugin that maps us to original RW types.
        bool isNewlyAllocated = false;

        if ( nativeTex->IsRWCompatible() == false )
        {
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTex->anonymousFormatLink;

            if ( !formatHandler )
            {
                throw RwException( "cannot fetch mipmap layer from Direct3D 9 native texture since it has an unknown D3DFORMAT" );
            }

            // Do stuff.
            uint32 mipWidth = mipLayer.width;
            uint32 mipHeight = mipLayer.height;

            eRasterFormat rasterFormat;
            uint32 depth;
            eColorOrdering colorOrder;

            formatHandler->GetTextureRWFormat( rasterFormat, depth, colorOrder );

            // Calculate the data size.
            uint32 dstRowSize = getD3DRasterDataRowSize( mipWidth, depth );

            uint32 texDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

            // Allocate new texels.
            void *newtexels = engineInterface->PixelAllocate( texDataSize );
            
            try
            {
                formatHandler->ConvertToRW(
                    mipLayer.texels, mipWidth, mipHeight, dstRowSize, mipLayer.dataSize,
                    newtexels
                );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            // Now return that new stuff.
            widthOut = mipWidth;
            heightOut = mipHeight;

            // Since we are returning a raw raster, plane dimm == layer dimm.
            layerWidthOut = mipWidth;
            layerHeightOut = mipHeight;

            dstRasterFormat = rasterFormat;
            dstColorOrder = colorOrder;
            dstDepth = depth;

            dstPaletteType = PALETTE_NONE;
            dstPaletteData = NULL;
            dstPaletteSize = 0;

            dstCompressionType = RWCOMPRESS_NONE;

            dstTexelsOut = newtexels;
            dstDataSizeOut = texDataSize;

            isNewlyAllocated = true;
        }
        else
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

            dstTexelsOut = mipLayer.texels;
            dstDataSizeOut = mipLayer.dataSize;
        }

        dstRowAlignment = getD3DTextureDataRowAlignment();

        hasAlpha = nativeTex->hasAlpha;

        isNewlyAllocatedOut = isNewlyAllocated;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D9::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        if ( nativeTex->IsRWCompatible() == false )
        {
            // If we have a native format plugin, we can ask it to create the encoded pixel data
            // by feeding it orignal RW type data.
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTex->anonymousFormatLink;

            if ( !formatHandler )
            {
                throw RwException( "cannot add mipmap layers to Direct3D 9 native texture as it has an unknown D3DFORMAT" );
            }

            // Calculate the row stride that is required to iterate through the RW buffer rows.
            uint32 srcRowSize = getRasterDataRowSize( width, depth, rowAlignment );

            // We create an encoding that is expected to be raw data.
            uint32 texDataSize = (uint32)formatHandler->GetFormatTextureDataSize( width, height );

            void *newtexels = engineInterface->PixelAllocate( texDataSize );

            try
            {
                // Request the plugin to create data for us.
                formatHandler->ConvertFromRW(
                    width, height, srcRowSize,
                    srcTexels, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
                    newtexels
                );
            }
            catch( ... )
            {
                engineInterface->PixelFree( newtexels );

                throw;
            }

            // Write stuff.
            mipLayer.width = width;
            mipLayer.height = height;
            
            mipLayer.layerWidth = layerWidth;
            mipLayer.layerHeight = layerHeight;

            mipLayer.texels = newtexels;
            mipLayer.dataSize = texDataSize;

            // We always newly allocate to store pixels here, because we want to
            // keep plugin architecture as simple as possible.
            hasDirectlyAcquiredOut = false;
        }
        else
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
    }
};

bool d3d9NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    d3d9MipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureD3D9::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool d3d9NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    d3d9MipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureD3D9::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void d3d9NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    virtualClearMipmaps <NativeTextureD3D9::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void d3d9NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    size_t mipmapCount = nativeTex->mipmaps.size();

    infoOut.mipmapCount = (uint32)mipmapCount;

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

void d3d9NativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    NativeTextureD3D9 *nativeTexture = (NativeTextureD3D9*)objMem;

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
        // Check whether we have a link to original RW types.
        // Otherwise we should have a native format extension, hopefully.
        if ( nativeTexture->d3dRasterFormatLink == false )
        {
            // Just get the string from the native format.
            d3dpublic::nativeTextureFormatHandler *formatHandler = nativeTexture->anonymousFormatLink;

            if ( formatHandler )
            {
                const char *formatName = formatHandler->GetFormatName();

                if ( formatName )
                {
                    formatString = formatName;
                }
                else
                {
                    formatString = "unspecified";
                }
            }
            else
            {
                formatString = "undefined";
            }
        }
        else
        {
            // We are a default raster.
            // Share functionality here.
            getDefaultRasterFormatString( nativeTexture->rasterFormat, nativeTexture->depth, nativeTexture->paletteType, nativeTexture->colorOrdering, formatString );
        }
    }

    if ( buf )
    {
        strncpy( buf, formatString.c_str(), bufLen );
    }

    lengthOut += formatString.length();
}

}