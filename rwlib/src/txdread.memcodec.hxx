// This file contains general memory encoding routines (so called swizzling).
// It is recommended to use this file if you need stable, proofed algorithms.
#ifndef RW_TEXTURE_MEMORY_ENCODING
#define RW_TEXTURE_MEMORY_ENCODING

// Optimized algorithms are frowned upon, because in general it is hard to proove their correctness.

namespace rw
{

namespace memcodec
{

// Common utilities for permutation providers.
namespace permutationUtilities
{
    inline static void permuteArray(
        const void *srcToBePermuted, uint32 rawWidth, uint32 rawHeight, uint32 rawDepth, uint32 rawColumnWidth, uint32 rawColumnHeight,
        void *dstTexels, uint32 packedWidth, uint32 packedHeight, uint32 packedDepth, uint32 packedColumnWidth, uint32 packedColumnHeight,
        uint32 colsWidth, uint32 colsHeight,
        const uint32 *permutationData_primCol, const uint32 *permutationData_secCol, uint32 permWidth, uint32 permHeight,
        uint32 permutationStride, uint32 permHoriSplit,
        uint32 srcRowAlignment, uint32 dstRowAlignment,
        bool revert
        )
    {
        // Get the dimensions of a column as expressed in units of the permutation format.
        uint32 permProcessColumnWidth = packedColumnWidth;
        uint32 permProcessColumnHeight = packedColumnHeight;

        uint32 permIterWidth = rawColumnWidth;
        uint32 permIterHeight = rawColumnHeight;

        uint32 permSourceWidth = rawWidth;
        uint32 permSourceHeight = rawHeight;

        uint32 packedTargetWidth = packedWidth;
        uint32 packedTargetHeight = packedHeight;

        uint32 permItemDepth = rawDepth;

        uint32 packedTransformedColumnWidth = ( permProcessColumnWidth * permutationStride ) / permHoriSplit;
        uint32 packedTransformedColumnHeight = ( permProcessColumnHeight );

        colsWidth *= permHoriSplit;

        // Get the stride through the packed data in raw format.
        uint32 packedTransformedStride = ( packedTargetWidth * permutationStride );

        // Determine the strides for both arrays.
        uint32 srcStride, targetStride;

        if ( !revert )
        {
            srcStride = permSourceWidth;
            targetStride = packedTransformedStride;
        }
        else
        {
            srcStride = packedTransformedStride;
            targetStride = permSourceWidth;
        }

        // Calculate the row sizes.
        uint32 srcRowSize = getRasterDataRowSize( srcStride, permItemDepth, srcRowAlignment );
        uint32 dstRowSize = getRasterDataRowSize( targetStride, permItemDepth, dstRowAlignment );

        // Permute the pixels.
        for ( uint32 colY = 0; colY < colsHeight; colY++ )
        {
            // Get the data to permute with.
            bool isPrimaryCol = ( colY % 2 == 0 );

            const uint32 *permuteData =
                ( isPrimaryCol ? permutationData_primCol : permutationData_secCol );

            // Get the 2D array offset of colY (source array).
            uint32 source_colY_pixeloff = ( colY * permIterHeight );

            // Get the 2D array offset of colY (target array).
            uint32 target_colY_pixeloff = ( colY * packedTransformedColumnHeight );

            for ( uint32 colX = 0; colX < colsWidth; colX++ )
            {
                // Get the 2D array offset of colX (source array).
                uint32 source_colX_pixeloff = ( colX * permIterWidth );

                // Get the 2D array offset of colX (target array).
                uint32 target_colX_pixeloff = ( colX * packedTransformedColumnWidth );

                // Loop through all pixels of this column and permute them.
                for ( uint32 permY = 0; permY < packedTransformedColumnHeight; permY++ )
                {
                    for ( uint32 permX = 0; permX < packedTransformedColumnWidth; permX++ )
                    {
                        // Get the index of this pixel.
                        uint32 localPixelIndex = ( permY * packedTransformedColumnWidth + permX );

                        // Get the new location to put the pixel at.
                        uint32 newPixelLoc = permuteData[ localPixelIndex ];

                        // Transform this coordinate into a 2D array position.
                        uint32 local_pixel_xOff = ( newPixelLoc % permIterWidth );
                        uint32 local_pixel_yOff = ( newPixelLoc / permIterWidth );

                        // Get the combined pixel position.
                        uint32 source_pixel_xOff = ( source_colX_pixeloff + local_pixel_xOff );
                        uint32 source_pixel_yOff = ( source_colY_pixeloff + local_pixel_yOff );

                        uint32 target_pixel_xOff = ( target_colX_pixeloff + permX );
                        uint32 target_pixel_yOff = ( target_colY_pixeloff + permY );

                        if ( source_pixel_xOff < permSourceWidth && source_pixel_yOff < permSourceHeight &&
                             target_pixel_xOff < packedTransformedStride &&
                             target_pixel_yOff < packedTargetHeight )
                        {
                            // Determine the 2D array coordinates for source and destionation arrays.
                            uint32 source_xOff, source_yOff;
                            uint32 target_xOff, target_yOff;

                            if ( !revert )
                            {
                                source_xOff = source_pixel_xOff;
                                source_yOff = source_pixel_yOff;

                                target_xOff = target_pixel_xOff;
                                target_yOff = target_pixel_yOff;
                            }
                            else
                            {
                                source_xOff = target_pixel_xOff;
                                source_yOff = target_pixel_yOff;

                                target_xOff = source_pixel_xOff;
                                target_yOff = source_pixel_yOff;
                            }

                            // Get the rows.
                            const void *srcRow = getConstTexelDataRow( srcToBePermuted, srcRowSize, source_yOff );
                            void *dstRow = getTexelDataRow( dstTexels, dstRowSize, target_yOff );

                            // Move the data over.
                            moveDataByDepth( dstRow, srcRow, permItemDepth, target_xOff, source_xOff );
                        }
                    }
                }
            }
        }
    }
};

// Class factory for creating a memory permutation engine.
template <typename baseSystem>
struct genericMemoryEncoder
{
    typedef typename baseSystem::encodingFormatType encodingFormatType;

    // Expects the following methods in baseSystem:
/*
    inline static uint32 getFormatEncodingDepth( encodingFormatType format );

    inline static bool isPackOperation( encodingFormatType srcFormat, encodingFormatType dstFormat );

    inline static bool getEncodingFormatDimensions(
        encodingFormatType encodingType,
        uint32& pixelColumnWidth, uint32& pixelColumnHeight
    );

    inline static bool getPermutationDimensions( encodingFormatType permFormat, uint32& permWidth, uint32& permHeight );

    inline static bool detect_packing_routine(
        encodingFormatType rawFormat, encodingFormatType packedFormat,
        const uint32*& permutationData_primCol,
        const uint32*& permutationData_secCol
    )
*/

    // Purpose of this routine is to pack smaller memory data units into bigger
    // memory data units in a way, so that unpacking is easier for the hardware than
    // it would be in its raw permutation.
    // It is pure optimization. A great hardware usage example is the PlayStation 2.
    inline static void* transformImageData(
        Interface *engineInterface,
        encodingFormatType srcFormat, encodingFormatType dstFormat,
        const void *srcToBeTransformed,
        uint32 srcMipWidth, uint32 srcMipHeight,
        uint32 srcRowAlignment, uint32 dstRowAlignment,
        uint32& dstMipWidthInOut, uint32& dstMipHeightInOut,
        uint32& dstDataSizeOut,
        bool hasDestinationDimms = false,
        bool lenientPacked = false
    )
    {
        assert(srcFormat != encodingFormatType::FORMAT_UNKNOWN);
        assert(dstFormat != encodingFormatType::FORMAT_UNKNOWN);

        if ( srcFormat == dstFormat )
        {
            return NULL;
        }

        // Decide whether its unpacking or packing.
        bool isPack = baseSystem::isPackOperation( srcFormat, dstFormat );

        // Packing is the operation of putting smaller data types into bigger data types.
        // Since we define data structures to pack things, we use those both ways.

        encodingFormatType rawFormat, packedFormat;

        if ( isPack )
        {
            rawFormat = srcFormat;
            packedFormat = dstFormat;
        }
        else
        {

            rawFormat = dstFormat;
            packedFormat = srcFormat;
        }

        // We need to get the dimensions of the permutation.
        // This is from the view of packing, so we use the format 'to be packed'.
        uint32 permWidth, permHeight;

        bool gotPermDimms = baseSystem::getPermutationDimensions(rawFormat, permWidth, permHeight);

        assert( gotPermDimms == true );

        // Calculate the permutation stride and the hori split.
        uint32 rawDepth = baseSystem::getFormatEncodingDepth( rawFormat );
        uint32 packedDepth = baseSystem::getFormatEncodingDepth( packedFormat );

        uint32 permutationStride = ( packedDepth / rawDepth );

        uint32 permHoriSplit = ( permutationStride / permWidth );

        // Get the dimensions of the permutation area.
        uint32 rawColumnWidth, rawColumnHeight;
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotRawDimms = baseSystem::getEncodingFormatDimensions( rawFormat, rawColumnWidth, rawColumnHeight );

        assert( gotRawDimms == true );

        bool gotPackedDimms = baseSystem::getEncodingFormatDimensions( packedFormat, packedColumnWidth, packedColumnHeight );

        assert( gotPackedDimms == true );

        // Calculate the dimensions.
        uint32 rawWidth, rawHeight;
        uint32 packedWidth, packedHeight;

        uint32 columnWidthCount;
        uint32 columnHeightCount;

        if ( isPack )
        {
            rawWidth = srcMipWidth;
            rawHeight = srcMipHeight;

            // The raw image does not have to be big enough to fill the entire packed
            // encoding.
            uint32 expRawWidth = ALIGN_SIZE( rawWidth, rawColumnWidth );
            uint32 expRawHeight = ALIGN_SIZE( rawHeight, rawColumnHeight );

            columnWidthCount = ( expRawWidth / rawColumnWidth );
            columnHeightCount = ( expRawHeight / rawColumnHeight );

            if ( hasDestinationDimms )
            {
                packedWidth = dstMipWidthInOut;
                packedHeight = dstMipHeightInOut;
            }
            else
            {
                packedWidth = ALIGN_SIZE( ( packedColumnWidth * columnWidthCount ) / permHoriSplit, packedColumnWidth );
                packedHeight = ( packedColumnHeight * columnHeightCount );
            }
        }
        else
        {
            packedWidth = srcMipWidth;
            packedHeight = srcMipHeight;

            if ( lenientPacked == true )
            {
                // This is a special mode where we allow partial packed transformation.
                // Essentially, we allow broken behavior, because somebody else thought it was a good idea.
                uint32 expPackedWidth = ALIGN_SIZE( packedWidth, packedColumnWidth );
                uint32 expPackedHeight = ALIGN_SIZE( packedHeight, packedColumnHeight );

                columnWidthCount = ( expPackedWidth / packedColumnWidth );
                columnHeightCount = ( expPackedHeight / packedColumnHeight );
            }
            else
            {
                // If texels are packed, they have to be properly formatted.
                // Else there is a real problem.
                assert((packedWidth % packedColumnWidth) == 0);
                assert((packedHeight % packedColumnHeight) == 0);

                columnWidthCount = ( packedWidth / packedColumnWidth );
                columnHeightCount = ( packedHeight / packedColumnHeight );
            }

            if ( hasDestinationDimms )
            {
                rawWidth = dstMipWidthInOut;
                rawHeight = dstMipHeightInOut;
            }
            else
            {
                rawWidth = ( ( rawColumnWidth * columnWidthCount ) * permHoriSplit );
                rawHeight = ( rawColumnHeight * columnHeightCount );
            }
        }

        // Determine the dimensions of the destination data.
        // We could have them already.
        uint32 dstMipWidth, dstMipHeight;

        if ( isPack )
        {
            dstMipWidth = packedWidth;
            dstMipHeight = packedHeight;
        }
        else
        {
            dstMipWidth = rawWidth;
            dstMipHeight = rawHeight;
        }

        // Allocate the container for the destination tranformation.
        uint32 dstFormatDepth;

        if ( isPack )
        {
            dstFormatDepth = packedDepth;
        }
        else
        {
            dstFormatDepth = rawDepth;
        }

        uint32 dstRowSize = getRasterDataRowSize( dstMipWidth, dstFormatDepth, dstRowAlignment );

        uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, dstMipHeight );

        void *newtexels = engineInterface->PixelAllocate( dstDataSize );
        
        if ( newtexels )
        {
            // Determine the encoding permutation.
            const uint32 *permutationData_primCol = NULL;
            const uint32 *permutationData_secCol = NULL;
            
            baseSystem::detect_packing_routine(
                rawFormat, packedFormat,
                permutationData_primCol,
                permutationData_secCol
            );
            
            if (permutationData_primCol != NULL && permutationData_secCol != NULL)
            {
                // Permute!
                permutationUtilities::permuteArray(
                    srcToBeTransformed, rawWidth, rawHeight, rawDepth, rawColumnWidth, rawColumnHeight,
                    newtexels, packedWidth, packedHeight, packedDepth, packedColumnWidth, packedColumnHeight,
                    columnWidthCount, columnHeightCount,
                    permutationData_primCol, permutationData_secCol,
                    permWidth, permHeight,
                    permutationStride, permHoriSplit,
                    srcRowAlignment, dstRowAlignment,
                    !isPack
                );
            }
            else
            {
                engineInterface->PixelFree( newtexels );

                newtexels = NULL;
            }
        }

        if ( newtexels )
        {
            // Return the destination dimms.
            if ( !hasDestinationDimms )
            {
                dstMipWidthInOut = dstMipWidth;
                dstMipHeightInOut = dstMipHeight;
            }

            dstDataSizeOut = dstDataSize;
        }

        return newtexels;
    }

    inline static bool getPackedFormatDimensions(
        encodingFormatType rawFormat, encodingFormatType packedFormat,
        uint32 rawWidth, uint32 rawHeight,
        uint32& packedWidthOut, uint32& packedHeightOut
    )
    {
        // Get the encoding properties of the source data.
        uint32 rawColumnWidth, rawColumnHeight;

        bool gotRawDimms = baseSystem::getEncodingFormatDimensions(rawFormat, rawColumnWidth, rawColumnHeight);

        if ( gotRawDimms == false )
        {
            return false;
        }

        uint32 rawDepth = baseSystem::getFormatEncodingDepth(rawFormat);

        // Make sure the raw texture is a multiple of the column dimensions.
        uint32 expRawWidth = ALIGN_SIZE( rawWidth, rawColumnWidth );
        uint32 expRawHeight = ALIGN_SIZE( rawHeight, rawColumnHeight );

        // Get the amount of columns from our source image.
        // The encoded image must have the same amount of columns, after all.
        uint32 rawWidthColumnCount = ( expRawWidth / rawColumnWidth );
        uint32 rawHeightColumnCount = ( expRawHeight / rawColumnHeight );

        // Get the encoding properties of the target data.
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotPackedDimms = baseSystem::getEncodingFormatDimensions(packedFormat, packedColumnWidth, packedColumnHeight);

        if ( gotPackedDimms == false )
        {
            return false;
        }

        uint32 packedDepth = baseSystem::getFormatEncodingDepth(packedFormat);

        // Get the dimensions of the packed format.
        // Since it has to have the same amount of columns, we multiply those column
        // dimensions with the column counts.
        uint32 packedWidth = ( rawWidthColumnCount * packedColumnWidth );
        uint32 packedHeight = ( rawHeightColumnCount * packedColumnHeight );

        if ( rawFormat != packedFormat )
        {
            // Get permutation dimensions.
            uint32 permWidth, permHeight;

            encodingFormatType permFormat = encodingFormatType::FORMAT_UNKNOWN;

            if ( rawDepth < packedDepth )
            {
                permFormat = rawFormat;
            }
            else
            {
                permFormat = packedFormat;
            }
        
            bool gotPermDimms = baseSystem::getPermutationDimensions(permFormat, permWidth, permHeight);

            if ( gotPermDimms == false )
            {
                return false;
            }

            // Get the amount of raw texels that can be put into one packed texel.
            uint32 permutationStride;

            bool isPack = baseSystem::isPackOperation( rawFormat, packedFormat );

            if ( isPack )
            {
                permutationStride = ( packedDepth / rawDepth );
            }
            else
            {
                permutationStride = ( rawDepth / packedDepth );
            }

            // Get the horizontal split count.
            uint32 permHoriSplit = ( permutationStride / permWidth );

            // Give the values to the runtime.
            if ( isPack )
            {
                packedWidthOut = ( packedWidth / permHoriSplit );
            }
            else
            {
                packedWidthOut = ( packedWidth * permHoriSplit );
            }
            packedHeightOut = packedHeight;
        }
        else
        {
            // Just return what we have.
            packedWidthOut = packedWidth;
            packedHeightOut = packedHeight;
        }

        // Make sure we align the packed coordinates.
        packedWidthOut = ALIGN_SIZE( packedWidthOut, packedColumnWidth );
        packedHeightOut = ALIGN_SIZE( packedHeightOut, packedColumnHeight );

        return true;
    }
};

}

}

#endif //RW_TEXTURE_MEMORY_ENCODING