#include "StdInc.h"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

#include "txdread.palette.hxx"

namespace rw
{

inline bool decompressTexelsUsingDXT(
    Interface *engineInterface, uint32 dxtType, eDXTCompressionMethod dxtMethod,
    uint32 texWidth, uint32 texHeight,
    uint32 texLayerWidth, uint32 texLayerHeight,
    const void *srcTexels, eRasterFormat rawRasterFormat, eColorOrdering rawColorOrder, uint32 rawDepth,
    void*& dstTexelsOut, uint32& dstTexelsDataSizeOut
)
{
	uint32 x = 0, y = 0;

    // Allocate the new texel array.
    uint32 texelItemCount = ( texLayerWidth * texLayerHeight );

	uint32 dataSize = getRasterDataSize(texelItemCount, rawDepth);

	void *newtexels = engineInterface->PixelAllocate( dataSize );

    // Get the compressed block count.
    uint32 compressedBlockCount = ( texWidth * texHeight ) / 16;

    bool successfullyDecompressed = true;

	for (uint32 n = 0; n < compressedBlockCount; n++)
    {
        PixelFormat::pixeldata32bit colors[4][4];

        bool couldDecompressBlock = decompressDXTBlock(dxtMethod, srcTexels, n, dxtType, colors);

        if (couldDecompressBlock == false)
        {
            // If even one block fails to decompress, abort.
            successfullyDecompressed = false;
            break;
        }

        // Write the colors.
        for (uint32 y_block = 0; y_block < 4; y_block++)
        {
            for (uint32 x_block = 0; x_block < 4; x_block++)
            {
                uint32 target_x = ( x + x_block );
                uint32 target_y = ( y + y_block );

                if ( target_x < texLayerWidth && target_y < texLayerHeight )
                {
                    const PixelFormat::pixeldata32bit& srcColor = colors[ y_block ][ x_block ];

                    uint8 red       = srcColor.red;
                    uint8 green     = srcColor.green;
                    uint8 blue      = srcColor.blue;
                    uint8 alpha     = srcColor.alpha;

                    uint32 dstColorIndex = PixelFormat::coord2index(target_x, target_y, texLayerWidth);

                    puttexelcolor(newtexels, dstColorIndex, rawRasterFormat, rawColorOrder, rawDepth, red, green, blue, alpha);
                }
            }
        }

		x += 4;

		if (x >= texWidth)
        {
			y += 4;
			x = 0;
		}

        if (y >= texHeight)
        {
            break;
        }
	}

    if ( !successfullyDecompressed )
    {
        // Delete the new texel array again.
        engineInterface->PixelFree( newtexels );
    }
    else
    {
        // Give the data to the runtime.
        dstTexelsOut = newtexels;
        dstTexelsDataSizeOut = dataSize;
    }

    return successfullyDecompressed;
}

bool genericDecompressDXTNative( Interface *engineInterface, pixelDataTraversal& pixelData, uint32 dxtType )
{
    eDXTCompressionMethod dxtMethod = engineInterface->GetDXTRuntime();

    // We must have stand-alone pixel data.
    // Otherwise we could mess up pretty badly!
    assert( pixelData.isNewlyAllocated == true );

    // Decide which raster format to use.
    eRasterFormat targetRasterFormat = getDXTDecompressionRasterFormat( engineInterface, dxtType, pixelData.hasAlpha );

    uint32 rasterFormatDepth = Bitmap::getRasterFormatDepth(targetRasterFormat);

    eColorOrdering colorOrder = pixelData.colorOrder;

    bool conversionSuccessful = true;

    uint32 mipmapCount = pixelData.mipmaps.size();

	for (uint32 i = 0; i < mipmapCount; i++)
    {
        pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ i ];

        void *texelData = mipLayer.texels;

		uint32 x = 0, y = 0;

        // Allocate the new texel array.
        uint32 texLayerWidth = mipLayer.mipWidth;
        uint32 texLayerHeight = mipLayer.mipHeight;

		void *newtexels;
		uint32 dataSize;

        // Get the compressed block count.
        uint32 texWidth = mipLayer.width;
        uint32 texHeight = mipLayer.height;

        bool successfullyDecompressed =
            decompressTexelsUsingDXT(
                engineInterface, dxtType, dxtMethod,
                texWidth, texHeight,
                texLayerWidth, texLayerHeight,
                texelData, targetRasterFormat, colorOrder, rasterFormatDepth,
                newtexels, dataSize
            );

        // If even one mipmap fails to decompress, abort.
        if ( !successfullyDecompressed )
        {
            assert( i == 0 );

            conversionSuccessful = false;
            break;
        }

        // Replace the texel data.
		engineInterface->PixelFree( texelData );

		mipLayer.texels = newtexels;
		mipLayer.dataSize = dataSize;

        // Normalize the dimensions.
        mipLayer.width = texLayerWidth;
        mipLayer.height = texLayerHeight;
	}

    if (conversionSuccessful)
    {
        // Update the depth.
        pixelData.depth = rasterFormatDepth;

        // Update the raster format.
        pixelData.rasterFormat = targetRasterFormat;

        // We are not compressed anymore.
        pixelData.compressionType = RWCOMPRESS_NONE;
    }

    return conversionSuccessful;
}

void genericCompressDXTNative( Interface *engineInterface, pixelDataTraversal& pixelData, uint32 dxtType )
{
    // We must get data in raw format.
    if ( pixelData.compressionType != RWCOMPRESS_NONE )
    {
        throw RwException( "runtime fault: attempting to compress an already compressed texture" );
    }

    if (dxtType != 1 && dxtType != 3 && dxtType != 5)
    {
        throw RwException( "cannot compress Direct3D textures using unsupported DXTn format" );
    }

    // We must have stand-alone pixel data.
    // Otherwise we could mess up pretty badly!
    assert( pixelData.isNewlyAllocated == true );

    // Compress it now.
    uint32 mipmapCount = pixelData.mipmaps.size();

    uint32 itemDepth = pixelData.depth;

    eRasterFormat rasterFormat = pixelData.rasterFormat;
    ePaletteType paletteType = pixelData.paletteType;
    eColorOrdering colorOrder = pixelData.colorOrder;

    uint32 maxpalette = pixelData.paletteSize;
    void *paletteData = pixelData.paletteData;

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        void *texelSource = mipLayer.texels;

        void *dxtArray = NULL;
        size_t dxtDataSize = 0;

        // Create the new DXT array.
        uint32 realMipWidth, realMipHeight;

        compressTexelsUsingDXT(
            engineInterface,
            dxtType, texelSource, mipWidth, mipHeight,
            rasterFormat, paletteData, paletteType, maxpalette, colorOrder, itemDepth,
            dxtArray, dxtDataSize,
            realMipWidth, realMipHeight
        );

        // Delete the raw texels.
        engineInterface->PixelFree( texelSource );

        if ( mipWidth != realMipWidth )
        {
            mipLayer.width = realMipWidth;
        }

        if ( mipHeight != realMipHeight )
        {
            mipLayer.height = realMipHeight;
        }

        // Put in the new DXTn texels.
        mipLayer.texels = dxtArray;

        // Update fields.
        mipLayer.dataSize = dxtDataSize;
    }

    // We are finished compressing.
    // If we were palettized, unset that.
    if ( paletteType != PALETTE_NONE )
    {
        // Free the palette colors.
        engineInterface->PixelFree( paletteData );

        // Reset the fields.
        pixelData.paletteType = PALETTE_NONE;
        pixelData.paletteData = NULL;
        pixelData.paletteSize = 0;
    }

    // Set a virtual raster format.
    // This is what is done by the R* DXT output system.
    {
        uint32 newDepth = itemDepth;
        eRasterFormat virtualRasterFormat = RASTER_8888;

        if ( dxtType == 1 )
        {
            newDepth = 16;

            if ( pixelData.hasAlpha )
            {
                virtualRasterFormat = RASTER_1555;
            }
            else
            {
                virtualRasterFormat = RASTER_565;
            }
        }
        else if ( dxtType == 2 || dxtType == 3 || dxtType == 4 || dxtType == 5 )
        {
            newDepth = 16;

            virtualRasterFormat = RASTER_4444;
        }

        if ( rasterFormat != virtualRasterFormat )
        {
            pixelData.rasterFormat = virtualRasterFormat;
        }

        if ( newDepth != itemDepth )
        {
            pixelData.depth = newDepth;
        }
    }

    // We are now successfully compressed, so set the correct compression type.
    eCompressionType targetCompressionType = RWCOMPRESS_NONE;

    if ( dxtType == 1 )
    {
        targetCompressionType = RWCOMPRESS_DXT1;
    }
    else if ( dxtType == 2 )
    {
        targetCompressionType = RWCOMPRESS_DXT2;
    }
    else if ( dxtType == 3 )
    {
        targetCompressionType = RWCOMPRESS_DXT3;
    }
    else if ( dxtType == 4 )
    {
        targetCompressionType = RWCOMPRESS_DXT4;
    }
    else if ( dxtType == 5 )
    {
        targetCompressionType = RWCOMPRESS_DXT5;
    }
    else
    {
        throw RwException( "runtime fault: unknown compression type request in DXT compressor" );
    }

    pixelData.compressionType = targetCompressionType;
}

void ConvertPaletteDepth(
    const void *srcTexels, void *dstTexels,
    uint32 texUnitCount,
    ePaletteType paletteType, uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth
)
{
    // Copy palette indice.
    for ( uint32 n = 0; n < texUnitCount; n++ )
    {
        uint8 palIndex;

        // Fetch the index
        bool gotPaletteIndex = getpaletteindex(srcTexels, paletteType, paletteSize, srcDepth, n, palIndex);

        if ( !gotPaletteIndex )
        {
            palIndex = 0;
        }

        // Put the index.
        if ( dstDepth == 4 )
        {
            ( (PixelFormat::palette4bit*)dstTexels )->setvalue(n, palIndex);
        }
        else if ( dstDepth == 8 )
        {
            ( (PixelFormat::palette8bit*)dstTexels )->setvalue(n, palIndex);
        }
        else
        {
            assert( 0 );
        }
    }
}

void ConvertMipmapLayer(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder, ePaletteType dstPaletteType,
    bool forceAllocation,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // Grab the source parameters.
    void *srcTexels = mipLayer.texels;

    uint32 srcTexelsDataSize = mipLayer.dataSize;

    uint32 srcWidth = mipLayer.width;               // the dimensions will stay the same.
    uint32 srcHeight = mipLayer.height;
    
    uint32 srcItemCount = ( srcWidth * srcHeight );

    // Check whether we need to reallocate the texels.
    void *dstTexels = srcTexels;

    uint32 dstTexelsDataSize = srcTexelsDataSize;

    if ( forceAllocation || srcDepth != dstDepth )
    {
        dstTexelsDataSize = getRasterDataSize( srcItemCount, dstDepth );

        dstTexels = engineInterface->PixelAllocate( dstTexelsDataSize );
    }

    if ( dstPaletteType != PALETTE_NONE )
    {
        // Make sure we came from a palette.
        assert( srcPaletteType != PALETTE_NONE );

        // We only have work to do if the depth changed or the pointers to the arrays changed.
        if ( srcDepth != dstDepth || srcTexels != dstTexels )
        {
            ConvertPaletteDepth(
                srcTexels, dstTexels,
                srcItemCount,
                srcPaletteType, srcPaletteSize,
                srcDepth, dstDepth
            );
        }
    }
    else
    {
        // If we are not a palette, then we have to process colors.
        for ( uint32 n = 0; n < srcItemCount; n++ )
        {
            uint8 r, g, b, a;

            bool hasColor = browsetexelcolor(
                srcTexels, srcPaletteType, srcPaletteData, srcPaletteSize, n, srcRasterFormat, srcColorOrder, srcDepth,
                r, g, b, a
            );

            if ( !hasColor )
            {
                r = 0;
                g = 0;
                b = 0;
                a = 0;
            }

            // Just put the color inside.
            puttexelcolor(
                dstTexels, n, dstRasterFormat, dstColorOrder, dstDepth,
                r, g, b, a
            );
        }
    }
    
    // Give data to the runtime.
    dstTexelsOut = dstTexels;
    dstDataSizeOut = dstTexelsDataSize;
}

bool ConvertMipmapLayerNative(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    bool isMipLayerTexels = true;

    // Perform this like a pipeline with multiple stages.
    uint32 srcDXTType;
    uint32 dstDXTType;

    bool isSrcDXTType = IsDXTCompressionType( srcCompressionType, srcDXTType );
    bool isDstDXTType = IsDXTCompressionType( dstCompressionType, dstDXTType );

    bool requiresCompression = ( srcCompressionType != dstCompressionType );

    if ( requiresCompression )
    {
        if ( isSrcDXTType )
        {
            assert( srcPaletteType == PALETTE_NONE );

            // Decompress stuff.
            eDXTCompressionMethod dxtMethod = engineInterface->GetDXTRuntime();

            void *decompressedTexels;
            uint32 decompressedSize;

            bool success = decompressTexelsUsingDXT(
                engineInterface, srcDXTType, dxtMethod,
                mipWidth, mipHeight,
                layerWidth, layerHeight,
                srcTexels, dstRasterFormat, dstColorOrder, dstDepth,
                decompressedTexels, decompressedSize
            );

            assert( success == true );

            // Update with raw raster data.
            srcRasterFormat = dstRasterFormat;
            srcColorOrder = dstColorOrder;
            srcDepth = dstDepth;

            srcTexels = decompressedTexels;
            srcDataSize = decompressedSize;

            mipWidth = layerWidth;
            mipHeight = layerHeight;

            srcCompressionType = RWCOMPRESS_NONE;

            isMipLayerTexels = false;
        }
    }

    if ( srcCompressionType == RWCOMPRESS_NONE && dstCompressionType == RWCOMPRESS_NONE )
    {
        uint32 texUnitCount = ( mipWidth * mipHeight );

        void *newtexels = NULL;
        uint32 dstDataSize = 0;

        if ( dstPaletteType == PALETTE_NONE )
        {
            dstDataSize = getRasterDataSize( texUnitCount, dstDepth );

            newtexels = engineInterface->PixelAllocate( dstDataSize );

            // Do the conversion.
            for ( uint32 n = 0; n < texUnitCount; n++ )
            {
                uint8 r, g, b, a;

                bool gotColor = browsetexelcolor( srcTexels, srcPaletteType, srcPaletteData, srcPaletteSize, n, srcRasterFormat, srcColorOrder, srcDepth, r, g, b, a );

                if ( !gotColor )
                {
                    r = 0;
                    g = 0;
                    b = 0;
                    a = 0;
                }

                puttexelcolor( newtexels, n, dstRasterFormat, dstColorOrder, dstDepth, r, g, b, a );
            }
        }
        else if ( srcPaletteType != PALETTE_NONE )
        {
            if ( srcPaletteData == dstPaletteData )
            {
                // Fix the indice, if necessary.
                if ( srcDepth != dstDepth )
                {
                    dstDataSize = getRasterDataSize( texUnitCount, dstDepth );

                    newtexels = engineInterface->PixelAllocate( dstDataSize );

                    // Convert the depth.
                    ConvertPaletteDepth(
                        srcTexels, newtexels,
                        texUnitCount,
                        srcPaletteType, srcPaletteSize,
                        srcDepth, dstDepth
                    );
                }
            }
            else
            {
                // Remap texels.
                RemapMipmapLayer(
                    engineInterface,
                    dstRasterFormat, dstColorOrder,
                    srcTexels, texUnitCount,
                    srcRasterFormat, srcColorOrder, srcDepth, srcPaletteType, srcPaletteData, srcPaletteSize,
                    dstPaletteData, dstPaletteSize,
                    dstDepth, dstPaletteType,
                    newtexels, dstDataSize
                );
            }
        }
        else
        {
            // There must be a destination palette already allocated, as we cannot create one.
            assert( dstPaletteData != NULL );

            // Remap.
            RemapMipmapLayer(
                engineInterface,
                dstRasterFormat, dstColorOrder,
                srcTexels, texUnitCount,
                srcRasterFormat, srcColorOrder, srcDepth, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstPaletteData, dstPaletteSize,
                dstDepth, dstPaletteType,
                newtexels, dstDataSize
            );
        }
        
        if ( newtexels != NULL )
        {
            if ( isMipLayerTexels == false )
            {
                // If we have temporary texels, remove them.
                engineInterface->PixelFree( srcTexels );
            }

            // Store the new texels.
            srcTexels = newtexels;
            srcDataSize = dstDataSize;

            // Update raster properties.
            srcRasterFormat = dstRasterFormat;
            srcColorOrder = dstColorOrder;
            srcDepth = dstDepth;

            isMipLayerTexels = false;
        }
    }

    if ( requiresCompression )
    {
        // Perform compression now.
        if ( isDstDXTType )
        {
            if ( dstDXTType == 2 || dstDXTType == 4 )
            {
                throw RwException( "unsupported DXT target in pixel conversion routine (sorry)" );
            }

            void *dstTexels;
            uint32 dstDataSize;

            uint32 newWidth, newHeight;

            compressTexelsUsingDXT(
                engineInterface,
                dstDXTType, srcTexels, mipWidth, mipHeight,
                srcRasterFormat, srcPaletteData, srcPaletteType, srcPaletteSize, srcColorOrder, srcDepth,
                dstTexels, dstDataSize,
                newWidth, newHeight
            );

            // Delete old texels (if necessary).
            if ( isMipLayerTexels == false )
            {
                engineInterface->PixelFree( srcTexels );
            }

            // Update parameters.
            srcTexels = dstTexels;
            srcDataSize = dstDataSize;

            // Clear raster format (for safety).
            srcRasterFormat = RASTER_DEFAULT;
            srcColorOrder = COLOR_BGRA;
            srcDepth = 16;

            mipWidth = newWidth;
            mipHeight = newHeight;
            
            srcCompressionType = dstCompressionType;

            isMipLayerTexels = false;
        }
    }

    // Output parameters.
    if ( copyAnyway == true || isMipLayerTexels == false )
    {
        dstPlaneWidthOut = mipWidth;
        dstPlaneHeightOut = mipHeight;

        dstTexelsOut = srcTexels;
        dstDataSizeOut = srcDataSize;

        return true;
    }

    return false;
}

bool ConvertMipmapLayerEx(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    uint32 mipWidth = mipLayer.width;
    uint32 mipHeight = mipLayer.height;

    uint32 layerWidth = mipLayer.mipWidth;
    uint32 layerHeight = mipLayer.mipHeight;

    void *srcTexels = mipLayer.texels;
    uint32 srcDataSize = mipLayer.dataSize;

    return
        ConvertMipmapLayerNative(
            engineInterface,
            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
            srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
            dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
            copyAnyway,
            dstPlaneWidthOut, dstPlaneHeightOut,
            dstTexelsOut, dstDataSizeOut
        );
}

bool DecideBestDXTCompressionFormat(
    Interface *engineInterface,
    bool srcHasAlpha,
    bool supportsDXT1, bool supportsDXT2, bool supportsDXT3, bool supportsDXT4, bool supportsDXT5,
    float quality,
    eCompressionType& dstCompressionTypeOut
)
{
    // Decide which DXT format we need based on the wanted support.
    eCompressionType targetCompressionType = RWCOMPRESS_NONE;

    // We go from DXT5 to DXT1, only mentioning the actually supported formats.
    if ( srcHasAlpha )
    {
        // Do a smart decision based on the quality parameter.
        if ( targetCompressionType == RWCOMPRESS_NONE )
        {
            if ( supportsDXT5 && quality >= 1.0f )
            {
                targetCompressionType = RWCOMPRESS_DXT5;
            }
            else if ( supportsDXT3 && quality < 1.0f )
            {
                targetCompressionType = RWCOMPRESS_DXT3;
            }
        }

        if ( targetCompressionType == RWCOMPRESS_NONE )
        {
            if ( supportsDXT5 )
            {
                targetCompressionType = RWCOMPRESS_DXT5;
            }
        }

        // No compression to DXT4 yet.

        if ( targetCompressionType == RWCOMPRESS_NONE )
        {
            if ( supportsDXT3 )
            {
                targetCompressionType = RWCOMPRESS_DXT3;
            }
        }

        // No compression to DXT2 yet.
    }

    if ( targetCompressionType == RWCOMPRESS_NONE )
    {
        // Finally we just try DXT1.
        if ( supportsDXT1 )
        {
            // Take it or leave it!
            // Actually, DXT1 _does_ support alpha, but I do not recommend using it.
            // It is only for very desperate people.
            targetCompressionType = RWCOMPRESS_DXT1;
        }
    }

    // Alright, so if we decided for anything other than a raw raster, we may begin compression.
    bool compressionSuccess = false;

    if ( targetCompressionType != RWCOMPRESS_NONE )
    {
        dstCompressionTypeOut = targetCompressionType;

        compressionSuccess = true;
    }

    return compressionSuccess;
}

void ConvertPaletteData(
    const void *srcPaletteTexels, void *dstPaletteTexels,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder, uint32 srcPalRasterDepth,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder, uint32 dstPalRasterDepth
)
{
    // Process valid colors.
    uint32 canProcessCount = std::min( srcPaletteSize, dstPaletteSize );

    for ( uint32 n = 0; n < canProcessCount; n++ )
    {
        uint8 r, g, b, a;

        bool hasColor = browsetexelcolor(
            srcPaletteTexels, PALETTE_NONE, NULL, 0, n, srcRasterFormat, srcColorOrder, srcPalRasterDepth,
            r, g, b, a
        );

        if ( !hasColor )
        {
            r = 0;
            g = 0;
            b = 0;
            a = 0;
        }

        puttexelcolor(
            dstPaletteTexels, n, dstRasterFormat, dstColorOrder, dstPalRasterDepth,
            r, g, b, a
        );
    }

    // Zero out any remainder.
    for ( uint32 n = canProcessCount; n < dstPaletteSize; n++ )
    {
        puttexelcolor(
            dstPaletteTexels, n, dstRasterFormat, dstColorOrder, dstPalRasterDepth,
            0, 0, 0, 0
        );
    }
}

bool ConvertPixelData( Interface *engineInterface, pixelDataTraversal& pixelsToConvert, const pixelFormat pixFormat )
{
    // We must have stand-alone pixel data.
    // Otherwise we could mess up pretty badly!
    assert( pixelsToConvert.isNewlyAllocated == true );

    // Decide how to convert stuff.
    bool hasUpdated = false;

    eCompressionType srcCompressionType = pixelsToConvert.compressionType;
    eCompressionType dstCompressionType = pixFormat.compressionType;

    uint32 srcDXTType, dstDXTType;

    bool isSrcDXTCompressed = IsDXTCompressionType( srcCompressionType, srcDXTType );
    bool isDstDXTCompressed = IsDXTCompressionType( dstCompressionType, dstDXTType );

    bool shouldRecalculateAlpha = false;

    if ( isSrcDXTCompressed || isDstDXTCompressed )
    {
        // Check whether the compression status even changed.
        if ( srcCompressionType != dstCompressionType )
        {
            // If we were compressed, decompress.
            bool compressionSuccess = false;
            bool decompressionSuccess = false;

            if ( isSrcDXTCompressed )
            {
                decompressionSuccess = genericDecompressDXTNative( engineInterface, pixelsToConvert, srcDXTType );
            }
            else
            {
                // No decompression means we are always successful.
                decompressionSuccess = true;
            }

            if ( decompressionSuccess )
            {
                // If we have to compress, do it.
                if ( isDstDXTCompressed )
                {
                    genericCompressDXTNative( engineInterface, pixelsToConvert, dstDXTType );

                    // No way to fail compression, yet.
                    compressionSuccess = true;
                }
                else
                {
                    // If we do not want to compress the target, then we
                    // want to put it into a proper pixel format.
                    pixelFormat pixSubFormat;
                    pixSubFormat.rasterFormat = pixFormat.rasterFormat;
                    pixSubFormat.depth = pixFormat.depth;
                    pixSubFormat.colorOrder = pixFormat.colorOrder;
                    pixSubFormat.paletteType = pixFormat.paletteType;
                    pixSubFormat.compressionType = RWCOMPRESS_NONE;

                    bool subSuccess = ConvertPixelData( engineInterface, pixelsToConvert, pixSubFormat );

                    // We are successful if the sub conversion succeeded.
                    compressionSuccess = subSuccess;
                }
            }

            if ( compressionSuccess || decompressionSuccess )
            {
                // TODO: if this routine is incomplete, revert to last known best status.
                hasUpdated = true;
            }
        }
    }
    else if ( srcCompressionType == RWCOMPRESS_NONE && dstCompressionType == RWCOMPRESS_NONE )
    {
        ePaletteType srcPaletteType = pixelsToConvert.paletteType;
        ePaletteType dstPaletteType = pixFormat.paletteType;

        if ( srcPaletteType == PALETTE_NONE && dstPaletteType != PALETTE_NONE )
        {
            // Call into the palettizer.
            // It will do the remainder of the complex job.
            PalettizePixelData( engineInterface, pixelsToConvert, pixFormat );

            // We assume the palettization has always succeeded.
            hasUpdated = true;

            // Recalculating alpha is important.
            shouldRecalculateAlpha = true;
        }
        else if ( srcPaletteType != PALETTE_NONE && dstPaletteType == PALETTE_NONE ||
                  srcPaletteType != PALETTE_NONE && dstPaletteType != PALETTE_NONE ||
                  srcPaletteType == PALETTE_NONE && dstPaletteType == PALETTE_NONE )
        {
            // Grab the values on the stack.
            eRasterFormat srcRasterFormat = pixelsToConvert.rasterFormat;
            eColorOrdering srcColorOrder = pixelsToConvert.colorOrder;
            uint32 srcDepth = pixelsToConvert.depth;

            eRasterFormat dstRasterFormat = pixFormat.rasterFormat;
            eColorOrdering dstColorOrder = pixFormat.colorOrder;
            uint32 dstDepth = pixFormat.depth;

            // Check whether we even have to update the texels.
            if ( srcRasterFormat != dstRasterFormat || srcPaletteType != dstPaletteType || srcColorOrder != dstColorOrder || srcDepth != dstDepth )
            {
                // Grab palette parameters.
                void *srcPaletteTexels = pixelsToConvert.paletteData;
                uint32 srcPaletteSize = pixelsToConvert.paletteSize;

                uint32 srcPalRasterDepth;

                if ( srcPaletteType != PALETTE_NONE )
                {
                    srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
                }

                void *dstPaletteTexels = srcPaletteTexels;
                uint32 dstPaletteSize = srcPaletteSize;

                uint32 dstPalRasterDepth;

                if ( dstPaletteType != PALETTE_NONE )
                {
                    dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );
                }

                // Check whether we have to update the palette texels.
                if ( dstPaletteType != PALETTE_NONE )
                {
                    // Make sure we had a palette before.
                    assert( srcPaletteType != PALETTE_NONE );

                    if ( dstPaletteType == PALETTE_4BIT )
                    {
                        dstPaletteSize = 16;
                    }
                    else if ( dstPaletteType == PALETTE_8BIT )
                    {
                        dstPaletteSize = 256;
                    }
                    else
                    {
                        assert( 0 );
                    }

                    // If the palette increased in size, allocate a new array for it.
                    if ( srcPaletteSize != dstPaletteSize || srcPalRasterDepth != dstPalRasterDepth )
                    {
                        uint32 dstPalDataSize = getRasterDataSize( dstPaletteSize, dstPalRasterDepth );

                        dstPaletteTexels = engineInterface->PixelAllocate( dstPalDataSize );
                    }
                }
                else
                {
                    dstPaletteTexels = NULL;
                    dstPaletteSize = 0;
                }

                // If we have a palette, process it.
                if ( srcPaletteType != PALETTE_NONE && dstPaletteType != PALETTE_NONE )
                {
                    ConvertPaletteData(
                        srcPaletteTexels, dstPaletteTexels,
                        srcPaletteSize, dstPaletteSize,
                        srcRasterFormat, srcColorOrder, srcDepth,
                        dstRasterFormat, dstColorOrder, dstDepth
                    );
                }

                // Process mipmaps.
                uint32 mipmapCount = pixelsToConvert.mipmaps.size();

                // Determine the depth of the items.
                for ( uint32 n = 0; n < mipmapCount; n++ )
                {
                    pixelDataTraversal::mipmapResource& mipLayer = pixelsToConvert.mipmaps[ n ];

                    void *dstTexels;
                    uint32 dstTexelsDataSize;

                    // Convert this mipmap.
                    ConvertMipmapLayer(
                        engineInterface,
                        mipLayer, srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteTexels, srcPaletteSize,
                        dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType,
                        false,
                        dstTexels, dstTexelsDataSize
                    );

                    // Update mipmap properties.
                    void *srcTexels = mipLayer.texels;

                    if ( dstTexels != srcTexels )
                    {
                        // Delete old texels.
                        // We always have texels allocated.
                        engineInterface->PixelFree( srcTexels );

                        // Replace stuff.
                        mipLayer.texels = dstTexels;
                    }

                    uint32 srcTexelsDataSize = mipLayer.dataSize;

                    if ( dstTexelsDataSize != srcTexelsDataSize )
                    {
                        // Update the data size.
                        mipLayer.dataSize = dstTexelsDataSize;
                    }
                }

                // Update the palette if necessary.
                if ( srcPaletteTexels != dstPaletteTexels )
                {
                    if ( srcPaletteTexels )
                    {
                        // Delete old palette texels.
                        engineInterface->PixelFree( srcPaletteTexels );
                    }

                    // Put in the new ones.
                    pixelsToConvert.paletteData = dstPaletteTexels;
                }

                if ( srcPaletteSize != dstPaletteSize )
                {
                    pixelsToConvert.paletteSize = dstPaletteSize;
                }

                // Update the D3DFORMAT field.
                hasUpdated = true;

                // We definately should recalculate alpha.
                shouldRecalculateAlpha = true;
            }

            // Update texture properties that changed.
            if ( srcRasterFormat != dstRasterFormat )
            {
                pixelsToConvert.rasterFormat = dstRasterFormat;
            }

            if ( srcPaletteType != dstPaletteType )
            {
                pixelsToConvert.paletteType = dstPaletteType;
            }

            if ( srcColorOrder != dstColorOrder )
            {
                pixelsToConvert.colorOrder = dstColorOrder;
            }

            if ( srcDepth != dstDepth )
            {
                pixelsToConvert.depth = dstDepth;
            }
        }
    }

    // If we have updated the pixels, we _should_ recalculate the pixel alpha flag.
    if ( hasUpdated )
    {
        if ( shouldRecalculateAlpha )
        {
            pixelsToConvert.hasAlpha = calculateHasAlpha( pixelsToConvert );
        }
    }

    return hasUpdated;
}

bool ConvertPixelDataDeferred( Interface *engineInterface, const pixelDataTraversal& srcPixels, pixelDataTraversal& dstPixels, const pixelFormat pixFormat )
{
    // First create a new copy of the texels.
    dstPixels.CloneFrom( engineInterface, srcPixels );

    // Perform the conversion on the new texels.
    return ConvertPixelData( engineInterface, dstPixels, pixFormat );
}

}