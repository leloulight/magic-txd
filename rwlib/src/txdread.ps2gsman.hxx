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
        const void *srcToBePermuted, uint32 srcWidth, uint32 srcHeight, uint32 srcDepth, uint32 srcColumnWidth, uint32 srcColumnHeight,
        void *dstTexels, uint32 dstWidth, uint32 dstHeight, uint32 dstDepth, uint32 dstColumnWidth, uint32 dstColumnHeight,
        uint32 colsWidth, uint32 colsHeight,
        const uint32 *permutationData_primCol, const uint32 *permutationData_secCol, uint32 permWidth, uint32 permHeight,
        uint32 permutationStride, uint32 permHoriSplit,
        bool revert
        )
    {
        // Get the dimensions of a column as expressed in units of the permutation format.
        uint32 permProcessColumnWidth = 0;
        uint32 permProcessColumnHeight = 0;

        uint32 permIterWidth = 0;
        uint32 permIterHeight = 0;

        uint32 permSourceWidth = 0;
        uint32 permSourceHeight = 0;

        uint32 packedTargetWidth = 0;
        uint32 packedTargetHeight = 0;

        uint32 permItemDepth = 0;

        if ( !revert )
        {
            permIterWidth = srcColumnWidth;
            permIterHeight = srcColumnHeight;

            permProcessColumnWidth = dstColumnWidth;
            permProcessColumnHeight = dstColumnHeight;

            permItemDepth = srcDepth;

            permSourceWidth = srcWidth;
            permSourceHeight = srcHeight;

            packedTargetWidth = dstWidth;
            packedTargetHeight = dstHeight;
        }
        else
        {
            permIterWidth = dstColumnWidth;
            permIterHeight = dstColumnHeight;

            permProcessColumnWidth = srcColumnWidth;
            permProcessColumnHeight = srcColumnHeight;

            permItemDepth = dstDepth;

            permSourceWidth = dstWidth;
            permSourceHeight = dstHeight;

            packedTargetWidth = srcWidth;
            packedTargetHeight = srcHeight;
        }

        uint32 packedTransformedColumnWidth = ( permProcessColumnWidth * permutationStride ) / permHoriSplit;
        uint32 packedTransformedColumnHeight = ( permProcessColumnHeight );

        colsWidth *= permHoriSplit;

        // Get the stride through the packed data in raw format.
        uint32 packedTransformedStride = ( packedTargetWidth * permutationStride );

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
                            // Get the array index into the arrays.
                            uint32 srcArrayIndex;
                            uint32 targetArrayIndex;

                            if ( !revert )
                            {
                                srcArrayIndex = ( source_pixel_yOff * permSourceWidth + source_pixel_xOff );

                                targetArrayIndex = ( target_pixel_yOff * packedTransformedStride + target_pixel_xOff );
                            }
                            else
                            {
                                targetArrayIndex = ( source_pixel_yOff * permSourceWidth + source_pixel_xOff );

                                srcArrayIndex = ( target_pixel_yOff * packedTransformedStride + target_pixel_xOff );
                            }

                            // Perform the texel movement.
                            if (permItemDepth == 4)
                            {
                                PixelFormat::palette4bit *srcData = (PixelFormat::palette4bit*)srcToBePermuted;

                                // Get the src item.
                                PixelFormat::palette4bit::trav_t travItem;

                                srcData->getvalue(srcArrayIndex, travItem);

                                // Put the dst item.
                                PixelFormat::palette4bit *dstData = (PixelFormat::palette4bit*)dstTexels;

                                dstData->setvalue(targetArrayIndex, travItem);
                            }
                            else if (permItemDepth == 8)
                            {
                                // Get the src item.
                                PixelFormat::palette8bit *srcData = (PixelFormat::palette8bit*)srcToBePermuted;

                                PixelFormat::palette8bit::trav_t travItem;

                                srcData->getvalue(srcArrayIndex, travItem);

                                // Put the dst item.
                                PixelFormat::palette8bit *dstData = (PixelFormat::palette8bit*)dstTexels;

                                dstData->setvalue(targetArrayIndex, travItem);
                            }
                            else if (permItemDepth == 16)
                            {
                                typedef PixelFormat::typedcolor <uint16> theColor;

                                // Get the src item.
                                theColor *srcData = (theColor*)srcToBePermuted;

                                theColor::trav_t travItem;

                                srcData->getvalue(srcArrayIndex, travItem);

                                // Put the dst item.
                                theColor *dstData = (theColor*)dstTexels;

                                dstData->setvalue(targetArrayIndex, travItem);
                            }
                            else if (permItemDepth == 24)
                            {
                                struct colorStruct
                                {
                                    uint8 x, y, z;
                                };

                                typedef PixelFormat::typedcolor <colorStruct> theColor;

                                // Get the src item.
                                theColor *srcData = (theColor*)srcToBePermuted;

                                theColor::trav_t travItem;

                                srcData->getvalue(srcArrayIndex, travItem);

                                // Put the dst item.
                                theColor *dstData = (theColor*)dstTexels;

                                dstData->setvalue(targetArrayIndex, travItem);
                            }
                            else if (permItemDepth == 32)
                            {
                                typedef PixelFormat::typedcolor <uint32> theColor;

                                // Get the src item.
                                theColor *srcData = (theColor*)srcToBePermuted;

                                theColor::trav_t travItem;

                                srcData->getvalue(srcArrayIndex, travItem);

                                // Put the dst item.
                                theColor *dstData = (theColor*)dstTexels;

                                dstData->setvalue(targetArrayIndex, travItem);
                            }
                            else
                            {
                                assert(0);
                            }
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

    inline static void* unpackImageData(
        Interface *engineInterface,
        eFormatEncodingType packedFormat, eFormatEncodingType rawFormat,
        uint32 rawItemDepth,
        const void *srcToBeDecoded, uint32 packedWidth, uint32 packedHeight,
        uint32& dstDataSizeOut, uint32 dstWidth, uint32 dstHeight
    )
    {
        assert(rawFormat != FORMAT_UNKNOWN);
        assert(packedFormat != FORMAT_UNKNOWN);

        assert(rawFormat != packedFormat);

        // Get the encoding properties of the source data.
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotPackedDimms = getEncodingFormatDimensions(packedFormat, packedColumnWidth, packedColumnHeight);

        assert( gotPackedDimms == true );

        uint32 packedDepth = getFormatEncodingDepth(packedFormat);

        // Make sure the packed texture is a multiple of the column dimensions.
        assert((packedWidth % packedColumnWidth) == 0);
        assert((packedHeight % packedColumnHeight) == 0);

        // Get the amount of columns from the packed image.
        // This is how much memory the memory space sets available to us.
        uint32 packedWidthColumnCount = ( packedWidth / packedColumnWidth );
        uint32 packedHeightColumnCount = ( packedHeight / packedColumnHeight );

        // Get the encoding properties of the target data.
        uint32 rawColumnWidth, rawColumnHeight;

        bool gotRawDimms = getEncodingFormatDimensions(rawFormat, rawColumnWidth, rawColumnHeight);

        assert( gotRawDimms == true );

        uint32 rawDepth = rawItemDepth;//getFormatEncodingDepth(rawFormat);

        // Calculate the size of the destination data.
        uint32 dstDataSize = getRasterDataSize( dstWidth * dstHeight, rawDepth );

        if ( dstDataSize == 0 )
            return NULL;

        // Allocate a buffer for the new permutation data.
        void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

        if ( dstTexels )
        {
            // Determine the encoding permutation.
            const uint32 *permutationData_primCol = NULL;
            const uint32 *permutationData_secCol = NULL;

            uint32 permWidth = 0;
            uint32 permHeight = 0;

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

                getPermutationDimensions(rawFormat, permWidth, permHeight);
            }

            if ( permutationData_primCol != NULL && permutationData_secCol != NULL )
            {
                // Get the amount of raw texels that can be put into one packed texel.
                uint32 permutationStride = ( packedDepth / rawDepth );

                // Get the horizontal split count.
                uint32 permHoriSplit = ( permutationStride / permWidth );

                // Execute the permutation engine.
                permuteArray(
                    srcToBeDecoded, packedWidth, packedHeight, packedDepth, packedColumnWidth, packedColumnHeight,
                    dstTexels, dstWidth, dstHeight, rawDepth, rawColumnWidth, rawColumnHeight,
                    packedWidthColumnCount, packedHeightColumnCount,
                    permutationData_primCol, permutationData_secCol, permWidth, permHeight,
                    permutationStride, permHoriSplit,
                    true
                );

                // Write the destination properties.
                dstDataSizeOut = dstDataSize;
            }
            else
            {
                engineInterface->PixelFree( dstTexels );

                dstTexels = NULL;
            }
        }

        return dstTexels;
    }

    inline static bool getPackedFormatDimensions(
        eFormatEncodingType rawFormat, eFormatEncodingType packedFormat,
        uint32 rawWidth, uint32 rawHeight,
        uint32& packedWidthOut, uint32& packedHeightOut
    )
    {
        if ( rawFormat == packedFormat )
        {
            packedWidthOut = rawWidth;
            packedHeightOut = rawHeight;
            return true;
        }

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

        if ( rawDepth < packedDepth )
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
        if ( rawDepth < packedDepth )
        {
            packedWidthOut = ( packedWidth / permHoriSplit );
        }
        else
        {
            packedWidthOut = ( packedWidth * permHoriSplit );
        }
        packedHeightOut = packedHeight;

        // Make sure we align the packed coordinates.
        packedWidthOut = ALIGN_SIZE( packedWidthOut, packedColumnWidth );
        packedHeightOut = ALIGN_SIZE( packedHeightOut, packedColumnHeight );

        return true;
    }

    inline static void* packImageData(
        Interface *engineInterface,
        eFormatEncodingType rawFormat, eFormatEncodingType packedFormat,
        uint32 rawItemDepth,
        const void *srcToBeEncoded, uint32 srcWidth, uint32 srcHeight,
        uint32& dstDataSizeOut, uint32& dstWidthOut, uint32& dstHeightOut
    )
    {
        assert(rawFormat != FORMAT_UNKNOWN);
        assert(packedFormat != FORMAT_UNKNOWN);

        assert(rawFormat != packedFormat);

        // Get the encoding properties of the source data.
        uint32 rawColumnWidth, rawColumnHeight;

        bool gotRawDimms = getEncodingFormatDimensions(rawFormat, rawColumnWidth, rawColumnHeight);

        assert( gotRawDimms == true );

        uint32 rawDepth = rawItemDepth;//getFormatEncodingDepth(rawFormat);

        // Make sure the raw texture is a multiple of the column dimensions.
        uint32 expSrcWidth = ALIGN_SIZE( srcWidth, rawColumnWidth );
        uint32 expSrcHeight = ALIGN_SIZE( srcHeight, rawColumnHeight );

        // Get the amount of columns from our source image.
        // The encoded image must have the same amount of columns, after all.
        uint32 rawWidthColumnCount = ( expSrcWidth / rawColumnWidth );
        uint32 rawHeightColumnCount = ( expSrcHeight / rawColumnHeight );

        // Get the encoding properties of the target data.
        uint32 packedColumnWidth, packedColumnHeight;

        bool gotPackedDimms = getEncodingFormatDimensions(packedFormat, packedColumnWidth, packedColumnHeight);

        assert( gotPackedDimms == true );

        uint32 packedDepth = getFormatEncodingDepth(packedFormat);

        // Get permutation info.
        uint32 permWidth = 0;
        uint32 permHeight = 0;

        bool gotPermDimms = getPermutationDimensions(rawFormat, permWidth, permHeight);

        assert( gotPermDimms == true );

        // Get the amount of raw texels that can be put into one packed texel.
        uint32 permutationStride = ( packedDepth / rawDepth );

        // Get the horizontal split count.
        uint32 permHoriSplit = ( permutationStride / permWidth );

        // Get the dimensions of the packed format.
        // Since it has to have the same amount of columns, we multiply those column
        // dimensions with the column counts.
        uint32 packedWidth = ALIGN_SIZE( ( rawWidthColumnCount * packedColumnWidth ) / permHoriSplit, packedColumnWidth );
        uint32 packedHeight = ( rawHeightColumnCount * packedColumnHeight );

        // Calculate the size of the destination data.
        uint32 dstDataSize = getRasterDataSize( packedWidth * packedHeight, packedDepth );

        if ( dstDataSize == 0 )
            return NULL;

        // Allocate a buffer for the new permutation data.
        void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

        if ( dstTexels )
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

            if ( permutationData_primCol != NULL && permutationData_secCol != NULL )
            {
                // Execute the permutation engine.
                permuteArray(
                    srcToBeEncoded, srcWidth, srcHeight, rawDepth, rawColumnWidth, rawColumnHeight,
                    dstTexels, packedWidth, packedHeight, packedDepth, packedColumnWidth, packedColumnHeight,
                    rawWidthColumnCount, rawHeightColumnCount,
                    permutationData_primCol, permutationData_secCol, permWidth, permHeight,
                    permutationStride, permHoriSplit,
                    false
                );

                // Write the destination properties.
                dstDataSizeOut = dstDataSize;

                dstWidthOut = packedWidth;
                dstHeightOut = packedHeight;
            }
            else
            {
                engineInterface->PixelFree( dstTexels );

                dstTexels = NULL;
            }
        }

        return dstTexels;
    }
};

};