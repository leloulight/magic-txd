#include <StdInc.h>

#include "txdread.xbox.hxx"

#include "txdread.d3d.hxx"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

#include "txdread.common.hxx"

#include "pluginutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

void xboxNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the image data chunk.
    {
        BlockProvider texImageDataBlock( &inputProvider );

        texImageDataBlock.EnterContext();

        try
        {
            if ( texImageDataBlock.getBlockID() == CHUNK_STRUCT )
            {
	            uint32 platform = texImageDataBlock.readUInt32();

	            if (platform != PLATFORM_XBOX)
                {
                    throw RwException( "invalid platform flag for XBOX texture native" );
                }

                // Cast to our native texture format.
                NativeTextureXBOX *platformTex = (NativeTextureXBOX*)nativeTex;

                int engineWarningLevel = engineInterface->GetWarningLevel();

                bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

                // Read the main texture header.
                textureMetaHeaderStructXbox metaInfo;
                texImageDataBlock.read( &metaInfo, sizeof(metaInfo) );

                // Read the texture names.
                {
                    char tmpbuf[ sizeof( metaInfo.name ) + 1 ];

                    // Make sure the name buffer is zero terminted.
                    tmpbuf[ sizeof( metaInfo.name ) ] = '\0';

                    // Move over the texture name.
                    memcpy( tmpbuf, metaInfo.name, sizeof( metaInfo.name ) );

                    theTexture->SetName( tmpbuf );

                    // Move over the texture mask name.
                    memcpy( tmpbuf, metaInfo.maskName, sizeof( metaInfo.maskName ) );

                    theTexture->SetMaskName( tmpbuf );
                }

                // Read format info.
                metaInfo.formatInfo.parse( *theTexture );

                // Deconstruct the rasterFormat flags.
                bool hasMipmaps = false;        // TODO: actually use this flag.
                bool autoMipmaps = false;

                readRasterFormatFlags( metaInfo.rasterFormat, platformTex->rasterFormat, platformTex->paletteType, hasMipmaps, autoMipmaps );

                platformTex->hasAlpha = ( metaInfo.hasAlpha != 0 );

                uint32 depth = metaInfo.depth;
                uint32 maybeMipmapCount = metaInfo.mipmapCount;

                platformTex->depth = depth;

                platformTex->rasterType = metaInfo.rasterType;

                platformTex->dxtCompression = metaInfo.dxtCompression;

                platformTex->autoMipmaps = autoMipmaps;

                ePaletteType paletteType = platformTex->paletteType;

                // XBOX texture are always BGRA.
                platformTex->colorOrder = COLOR_BGRA;

                // Verify the parameters.
                eRasterFormat rasterFormat = platformTex->rasterFormat;

                // - depth
                {
                    bool isValidDepth = true;

                    if (paletteType == PALETTE_4BIT)
                    {
                        if (depth != 4 && depth != 8)
                        {
                            isValidDepth = false;
                        }
                    }
                    else if (paletteType == PALETTE_8BIT)
                    {
                        if (depth != 8)
                        {
                            isValidDepth = false;
                        }
                    }
                    // TODO: find more ways of verification here.

                    if (isValidDepth == false)
                    {
                        // We cannot repair a broken depth.
                        throw RwException( "texture " + theTexture->GetName() + " has an invalid depth" );
                    }
                }

                uint32 remainingImageSectionData = metaInfo.imageDataSectionSize;

                if (paletteType != PALETTE_NONE)
                {
                    uint32 palItemCount = getD3DPaletteCount(paletteType);

                    uint32 palDepth = Bitmap::getRasterFormatDepth( rasterFormat );

                    uint32 paletteDataSize = getRasterDataSize( palItemCount, palDepth );

                    // Do we have palette data in the stream?
                    texImageDataBlock.check_read_ahead( paletteDataSize );

	                void *palData = engineInterface->PixelAllocate( paletteDataSize );

                    try
                    {
	                    texImageDataBlock.read( palData, paletteDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( palData );

                        throw;
                    }

                    // Write the parameters.
                    platformTex->palette = palData;
	                platformTex->paletteSize = palItemCount;
                }

                // Read all the textures.
                uint32 actualMipmapCount = 0;

                mipGenLevelGenerator mipLevelGen( metaInfo.width, metaInfo.height );

                if ( !mipLevelGen.isValidLevel() )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has invalid dimensions" );
                }

                uint32 dxtCompression = platformTex->dxtCompression;

                bool hasZeroSizedMipmaps = false;

                for (uint32 i = 0; i < maybeMipmapCount; i++)
                {
                    if ( remainingImageSectionData == 0 )
                    {
                        break;
                    }

                    bool couldIncrementLevel = true;

	                if (i > 0)
                    {
                        couldIncrementLevel = mipLevelGen.incrementLevel();
                    }

                    if ( !couldIncrementLevel )
                    {
                        break;
                    }

                    // If any dimension is zero, ignore that mipmap.
                    if ( !mipLevelGen.isValidLevel() )
                    {
                        hasZeroSizedMipmaps = true;
                        break;
                    }

                    // Start a new mipmap layer.
                    NativeTextureXBOX::mipmapLayer newLayer;

                    newLayer.layerWidth = mipLevelGen.getLevelWidth();
                    newLayer.layerHeight = mipLevelGen.getLevelHeight();

                    // Process dimensions.
                    uint32 texWidth = newLayer.layerWidth;
                    uint32 texHeight = newLayer.layerHeight;
                    {
		                // DXT compression works on 4x4 blocks,
		                // align the dimensions.
		                if (dxtCompression != 0)
                        {
			                texWidth = ALIGN_SIZE( texWidth, 4u );
                            texHeight = ALIGN_SIZE( texHeight, 4u );
		                }
                    }

                    newLayer.width = texWidth;
                    newLayer.height = texHeight;

                    // Calculate the data size of this mipmap.
                    uint32 texUnitCount = ( texWidth * texHeight );
                    uint32 texDataSize = 0;

                    if (dxtCompression != 0)
                    {
                        uint32 dxtType = 0;

	                    if (dxtCompression == 0xC)	// DXT1 (?)
                        {
                            dxtType = 1;
                        }
                        else if (dxtCompression == 0xD)
                        {
                            dxtType = 2;
                        }
                        else if (dxtCompression == 0xE)
                        {
                            dxtType = 3;
                        }
                        else if (dxtCompression == 0xF)
                        {
                            dxtType = 4;
                        }
                        else if (dxtCompression == 0x10)
                        {
                            dxtType = 5;
                        }
                        else
                        {
                            throw RwException( "texture " + theTexture->GetName() + " has an unknown compression type" );
                        }
            	        
                        texDataSize = getDXTRasterDataSize(dxtType, texUnitCount);
                    }
                    else
                    {
                        // There should also be raw rasters supported.
                        texDataSize = getRasterDataSize(texUnitCount, depth);
                    }

                    if ( remainingImageSectionData < texDataSize )
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has an invalid image data section size parameter" );
                    }

                    // Decrement the overall remaining size.
                    remainingImageSectionData -= texDataSize;

                    // Store the texture size.
                    newLayer.dataSize = texDataSize;

                    // Do we have texel data in the stream?
                    texImageDataBlock.check_read_ahead( texDataSize );

                    // Fetch the texels.
                    newLayer.texels = engineInterface->PixelAllocate( texDataSize );

                    try
                    {
	                    texImageDataBlock.read( newLayer.texels, texDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( newLayer.texels );

                        throw;
                    }

                    // Store the layer.
                    platformTex->mipmaps.push_back( newLayer );

                    // Increase mipmap count.
                    actualMipmapCount++;
                }

                if ( actualMipmapCount == 0 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " is empty" );
                }

                if ( remainingImageSectionData != 0 )
                {
                    if ( engineWarningLevel >= 2 )
                    {
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " appears to have image section meta-data" );
                    }

                    // Skip those bytes.
                    texImageDataBlock.skip( remainingImageSectionData );
                }

                if ( hasZeroSizedMipmaps )
                {
                    if ( !engineIgnoreSecureWarnings )
                    {
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has zero sized mipmaps" );
                    }
                }

                // Fix filtering mode.
                fixFilteringMode( *theTexture, actualMipmapCount );

                // Fix the auto-mipmap flag.
                {
                    bool hasAutoMipmaps = platformTex->autoMipmaps;

                    if ( hasAutoMipmaps )
                    {
                        bool canHaveAutoMipmaps = ( actualMipmapCount == 1 );

                        if ( !canHaveAutoMipmaps )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid auto-mipmap flag (fixing)" );

                            platformTex->autoMipmaps = false;
                        }
                    }
                }
            }
            else
            {
                engineInterface->PushWarning( "could not find texture image data struct in XBOX texture native" );
            }
        }
        catch( ... )
        {
            texImageDataBlock.LeaveContext();

            throw;
        }

        texImageDataBlock.LeaveContext();
    }

    // Now the extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static PluginDependantStructRegister <xboxNativeTextureTypeProvider, RwInterfaceFactory_t> xboxNativeTexturePlugin( engineFactory );

void registerXBOXNativeTexture( Interface *engineInterface )
{
    xboxNativeTextureTypeProvider *xboxTexEnv = xboxNativeTexturePlugin.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( xboxTexEnv )
    {
        RegisterNativeTextureType( engineInterface, "XBOX", xboxTexEnv, sizeof( NativeTextureXBOX ) );
    }
}

inline void copyPaletteData(
    Interface *engineInterface,
    ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eRasterFormat srcRasterFormat,
    bool isNewlyAllocated,
    ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize
)
{
    dstPaletteType = srcPaletteType;

    if ( dstPaletteType != PALETTE_NONE )
    {
        dstPaletteSize = srcPaletteSize;

        if ( isNewlyAllocated )
        {
            // Create a new copy.
            uint32 palRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );

            uint32 palDataSize = getRasterDataSize( dstPaletteSize, palRasterDepth );

            dstPaletteData = engineInterface->PixelAllocate( palDataSize );

            memcpy( dstPaletteData, srcPaletteData, palDataSize );
        }
        else
        {
            // Just move it over.
            dstPaletteData = srcPaletteData;
        }
    }
}

inline eCompressionType getXBOXCompressionType( const NativeTextureXBOX *nativeTex )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    uint32 xboxCompressionType = nativeTex->dxtCompression;

    if ( xboxCompressionType != 0 )
    {
        if ( xboxCompressionType == 0xc )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( xboxCompressionType == 0xd )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( xboxCompressionType == 0xe )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( xboxCompressionType == 0xf )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else if ( xboxCompressionType == 0x10 )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else
        {
            assert( 0 );
        }
    }

    return rwCompressionType;
}

inline bool isXBOXTextureSwizzled( const NativeTextureXBOX *nativeTex )
{
    return ( nativeTex->dxtCompression == 0 );
}

void xboxNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native format.
    NativeTextureXBOX *platformTex = (NativeTextureXBOX*)objMem;

    // We need to find out how to store the texels into the traversal containers.
    // If we store compressed image data, we can give the data directly to the runtime.
    eCompressionType rwCompressionType = getXBOXCompressionType( platformTex );

    uint32 srcDepth = platformTex->depth;

    eRasterFormat dstRasterFormat = platformTex->rasterFormat;
    uint32 dstDepth = platformTex->depth;

    bool hasAlpha = platformTex->hasAlpha;

    bool isSwizzledFormat = isXBOXTextureSwizzled( platformTex );

    bool canHavePalette = false;

    if ( rwCompressionType == RWCOMPRESS_DXT1 ||
         rwCompressionType == RWCOMPRESS_DXT2 ||
         rwCompressionType == RWCOMPRESS_DXT3 ||
         rwCompressionType == RWCOMPRESS_DXT4 ||
         rwCompressionType == RWCOMPRESS_DXT5 )
    {
        dstRasterFormat = getVirtualRasterFormat( hasAlpha, rwCompressionType );

        dstDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );
    }
    else if ( rwCompressionType == RWCOMPRESS_NONE )
    {
        // We can have a palette.
        canHavePalette = true;
    }
    else
    {
        throw RwException( "invalid compression type in XBOX texture native pixel fetch" );
    }

    // If we are swizzled, then we have to create a new copy of the texels which is in linear format.
    // Otherwise we can optimize.
    bool isNewlyAllocated = ( isSwizzledFormat == true );

    uint32 mipmapCount = platformTex->mipmaps.size();

    // Allocate virtual mipmaps.
    pixelsOut.mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureXBOX::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

        // Fetch all mipmap data onto the stack.
        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        void *dstTexels = NULL;
        uint32 dstDataSize = 0;

        if ( isSwizzledFormat == false )
        {
            // We can simply move things over.
            dstTexels = mipLayer.texels;
            dstDataSize = mipLayer.dataSize;
        }
        else
        {
            // Here we have to put the pixels into a linear format.
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = srcDepth;
            swizzleTrav.texels = mipLayer.texels;
            swizzleTrav.dataSize = mipLayer.dataSize;

            NativeTextureXBOX::unswizzleMipmap( engineInterface, swizzleTrav );

            assert( swizzleTrav.newtexels != swizzleTrav.texels );

            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;
            dstTexels = swizzleTrav.newtexels;
            dstDataSize = swizzleTrav.newDataSize;
        }

        // Create a new virtual mipmap layer.
        pixelDataTraversal::mipmapResource newLayer;

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;

        newLayer.mipWidth = layerWidth;
        newLayer.mipHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store this layer.
        pixelsOut.mipmaps[ n ] = newLayer;
    }

    // If we can have a palette, copy it over.
    ePaletteType dstPaletteType = PALETTE_NONE;
    void *dstPaletteData = NULL;
    uint32 dstPaletteSize = 0;

    if ( canHavePalette )
    {
        copyPaletteData(
            engineInterface,
            platformTex->paletteType, platformTex->palette, platformTex->paletteSize,
            dstRasterFormat, isNewlyAllocated,
            dstPaletteType, dstPaletteData, dstPaletteSize
        );
    }

    // Copy over general raster data.
    pixelsOut.rasterFormat = dstRasterFormat;
    pixelsOut.depth = dstDepth;
    pixelsOut.colorOrder = platformTex->colorOrder;

    pixelsOut.compressionType = rwCompressionType;

    // Give it the palette data.
    pixelsOut.paletteData = dstPaletteData;
    pixelsOut.paletteSize = dstPaletteSize;
    pixelsOut.paletteType = dstPaletteType;

    // Copy over more advanced parameters.
    pixelsOut.hasAlpha = hasAlpha;
    pixelsOut.autoMipmaps = platformTex->autoMipmaps;
    pixelsOut.cubeTexture = false;  // XBOX never has cube textures.
    pixelsOut.rasterType = platformTex->rasterType;

    // Tell the runtime whether we newly allocated those texels.
    pixelsOut.isNewlyAllocated = isNewlyAllocated;
}

void xboxNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // Cast to our native texture container.
    NativeTextureXBOX *xboxTex = (NativeTextureXBOX*)objMem;

    // Clear any previous image data.
    //xboxTex->clearTexelData();

    // We need to make sure that the raster data is compatible.
    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;
    ePaletteType srcPaletteType = pixelsIn.paletteType;
    void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = pixelsIn.depth;
    eColorOrdering dstColorOrder = srcColorOrder;

    // Move over the texture data.
    eCompressionType rwCompressionType = pixelsIn.compressionType;

    uint32 xboxCompressionType = 0;

    bool requiresSwizzling = false;

    bool canHavePaletteData = false;

    bool canBeConverted = false;

    if ( rwCompressionType != RWCOMPRESS_NONE )
    {
        if ( rwCompressionType == RWCOMPRESS_DXT1 )
        {
            xboxCompressionType = 0xC;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT2 )
        {
            xboxCompressionType = 0xD;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT3 )
        {
            xboxCompressionType = 0xE;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT4 )
        {
            xboxCompressionType = 0xF;
        }
        else if ( rwCompressionType == RWCOMPRESS_DXT5 )
        {
            xboxCompressionType = 0x10;
        }
        else
        {
            assert( 0 );
        }

        // Compressed rasters are what they are.
        // They do not need swizzling.
        requiresSwizzling = false;

        canHavePaletteData = false;
    }
    else
    {
        // We need to verify that we have good raster properties.
        // Luckily, we only have to do this in this routine.
        if ( srcPaletteType != PALETTE_NONE )
        {
            if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
            {
                dstDepth = 8;
            }
            else
            {
                assert( 0 );
            }
        }
        else
        {
            if ( srcRasterFormat == RASTER_1555 )
            {
                dstDepth = 16;
            }
            else if ( srcRasterFormat == RASTER_565 )
            {
                dstDepth = 16;
            }
            else if ( srcRasterFormat == RASTER_4444 )
            {
                dstDepth = 16;
            }
            else if ( srcRasterFormat == RASTER_8888 )
            {
                dstDepth = 32;
            }
            else if ( srcRasterFormat == RASTER_888 )
            {
                dstDepth = 32;
            }
            else if ( srcRasterFormat == RASTER_555 )
            {
                dstDepth = 16;
            }
        }

        // XBOX textures always have BGRA color order, no matter if palette.
        dstColorOrder = COLOR_BGRA;

        canBeConverted = true;

        // Raw bitmap rasters are always swizzled.
        requiresSwizzling = true;

        canHavePaletteData = true;
    }

    // Check whether we need conversion.
    bool requiresConversion = false;

    if ( canBeConverted )
    {
        if ( srcRasterFormat != dstRasterFormat || srcDepth != dstDepth || srcColorOrder != dstColorOrder )
        {
            // If any format properity is different, we need to convert.
            requiresConversion = true;
        }
    }

    // Store texel data.
    // If we require swizzling, we cannot directly acquire the data.
    bool hasDirectlyAcquired = ( requiresSwizzling == false && requiresConversion == false );

    uint32 mipmapCount = pixelsIn.mipmaps.size();

    uint32 depth = pixelsIn.depth;

    // Allocate the mipmaps on the XBOX texture.
    xboxTex->mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const pixelDataTraversal::mipmapResource& srcLayer = pixelsIn.mipmaps[ n ];

        // Grab the pixel information onto the stack.
        uint32 mipWidth = srcLayer.width;
        uint32 mipHeight = srcLayer.height;

        uint32 layerWidth = srcLayer.mipWidth;
        uint32 layerHeight = srcLayer.mipHeight;

        // The destination texels.
        void *dstTexels = srcLayer.texels;
        uint32 dstDataSize = srcLayer.dataSize;

        bool hasNewTexels = false;

        // It could need conversion to another format.
        if ( requiresConversion )
        {
            // Call the helper function.
            ConvertMipmapLayer(
                engineInterface,
                srcLayer,
                srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstRasterFormat, dstDepth, dstColorOrder, srcPaletteType,
                true,
                dstTexels, dstDataSize
            );

            hasNewTexels = true;
        }

        // If we require swizzling, then we need to allocate a new copy of the texels
        // as destination buffer.
        if ( requiresSwizzling )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = depth;
            swizzleTrav.texels = dstTexels;
            swizzleTrav.dataSize = dstDataSize;

            NativeTextureXBOX::unswizzleMipmap( engineInterface, swizzleTrav );

            assert( swizzleTrav.texels != swizzleTrav.newtexels );

            // If we had newly allocated texels, free them.
            if ( hasNewTexels == true )
            {
                engineInterface->PixelFree( dstTexels );
            }

            // Update with the swizzled parameters.
            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;
            dstTexels = swizzleTrav.newtexels;
            dstDataSize = swizzleTrav.newDataSize;

            hasNewTexels = true;
        }

        // Store it as mipmap.
        NativeTextureXBOX::mipmapLayer& newLayer = xboxTex->mipmaps[ n ];

        newLayer.width = mipWidth;
        newLayer.height = mipHeight;

        newLayer.layerWidth = layerWidth;
        newLayer.layerHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;
    }

    // Also copy over the palette data.
    ePaletteType dstPaletteType = PALETTE_NONE;
    void *dstPaletteData = NULL;
    uint32 dstPaletteSize = 0;

    if ( canHavePaletteData )
    {
        dstPaletteType = srcPaletteType;

        if ( dstPaletteType != PALETTE_NONE )
        {
            dstPaletteSize = srcPaletteSize;

            if ( requiresConversion || hasDirectlyAcquired == false )
            {
                // Create a new copy.
                uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

                uint32 palDataSize = getRasterDataSize( dstPaletteSize, dstPalRasterDepth );

                dstPaletteData = engineInterface->PixelAllocate( palDataSize );

                if ( requiresConversion )
                {
                    uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );

                    ConvertPaletteData(
                        srcPaletteData, dstPaletteData,
                        srcPaletteSize, dstPaletteSize,
                        srcRasterFormat, srcColorOrder, srcPalRasterDepth,
                        dstRasterFormat, dstColorOrder, dstPalRasterDepth
                    );
                }
                else
                {
                    memcpy( dstPaletteData, srcPaletteData, palDataSize );
                }
            }
            else
            {
                // Just move it over.
                dstPaletteData = srcPaletteData;
            }
        }
    }

    // Copy general raster information.
    xboxTex->rasterFormat = dstRasterFormat;
    xboxTex->depth = dstDepth;
    xboxTex->colorOrder = dstColorOrder;

    // Store the palette.
    xboxTex->paletteType = dstPaletteType;
    xboxTex->palette = dstPaletteData;
    xboxTex->paletteSize = dstPaletteSize;

    // Copy more advanced properties.
    xboxTex->rasterType = pixelsIn.rasterType;
    xboxTex->hasAlpha = pixelsIn.hasAlpha;

    // Properly set the automipmaps field.
    bool autoMipmaps = pixelsIn.autoMipmaps;

    if ( mipmapCount > 1 )
    {
        autoMipmaps = false;
    }

    xboxTex->autoMipmaps = autoMipmaps;

    xboxTex->dxtCompression = xboxCompressionType;

    // Since we have to swizzle the pixels, we cannot directly acquire the texels.
    // If the input was compressed though, we can directly take the pixels.
    feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
}

void xboxNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    if ( deallocate )
    {
        // We simply deallocate everything.
        if ( void *palette = nativeTex->palette )
        {
            engineInterface->PixelFree( palette );
        }

        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );
    }

    // Clear all connections.
    nativeTex->palette = NULL;
    nativeTex->paletteSize = 0;
    nativeTex->paletteType = PALETTE_NONE;

    nativeTex->mipmaps.clear();

    // Reset raster parameters for debugging purposes.
    nativeTex->rasterFormat = RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->colorOrder = COLOR_BGRA;
    nativeTex->hasAlpha = false;
    nativeTex->autoMipmaps = false;
    nativeTex->dxtCompression = 0;
}

struct xboxMipmapManager
{
    NativeTextureXBOX *nativeTex;

    inline xboxMipmapManager( NativeTextureXBOX *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        void *texels = mipLayer.texels;
        uint32 dataSize = mipLayer.dataSize;

        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        uint32 depth = nativeTex->depth;

        bool isNewlyAllocated = false;

        // Decide whether the texture is swizzled.
        bool isSwizzled = isXBOXTextureSwizzled( this->nativeTex );

        // If we are swizzled, we must unswizzle.
        if ( isSwizzled )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = mipWidth;
            swizzleTrav.mipHeight = mipHeight;
            swizzleTrav.depth = depth;
            swizzleTrav.texels = texels;
            swizzleTrav.dataSize = dataSize;

            NativeTextureXBOX::unswizzleMipmap( engineInterface, swizzleTrav );

            mipWidth = swizzleTrav.newWidth;
            mipHeight = swizzleTrav.newHeight;

            texels = swizzleTrav.newtexels;
            dataSize = swizzleTrav.newDataSize;

            isNewlyAllocated = true;
        }

        // Return stuff.
        widthOut = mipWidth;
        heightOut = mipHeight;

        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstRasterFormat = nativeTex->rasterFormat;
        dstColorOrder = nativeTex->colorOrder;
        dstDepth = depth;

        dstPaletteType = nativeTex->paletteType;
        dstPaletteData = nativeTex->palette;
        dstPaletteSize = nativeTex->paletteSize;

        dstCompressionType = getXBOXCompressionType( nativeTex );

        hasAlpha = nativeTex->hasAlpha;

        dstTexelsOut = texels;
        dstDataSizeOut = dataSize;

        // We may or may not need to swizzle.
        isNewlyAllocatedOut = isNewlyAllocated;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureXBOX::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Check whether the texture is swizzled.
        bool isSwizzled = isXBOXTextureSwizzled( nativeTex );

        bool hasNewlyAllocated = false;

        // Convert the texels into our appopriate format.
        {
            bool hasChanged = ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                nativeTex->rasterFormat, nativeTex->depth, nativeTex->colorOrder,
                nativeTex->paletteType, nativeTex->palette, nativeTex->paletteSize,
                getXBOXCompressionType( nativeTex ),
                false,
                width, height,
                srcTexels, dataSize
            );

            hasNewlyAllocated = hasChanged;
        }

        // If we are swizzled, we also must swizzle all input textures.
        if ( isSwizzled )
        {
            NativeTextureXBOX::swizzleMipmapTraversal swizzleTrav;

            swizzleTrav.mipWidth = width;
            swizzleTrav.mipHeight = height;
            swizzleTrav.texels = srcTexels;
            swizzleTrav.dataSize = dataSize;

            NativeTextureXBOX::swizzleMipmap( engineInterface, swizzleTrav );

            if ( hasNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );
            }

            width = swizzleTrav.newWidth;
            height = swizzleTrav.newHeight;
            srcTexels = swizzleTrav.newtexels;
            dataSize = swizzleTrav.newDataSize;

            hasNewlyAllocated = true;
        }

        // We have no more auto mipmaps.
        nativeTex->autoMipmaps = false;

        // Store the encoded mipmap data.
        mipLayer.width = width;
        mipLayer.height = height;
        mipLayer.layerWidth = layerWidth;
        mipLayer.layerHeight = layerHeight;
        mipLayer.texels = srcTexels;
        mipLayer.dataSize = dataSize;

        hasDirectlyAcquiredOut = ( hasNewlyAllocated == false );
    }
};

bool xboxNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    xboxMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureXBOX::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool xboxNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    xboxMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureXBOX::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void xboxNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

    virtualClearMipmaps <NativeTextureXBOX::mipmapLayer> (
        engineInterface, nativeTex->mipmaps
    );
}

void xboxNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

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

};