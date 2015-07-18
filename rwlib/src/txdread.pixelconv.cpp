#include "StdInc.h"

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

#include "txdread.palette.hxx"

namespace rw
{

inline bool decompressTexelsUsingDXT(
    Interface *engineInterface, uint32 dxtType, eDXTCompressionMethod dxtMethod,
    uint32 texWidth, uint32 texHeight, uint32 texRowAlignment,
    uint32 texLayerWidth, uint32 texLayerHeight,
    const void *srcTexels, eRasterFormat rawRasterFormat, eColorOrdering rawColorOrder, uint32 rawDepth,
    void*& dstTexelsOut, uint32& dstTexelsDataSizeOut
)
{
	uint32 x = 0, y = 0;

    // Allocate the new texel array.
	uint32 rowSize = getRasterDataRowSize( texWidth, rawDepth, texRowAlignment );

    uint32 dataSize = getRasterDataSizeByRowSize( rowSize, texHeight );

	void *newtexels = engineInterface->PixelAllocate( dataSize );

    // Get the compressed block count.
    uint32 compressedBlockCount = ( texWidth * texHeight ) / 16;

    bool successfullyDecompressed = true;

    colorModelDispatcher <void> putDispatch( rawRasterFormat, rawColorOrder, rawDepth, NULL, 0, PALETTE_NONE );

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

                    // Get the target row.
                    void *theRow = getTexelDataRow( newtexels, rowSize, target_y );

                    putDispatch.setRGBA(theRow, target_x, red, green, blue, alpha);
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

bool genericDecompressDXTNative(
    Interface *engineInterface, pixelDataTraversal& pixelData, uint32 dxtType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder
)
{
    eDXTCompressionMethod dxtMethod = engineInterface->GetDXTRuntime();

    // We must have stand-alone pixel data.
    // Otherwise we could mess up pretty badly!
    assert( pixelData.isNewlyAllocated == true );

    // Decide which raster format to use.
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
                texWidth, texHeight, dstRowAlignment,
                texLayerWidth, texLayerHeight,
                texelData, dstRasterFormat, dstColorOrder, dstDepth,
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
        pixelData.depth = dstDepth;

        // Update the raster format.
        pixelData.rasterFormat = dstRasterFormat;

        // Update row alignment.
        pixelData.rowAlignment = dstRowAlignment;

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
    uint32 rowAlignment = pixelData.rowAlignment;

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
            dxtType, texelSource, mipWidth, mipHeight, rowAlignment,
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

    pixelData.rowAlignment = 0; // we are not raw anymore, so we do not have a row alignment.
    pixelData.compressionType = targetCompressionType;
}

void ConvertPaletteDepthEx(
    const void *srcTexels, void *dstTexels,
    uint32 srcTexelOffX, uint32 srcTexelOffY,
    uint32 dstTexelOffX, uint32 dstTexelOffY,
    uint32 texWidth, uint32 texHeight,
    uint32 texProcessWidth, uint32 texProcessHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
)
{
    // Copy palette indice.
    uint32 srcRowSize = getRasterDataRowSize( texWidth, srcDepth, srcRowAlignment );
    uint32 dstRowSize = getRasterDataRowSize( texWidth, dstDepth, dstRowAlignment );

    for ( uint32 row = 0; row < texProcessHeight; row++ )
    {
        const void *srcRow = getConstTexelDataRow( srcTexels, srcRowSize, row + srcTexelOffY );
        void *dstRow = getTexelDataRow( dstTexels, dstRowSize, row + dstTexelOffY );

        for ( uint32 col = 0; col < texProcessWidth; col++ )
        {
            uint8 palIndex;

            // Fetch the index
            {
                uint32 realSrcPalIndex = ( col + srcTexelOffX );

                bool gotPaletteIndex = getpaletteindex(srcRow, srcPaletteType, paletteSize, srcDepth, realSrcPalIndex, palIndex);

                if ( !gotPaletteIndex )
                {
                    palIndex = 0;
                }
            }

            // Put the index.
            {
                uint32 realDstPalIndex = ( col + dstTexelOffX );

                setpaletteindex(dstRow, realDstPalIndex, dstDepth, dstPaletteType, palIndex);
            }
        }
    }
}

void ConvertPaletteDepth(
    const void *srcTexels, void *dstTexels,
    uint32 texWidth, uint32 texHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
)
{
    ConvertPaletteDepthEx(
        srcTexels, dstTexels,
        0, 0,
        0, 0,
        texWidth, texHeight,
        texWidth, texHeight,
        srcPaletteType, dstPaletteType,
        paletteSize,
        dstDepth, dstDepth,
        srcRowAlignment, dstRowAlignment
    );
}

template <typename srcColorDispatcher, typename dstColorDispatcher>
inline void copyTexelData(
    const void *srcTexels, void *dstTexels,
    srcColorDispatcher& fetchDispatch, dstColorDispatcher& putDispatch,
    uint32 mipWidth, uint32 mipHeight,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
)
{
    uint32 srcRowSize = getRasterDataRowSize( mipWidth, srcDepth, srcRowAlignment );
    uint32 dstRowSize = getRasterDataRowSize( mipWidth, dstDepth, dstRowAlignment );

    copyTexelDataEx(
        srcTexels, dstTexels,
        fetchDispatch, putDispatch,
        mipWidth, mipHeight,
        0, 0,
        0, 0,
        srcRowSize, dstRowSize
    );
}

void ConvertMipmapLayer(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType,
    bool forceAllocation,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // Grab the source parameters.
    void *srcTexels = mipLayer.texels;

    uint32 srcTexelsDataSize = mipLayer.dataSize;

    uint32 srcWidth = mipLayer.width;               // the dimensions will stay the same.
    uint32 srcHeight = mipLayer.height;

    // Check whether we need to reallocate the texels.
    void *dstTexels = srcTexels;

    uint32 dstTexelsDataSize = srcTexelsDataSize;

    if ( forceAllocation || shouldAllocateNewRasterBuffer( srcWidth, srcDepth, srcRowAlignment, dstDepth, dstRowAlignment ) )
    {
        uint32 rowSize = getRasterDataRowSize( srcWidth, dstDepth, dstRowAlignment );

        dstTexelsDataSize = getRasterDataSizeByRowSize( rowSize, srcHeight );

        dstTexels = engineInterface->PixelAllocate( dstTexelsDataSize );
    }

    if ( dstPaletteType != PALETTE_NONE )
    {
        // Make sure we came from a palette.
        assert( srcPaletteType != PALETTE_NONE );

        // We only have work to do if the depth changed or the pointers to the arrays changed.
        if ( srcDepth != dstDepth || srcTexels != dstTexels || srcPaletteType != dstPaletteType )
        {
            ConvertPaletteDepth(
                srcTexels, dstTexels,
                srcWidth, srcHeight,
                srcPaletteType, dstPaletteType, srcPaletteSize,
                srcDepth, dstDepth,
                srcRowAlignment, dstRowAlignment
            );
        }
    }
    else
    {
        colorModelDispatcher <const void> fetchDispatch( srcRasterFormat, srcColorOrder, srcDepth, srcPaletteData, srcPaletteSize, srcPaletteType );
        colorModelDispatcher <void> putDispatch( dstRasterFormat, dstColorOrder, dstDepth, NULL, 0, PALETTE_NONE );

        copyTexelData(
            srcTexels, dstTexels,
            fetchDispatch, putDispatch,
            srcWidth, srcHeight,
            srcDepth, dstDepth,
            srcRowAlignment, dstRowAlignment
        );
    }
    
    // Give data to the runtime.
    dstTexelsOut = dstTexels;
    dstDataSizeOut = dstTexelsDataSize;
}

bool ConvertMipmapLayerNative(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
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
                mipWidth, mipHeight, dstRowAlignment,
                layerWidth, layerHeight,
                srcTexels, dstRasterFormat, dstColorOrder, dstDepth,
                decompressedTexels, decompressedSize
            );

            assert( success == true );

            // Update with raw raster data.
            srcRasterFormat = dstRasterFormat;
            srcColorOrder = dstColorOrder;
            srcDepth = dstDepth;
            srcRowAlignment = dstRowAlignment;

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
        void *newtexels = NULL;
        uint32 dstDataSize = 0;

        if ( dstPaletteType == PALETTE_NONE )
        {
            uint32 srcRowSize = getRasterDataRowSize( mipWidth, srcDepth, srcRowAlignment );

            uint32 dstRowSize = getRasterDataRowSize( mipWidth, dstDepth, dstRowAlignment );

            dstDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

            newtexels = engineInterface->PixelAllocate( dstDataSize );

            colorModelDispatcher <const void> fetchDispatch( srcRasterFormat, srcColorOrder, srcDepth, srcPaletteData, srcPaletteSize, srcPaletteType );
            colorModelDispatcher <void> putDispatch( dstRasterFormat, dstColorOrder, dstDepth, NULL, 0, PALETTE_NONE );

            // Do the conversion.
            copyTexelDataEx(
                srcTexels, newtexels,
                fetchDispatch, putDispatch,
                mipWidth, mipHeight,
                0, 0,
                0, 0,
                srcRowSize, dstRowSize
            );
        }
        else if ( srcPaletteType != PALETTE_NONE )
        {
            if ( srcPaletteData == dstPaletteData )
            {
                // Fix the indice, if necessary.
                if ( srcDepth != dstDepth || srcPaletteType != dstPaletteType )
                {
                    uint32 dstRowSize = getRasterDataRowSize( mipWidth, dstDepth, dstRowAlignment );

                    dstDataSize = getRasterDataSizeByRowSize( dstRowSize, mipHeight );

                    newtexels = engineInterface->PixelAllocate( dstDataSize );

                    // Convert the depth.
                    ConvertPaletteDepth(
                        srcTexels, newtexels,
                        mipWidth, mipHeight,
                        srcPaletteType, dstPaletteType, srcPaletteSize,
                        srcDepth, dstDepth,
                        srcRowAlignment, dstRowAlignment
                    );
                }
                else
                {
                    dstDataSize = srcDataSize;

                    newtexels = engineInterface->PixelAllocate( dstDataSize );

                    // We just copy.
                    memcpy( newtexels, srcTexels, dstDataSize );
                }
            }
            else
            {
                // Remap texels.
                RemapMipmapLayer(
                    engineInterface,
                    dstRasterFormat, dstColorOrder,
                    srcTexels, mipWidth, mipHeight,
                    srcRasterFormat, srcColorOrder, srcDepth, srcPaletteType, srcPaletteData, srcPaletteSize,
                    dstPaletteData, dstPaletteSize,
                    dstDepth, dstPaletteType,
                    srcRowAlignment, dstRowAlignment,
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
                srcTexels, mipWidth, mipHeight,
                srcRasterFormat, srcColorOrder, srcDepth, srcPaletteType, srcPaletteData, srcPaletteSize,
                dstPaletteData, dstPaletteSize,
                dstDepth, dstPaletteType,
                srcRowAlignment, dstRowAlignment,
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
            srcRowAlignment = dstRowAlignment;

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
                dstDXTType, srcTexels, mipWidth, mipHeight, srcRowAlignment,
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
            srcRowAlignment = 0;

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
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
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
            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
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

    colorModelDispatcher <const void> fetchDispatcher( srcRasterFormat, srcColorOrder, srcPalRasterDepth, NULL, 0, PALETTE_NONE );
    colorModelDispatcher <void> putDispatcher( dstRasterFormat, dstColorOrder, dstPalRasterDepth, NULL, 0, PALETTE_NONE );

    for ( uint32 n = 0; n < canProcessCount; n++ )
    {
        abstractColorItem colorItem;

        fetchDispatcher.getColor( srcPaletteTexels, n, colorItem );

        putDispatcher.setColor( dstPaletteTexels, n, colorItem );
    }

    // Zero out any remainder.
    for ( uint32 n = canProcessCount; n < dstPaletteSize; n++ )
    {
        putDispatcher.clearColor( dstPaletteTexels, n );
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
                decompressionSuccess =
                    genericDecompressDXTNative(
                        engineInterface, pixelsToConvert, srcDXTType,
                        pixFormat.rasterFormat, Bitmap::getRasterFormatDepth( pixFormat.rasterFormat ),
                        pixFormat.rowAlignment, pixFormat.colorOrder
                    );
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
                    pixSubFormat.rowAlignment = pixFormat.rowAlignment;
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
            uint32 srcRowAlignment = pixelsToConvert.rowAlignment;

            eRasterFormat dstRasterFormat = pixFormat.rasterFormat;
            eColorOrdering dstColorOrder = pixFormat.colorOrder;
            uint32 dstDepth = pixFormat.depth;
            uint32 dstRowAlignment = pixFormat.rowAlignment;

            // Check whether we even have to update the texels.
            if ( srcRasterFormat != dstRasterFormat || srcPaletteType != dstPaletteType || srcColorOrder != dstColorOrder || srcDepth != dstDepth || srcRowAlignment != dstRowAlignment )
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

                    dstPaletteSize = getPaletteItemCount( dstPaletteType );

                    // If the palette increased in size, allocate a new array for it.
                    if ( srcPaletteSize != dstPaletteSize || srcPalRasterDepth != dstPalRasterDepth )
                    {
                        uint32 dstPalDataSize = getPaletteDataSize( dstPaletteSize, dstPalRasterDepth );

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
                        mipLayer,
                        srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteTexels, srcPaletteSize,
                        dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType,
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
            
            if ( srcRowAlignment != dstRowAlignment )
            {
                pixelsToConvert.rowAlignment = dstRowAlignment;
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

// Pixel manipulation API.
bool BrowseTexelRGBA(
    const void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut
)
{
    return colorModelDispatcher <const void> ( rasterFormat, colorOrder, depth, paletteData, paletteSize, paletteType ).getRGBA( texelSource, texelIndex, redOut, greenOut, blueOut, alphaOut );
}

bool BrowseTexelLuminance(
    const void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& lumOut
)
{
    return colorModelDispatcher <const void> ( rasterFormat, COLOR_RGBA, depth, paletteData, paletteSize, paletteType ).getLuminance( texelSource, texelIndex, lumOut );
}

eColorModel GetRasterFormatColorModel( eRasterFormat rasterFormat )
{
    return getColorModelFromRasterFormat( rasterFormat );
}

bool PutTexelRGBA(
    void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder,
    uint8 red, uint8 green, uint8 blue, uint8 alpha
)
{
    return colorModelDispatcher <void> ( rasterFormat, colorOrder, depth, NULL, 0, PALETTE_NONE ).setRGBA( texelSource, texelIndex, red, green, blue, alpha );
}

bool PutTexelLuminance(
    void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth,
    uint8 lum
)
{
    return colorModelDispatcher <void> ( rasterFormat, COLOR_RGBA, depth, NULL, 0, PALETTE_NONE ).setLuminance( texelSource, texelIndex, lum );
}

void pixelDataTraversal::FreePixels( Interface *engineInterface )
{
    if ( this->isNewlyAllocated )
    {
        uint32 mipmapCount = this->mipmaps.size();

        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            mipmapResource& thisLayer = this->mipmaps[ n ];

            if ( void *texels = thisLayer.texels )
            {
                engineInterface->PixelFree( texels );

                thisLayer.texels = NULL;
            }
        }

        this->mipmaps.clear();

        if ( void *paletteData = this->paletteData )
        {
            engineInterface->PixelFree( paletteData );

            this->paletteData = NULL;
        }

        this->isNewlyAllocated = false;
    }
}

void pixelDataTraversal::CloneFrom( Interface *engineInterface, const pixelDataTraversal& right )
{
    // Free any previous data.
    this->FreePixels( engineInterface );

    // Clone parameters.
    eRasterFormat rasterFormat = right.rasterFormat;

    this->isNewlyAllocated = true;  // since we clone
    this->rasterFormat = rasterFormat;
    this->depth = right.depth;
    this->rowAlignment = right.rowAlignment;
    this->colorOrder = right.colorOrder;

    // Clone palette.
    this->paletteType = right.paletteType;
    
    void *srcPaletteData = right.paletteData;
    void *dstPaletteData = NULL;

    uint32 dstPaletteSize = 0;

    if ( srcPaletteData )
    {
        uint32 srcPaletteSize = right.paletteSize;  
        
        // Copy the palette texels.
        uint32 palRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

        uint32 palDataSize = getPaletteDataSize( srcPaletteSize, palRasterDepth );

        dstPaletteData = engineInterface->PixelAllocate( palDataSize );

        memcpy( dstPaletteData, srcPaletteData, palDataSize );

        dstPaletteSize = srcPaletteSize;
    }

    this->paletteData = dstPaletteData;
    this->paletteSize = dstPaletteSize;

    // Clone mipmaps.
    uint32 mipmapCount = right.mipmaps.size();

    this->mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const mipmapResource& srcLayer = right.mipmaps[ n ];

        mipmapResource newLayer;

        newLayer.width = srcLayer.width;
        newLayer.height = srcLayer.height;

        newLayer.mipWidth = srcLayer.mipWidth;
        newLayer.mipHeight = srcLayer.mipHeight;

        // Copy the mipmap layer texels.
        uint32 mipDataSize = srcLayer.dataSize;

        const void *srcTexels = srcLayer.texels;

        void *newtexels = engineInterface->PixelAllocate( mipDataSize );

        memcpy( newtexels, srcTexels, mipDataSize );

        newLayer.texels = newtexels;
        newLayer.dataSize = mipDataSize;

        // Store this layer.
        this->mipmaps[ n ] = newLayer;
    }

    // Clone non-trivial parameters.
    this->compressionType = right.compressionType;
    this->hasAlpha = right.hasAlpha;
    this->autoMipmaps = right.autoMipmaps;
    this->cubeTexture = right.cubeTexture;
    this->rasterType = right.rasterType;
}

void pixelDataTraversal::mipmapResource::Free( Interface *engineInterface )
{
    // Free the data here, since we have the Interface struct defined.
    if ( void *ourTexels = this->texels )
    {
        engineInterface->PixelFree( ourTexels );

        // We have no more texels.
        this->texels = NULL;
    }
}

}