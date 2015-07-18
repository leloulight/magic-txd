namespace rw
{

// The PS2 memory is a rectangular device. Basically its a set of pages that can be used for allocating image chunks.
// This class is supposed to emulate the texture allocation behavior.
namespace ps2GSMemoryLayoutArrangements
{
    // Layout arrangements.
    const static uint32 psmct32[4][8] =
    {
        { 0u, 1u, 4u, 5u, 16u, 17u, 20u, 21u },
        { 2u, 3u, 6u, 7u, 18u, 19u, 22u, 23u },
        { 8u, 9u, 12u, 13u, 24u, 25u, 28u, 29u },
        { 10u, 11u, 14u, 15u, 26u, 27u, 30u, 31u }
    };
    const static uint32 psmz32[4][8] =
    {
        { 24u, 25u, 28u, 29u, 8u, 9u, 12u, 13u },
        { 26u, 27u, 30u, 31u, 10u, 11u, 14u, 15u },
        { 16u, 17u, 20u, 21u, 0u, 1u, 4u, 5u },
        { 18u, 19u, 22u, 23u, 2u, 3u, 6u, 7u }
    };
    const static uint32 psmct16[8][4] =
    {
        { 0u, 2u, 8u, 10u },
        { 1u, 3u, 9u, 11u },
        { 4u, 6u, 12u, 14u },
        { 5u, 7u, 13u, 15u },
        { 16u, 18u, 24u, 26u },
        { 17u, 19u, 25u, 27u },
        { 20u, 22u, 28u, 30u },
        { 21u, 23u, 29u, 31u }
    };
    const static uint32 psmz16[8][4] =
    {
        { 24u, 26u, 16u, 18u },
        { 25u, 27u, 17u, 19u },
        { 28u, 30u, 20u, 22u },
        { 29u, 31u, 21u, 23u },
        { 8u, 10u, 0u, 2u },
        { 9u, 11u, 1u, 3u },
        { 12u, 14u, 4u, 6u },
        { 13u, 15u, 5u, 7u }
    };
    const static uint32 psmct16s[8][4] =
    {
        { 0u, 2u, 16u, 18u },
        { 1u, 3u, 17u, 19u },
        { 8u, 10u, 24u, 26u },
        { 9u, 11u, 25u, 27u },
        { 4u, 6u, 20u, 22u },
        { 5u, 7u, 21u, 23u },
        { 12u, 14u, 28u, 30u },
        { 13u, 15u, 29u, 31u }
    };
    const static uint32 psmz16s[8][4] =
    {
        { 24u, 26u, 8u, 10u },
        { 25u, 27u, 9u, 11u },
        { 16u, 18u, 0u, 2u },
        { 17u, 19u, 1u, 3u },
        { 28u, 30u, 12u, 14u },
        { 29u, 31u, 13u, 15u },
        { 20u, 22u, 4u, 6u },
        { 21u, 23u, 5u, 7u }
    };
    const static uint32 psmt8[4][8] =
    {
        { 0u, 1u, 4u, 5u, 16u, 17u, 20u, 21u },
        { 2u, 3u, 6u, 7u, 18u, 19u, 22u, 23u },
        { 8u, 9u, 12u, 13u, 24u, 25u, 28u, 29u },
        { 10u, 11u, 14u, 15u, 26u, 27u, 30u, 31u }
    };
    const static uint32 psmt4[8][4] =
    {
        { 0u, 2u, 8u, 10u },
        { 1u, 3u, 9u, 11u },
        { 4u, 6u, 12u, 14u },
        { 5u, 7u, 13u, 15u },
        { 16u, 18u, 24u, 26u },
        { 17u, 19u, 25u, 27u },
        { 20u, 22u, 28u, 30u },
        { 21u, 23u, 29u, 31u }
    };
};

// There structs define how blocks of pixels of smaller size get packed into blocks of pixels of bigger size.
// They are essentially what you call "swizzling", just without confusion shit.
namespace ps2GSPixelEncodingFormats
{
    // width: 32px, height: 4px
    const static uint32 psmt4_to_psmct32_prim[] =
    {
        0, 68, 8, 76, 16, 84, 24, 92,
        1, 69, 9, 77, 17, 85, 25, 93,
        2, 70, 10, 78, 18, 86, 26, 94,
        3, 71, 11, 79, 19, 87, 27, 95,
        4, 64, 12, 72, 20, 80, 28, 88,
        5, 65, 13, 73, 21, 81, 29, 89,
        6, 66, 14, 74, 22, 82, 30, 90,
        7, 67, 15, 75, 23, 83, 31, 91,
        32, 100, 40, 108, 48, 116, 56, 124,
        33, 101, 41, 109, 49, 117, 57, 125,
        34, 102, 42, 110, 50, 118, 58, 126,
        35, 103, 43, 111, 51, 119, 59, 127,
        36, 96, 44, 104, 52, 112, 60, 120,
        37, 97, 45, 105, 53, 113, 61, 121,
        38, 98, 46, 106, 54, 114, 62, 122,
        39, 99, 47, 107, 55, 115, 63, 123
    };

    const static uint32 psmt4_to_psmct32_sec[] =
    {
        4, 64, 12, 72, 20, 80, 28, 88,
        5, 65, 13, 73, 21, 81, 29, 89,
        6, 66, 14, 74, 22, 82, 30, 90,
        7, 67, 15, 75, 23, 83, 31, 91,
        0, 68, 8, 76, 16, 84, 24, 92,
        1, 69, 9, 77, 17, 85, 25, 93,
        2, 70, 10, 78, 18, 86, 26, 94,
        3, 71, 11, 79, 19, 87, 27, 95,
        36, 96, 44, 104, 52, 112, 60, 120,
        37, 97, 45, 105, 53, 113, 61, 121,
        38, 98, 46, 106, 54, 114, 62, 122,
        39, 99, 47, 107, 55, 115, 63, 123,
        32, 100, 40, 108, 48, 116, 56, 124,
        33, 101, 41, 109, 49, 117, 57, 125,
        34, 102, 42, 110, 50, 118, 58, 126,
        35, 103, 43, 111, 51, 119, 59, 127
    };

    // width: 16px, height: 4px
    const static uint32 psmt8_to_psmct32_prim[] =
    {
        0, 36, 8, 44,
        1, 37, 9, 45,
        2, 38, 10, 46,
        3, 39, 11, 47,
        4, 32, 12, 40,
        5, 33, 13, 41,
        6, 34, 14, 42,
        7, 35, 15, 43,
        16, 52, 24, 60,
        17, 53, 25, 61,
        18, 54, 26, 62,
        19, 55, 27, 63,
        20, 48, 28, 56,
        21, 49, 29, 57,
        22, 50, 30, 58,
        23, 51, 31, 59
    };

    const static uint32 psmt8_to_psmct32_sec[] =
    {
        4, 32, 12, 40,
        5, 33, 13, 41,
        6, 34, 14, 42,
        7, 35, 15, 43,
        0, 36, 8, 44,
        1, 37, 9, 45,
        2, 38, 10, 46,
        3, 39, 11, 47,
        20, 48, 28, 56,
        21, 49, 29, 57,
        22, 50, 30, 58,
        23, 51, 31, 59,
        16, 52, 24, 60,
        17, 53, 25, 61,
        18, 54, 26, 62,
        19, 55, 27, 63
    };

    inline static bool getEncodingFormatDimensions(
        eFormatEncodingType encodingType,
        uint32& pixelColumnWidth, uint32& pixelColumnHeight
    )
    {
        uint32 rawColumnWidth = 0;
        uint32 rawColumnHeight = 0;

        if (encodingType == FORMAT_IDTEX4)
        {
            // PSMT4
            rawColumnWidth = 32;
            rawColumnHeight = 4;
        }
        else if (encodingType == FORMAT_IDTEX8)
        {
            // PSMT8
            rawColumnWidth = 16;
            rawColumnHeight = 4;
        }
        else if (encodingType == FORMAT_IDTEX8_COMPRESSED)
        {
            // special format used by RenderWare (undocumented)
            rawColumnWidth = 16;
            rawColumnHeight = 4;
        }
        else if (encodingType == FORMAT_TEX16)
        {
            // PSMCT16
            rawColumnWidth = 16;
            rawColumnHeight = 2;
        }
        else if (encodingType == FORMAT_TEX32)
        {
            // PSMCT32
            rawColumnWidth = 8;
            rawColumnHeight = 2;
        }
        else
        {
            return false;
        }
        
        pixelColumnWidth = rawColumnWidth;
        pixelColumnHeight = rawColumnHeight;
        return true;
    }

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
                             target_pixel_yOff < packedTransformedColumnHeight * colsHeight )
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

    inline bool getPermutationDimensions(eFormatEncodingType permFormat, uint32& permWidth, uint32& permHeight)
    {
        if (permFormat == FORMAT_IDTEX4)
        {
            permWidth = 8;
            permHeight = 16;
        }
        else if (permFormat == FORMAT_IDTEX8 || permFormat == FORMAT_IDTEX8_COMPRESSED)
        {
            permWidth = 4;
            permHeight = 16;
        }
        else
        {
            return false;
        }

        return true;
    }

    inline static void* transformImageData(
        Interface *engineInterface,
        eFormatEncodingType srcFormat, eFormatEncodingType dstFormat,
        const void *srcToBeTransformed,
        uint32 srcMipWidth, uint32 srcMipHeight,
        uint32 srcRowAlignment, uint32 dstRowAlignment,
        uint32& dstMipWidthInOut, uint32& dstMipHeightInOut,
        uint32& dstDataSizeOut,
        bool hasDestinationDimms = false
    )
    {
        assert(srcFormat != FORMAT_UNKNOWN);
        assert(dstFormat != FORMAT_UNKNOWN);

        if ( srcFormat == dstFormat )
        {
            return NULL;
        }

        // Decide whether its unpacking or packing.
        bool isPack = isPackOperation( srcFormat, dstFormat );

        // Packing is the operation of putting smaller data types into bigger data types.
        // Since we define data structures to pack things, we use those both ways.

        eFormatEncodingType rawFormat, packedFormat;

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

        bool gotPermDimms = getPermutationDimensions(rawFormat, permWidth, permHeight);

        assert( gotPermDimms == true );

        // Calculate the permutation stride and the hori split.
        uint32 rawDepth = getFormatEncodingDepth( rawFormat );
        uint32 packedDepth = getFormatEncodingDepth( packedFormat );

        uint32 permutationStride = ( packedDepth / rawDepth );

        uint32 permHoriSplit = ( permutationStride / permWidth );

        // Get the dimensions of the permutation area.
        uint32 rawColumnWidth, rawColumnHeight;
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotRawDimms = getEncodingFormatDimensions( rawFormat, rawColumnWidth, rawColumnHeight );

        assert( gotRawDimms == true );

        bool gotPackedDimms = getEncodingFormatDimensions( packedFormat, packedColumnWidth, packedColumnHeight );

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

            // If texels are packed, they have to be properly formatted.
            // Else there is a real problem.
            assert((packedWidth % packedColumnWidth) == 0);
            assert((packedHeight % packedColumnHeight) == 0);

            columnWidthCount = ( packedWidth / packedColumnWidth );
            columnHeightCount = ( packedHeight / packedColumnHeight );

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

            if (packedFormat == FORMAT_TEX32)
            {
                if (rawFormat == FORMAT_IDTEX4)
                {
                    permutationData_primCol = psmt4_to_psmct32_prim;
                    permutationData_secCol = psmt4_to_psmct32_sec;
                }
                else if (rawFormat == FORMAT_IDTEX8 || rawFormat == FORMAT_IDTEX8_COMPRESSED)
                {
                    permutationData_primCol = psmt8_to_psmct32_prim;
                    permutationData_secCol = psmt8_to_psmct32_sec;
                }
            }

            if (permutationData_primCol != NULL && permutationData_secCol != NULL)
            {
                // Permute!
                permuteArray(
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
        eFormatEncodingType rawFormat, eFormatEncodingType packedFormat,
        uint32 rawWidth, uint32 rawHeight,
        uint32& packedWidthOut, uint32& packedHeightOut
    )
    {
        // Get the encoding properties of the source data.
        uint32 rawColumnWidth, rawColumnHeight;

        bool gotRawDimms = getEncodingFormatDimensions(rawFormat, rawColumnWidth, rawColumnHeight);

        if ( gotRawDimms == false )
        {
            return false;
        }

        uint32 rawDepth = getFormatEncodingDepth(rawFormat);

        // Make sure the raw texture is a multiple of the column dimensions.
        uint32 expRawWidth = ALIGN_SIZE( rawWidth, rawColumnWidth );
        uint32 expRawHeight = ALIGN_SIZE( rawHeight, rawColumnHeight );

        // Get the amount of columns from our source image.
        // The encoded image must have the same amount of columns, after all.
        uint32 rawWidthColumnCount = ( expRawWidth / rawColumnWidth );
        uint32 rawHeightColumnCount = ( expRawHeight / rawColumnHeight );

        // Get the encoding properties of the target data.
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotPackedDimms = getEncodingFormatDimensions(packedFormat, packedColumnWidth, packedColumnHeight);

        if ( gotPackedDimms == false )
        {
            return false;
        }

        uint32 packedDepth = getFormatEncodingDepth(packedFormat);

        // Get the dimensions of the packed format.
        // Since it has to have the same amount of columns, we multiply those column
        // dimensions with the column counts.
        uint32 packedWidth = ( rawWidthColumnCount * packedColumnWidth );
        uint32 packedHeight = ( rawHeightColumnCount * packedColumnHeight );

        if ( rawFormat != packedFormat )
        {
            // Get permutation dimensions.
            uint32 permWidth, permHeight;

            eFormatEncodingType permFormat = FORMAT_UNKNOWN;

            if ( rawDepth < packedDepth )
            {
                permFormat = rawFormat;
            }
            else
            {
                permFormat = packedFormat;
            }
        
            bool gotPermDimms = getPermutationDimensions(permFormat, permWidth, permHeight);

            if ( gotPermDimms == false )
            {
                return false;
            }

            // Get the amount of raw texels that can be put into one packed texel.
            uint32 permutationStride;

            bool isPack = isPackOperation( rawFormat, packedFormat );

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

};