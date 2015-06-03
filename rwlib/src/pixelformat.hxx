namespace rw
{

static inline bool getpaletteindex(
    const void *texelSource, ePaletteType paletteType, uint32 maxpalette, uint32 itemDepth, uint32 colorIndex,
    uint8& paletteIndexOut
)
{
    // Get the color lookup index from the texel.
    uint8 paletteIndex;

    bool couldGetIndex = false;

    if (paletteType == PALETTE_4BIT)
    {
        if (itemDepth == 4)
        {
            PixelFormat::palette4bit *srcData = (PixelFormat::palette4bit*)texelSource;

            srcData->getvalue(colorIndex, paletteIndex);

            couldGetIndex = true;
        }
        else if (itemDepth == 8)
        {
            PixelFormat::palette8bit *srcData = (PixelFormat::palette8bit*)texelSource;

            srcData->getvalue(colorIndex, paletteIndex);

            // Trim off unused bits.
            paletteIndex &= 0xF;

            couldGetIndex = true;
        }
    }
    else if (paletteType == PALETTE_8BIT)
    {
        if (itemDepth == 8)
        {
            PixelFormat::palette8bit *srcData = (PixelFormat::palette8bit*)texelSource;

            srcData->getvalue(colorIndex, paletteIndex);

            couldGetIndex = true;
        }
    }

    bool couldResolveSource = false;

    if (couldGetIndex && paletteIndex < maxpalette)
    {
        couldResolveSource = true;

        paletteIndexOut = paletteIndex;
    }

    return couldResolveSource;
}

static inline bool browsetexelcolor(
    const void *texelSource, ePaletteType paletteType, const void *paletteData, uint32 maxpalette,
    uint32 colorIndex, eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 itemDepth,
    uint8& red, uint8& green, uint8& blue, uint8& alpha)
{
    typedef PixelFormat::texeltemplate <PixelFormat::pixeldata32bit> pixel32_t;

    bool hasColor = false;

    const void *realTexelSource = NULL;
    uint32 realColorIndex = 0;
    uint32 realColorDepth = 0;

    bool couldResolveSource = false;

    uint8 prered, pregreen, preblue, prealpha;

    if (paletteType == PALETTE_4BIT || paletteType == PALETTE_8BIT)
    {
        realTexelSource = paletteData;

        uint8 paletteIndex;

        bool couldResolvePalIndex = getpaletteindex(texelSource, paletteType, maxpalette, itemDepth, colorIndex, paletteIndex);

        if (couldResolvePalIndex)
        {
            realColorIndex = paletteIndex;
            realColorDepth = Bitmap::getRasterFormatDepth(rasterFormat);

            couldResolveSource = true;
        }
    }
    else
    {
        realTexelSource = texelSource;
        realColorIndex = colorIndex;
        realColorDepth = itemDepth;

        couldResolveSource = true;
    }

    if ( !couldResolveSource )
        return false;

    if (rasterFormat == RASTER_1555)
    {
        if (realColorDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 5;
                uint16 green : 5;
                uint16 blue : 5;
                uint16 alpha : 1;
            };

            typedef PixelFormat::texeltemplate <pixel_t> pixel1555_t;

            pixel1555_t *srcData = (pixel1555_t*)realTexelSource;

            srcData->getcolor(realColorIndex, prered, pregreen, preblue, prealpha);

            // Scale the color values.
            prered      = (uint32)prered * 0xFF / 0x1F;
            pregreen    = (uint32)pregreen * 0xFF / 0x1F;
            preblue     = (uint32)preblue * 0xFF / 0x1F;
            prealpha    = 0xFF;

            hasColor = true;
        }
    }
    else if (rasterFormat == RASTER_565)
    {
        if (realColorDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 5;
                uint16 green : 6;
                uint16 blue : 5;
            };

            pixel_t *srcData = ( (pixel_t*)realTexelSource + realColorIndex );

            prered = srcData->red;
            pregreen = srcData->green;
            preblue = srcData->blue;

            // Scale the color values.
            prered      = (uint32)prered * 0xFF / 0x1F;
            pregreen    = (uint32)pregreen * 0xFF / 0x3F;
            preblue     = (uint32)preblue * 0xFF / 0x1F;
            prealpha    = 0xFF;

            hasColor = true;
        }
    }
    else if (rasterFormat == RASTER_4444)
    {
        if (realColorDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 4;
                uint16 green : 4;
                uint16 blue : 4;
                uint16 alpha : 4;
            };

            typedef PixelFormat::texeltemplate <pixel_t> pixel4444_t;

            pixel4444_t *srcData = (pixel4444_t*)realTexelSource;

            srcData->getcolor(realColorIndex, prered, pregreen, preblue, prealpha);

            // Scale the color values.
            prered      = (uint32)prered * 0xFF / 0xF;
            pregreen    = (uint32)pregreen * 0xFF / 0xF;
            preblue     = (uint32)preblue * 0xFF / 0xF;
            prealpha    = (uint32)prealpha * 0xFF / 0xF;

            hasColor = true;
        }
    }
    else if (rasterFormat == RASTER_8888)
    {
        if (realColorDepth == 32)
        {
            pixel32_t *srcData = (pixel32_t*)realTexelSource;

            srcData->getcolor(realColorIndex, prered, pregreen, preblue, prealpha);

            hasColor = true;
        }
    }
    else if (rasterFormat == RASTER_888)
    {
        if (realColorDepth == 32)
        {
            struct pixel_t
            {
                uint8 red;
                uint8 green;
                uint8 blue;
                uint8 unused;
            };

            pixel_t *srcData = ( (pixel_t*)realTexelSource + realColorIndex );

            // Get the color values.
            prered = srcData->red;
            pregreen = srcData->green;
            preblue = srcData->blue;
            prealpha = 0xFF;

            hasColor = true;
        }
        else if (realColorDepth == 24)
        {
            struct pixel_t
            {
                uint8 red;
                uint8 green;
                uint8 blue;
            };

            pixel_t *srcData = ( (pixel_t*)realTexelSource + realColorIndex );

            // Get the color values.
            prered = srcData->red;
            pregreen = srcData->green;
            preblue = srcData->blue;
            prealpha = 0xFF;

            hasColor = true;
        }
    }

    if ( hasColor )
    {
        // Respect color ordering.
        if ( colorOrder == COLOR_RGBA )
        {
            red = prered;
            green = pregreen;
            blue = preblue;
            alpha = prealpha;
        }
        else if ( colorOrder == COLOR_BGRA )
        {
            red = preblue;
            green = pregreen;
            blue = prered;
            alpha = prealpha;
        }
        else if ( colorOrder == COLOR_ABGR )
        {
            red = prealpha;
            green = preblue;
            blue = pregreen;
            alpha = prered;
        }
        else
        {
            assert( 0 );
        }
    }

    return hasColor;
}

static inline uint8 scalecolor(uint8 color, uint32 curMax, uint32 newMax)
{
    return (uint8)( (double)color / (double)curMax * (double)newMax );
}

static inline bool puttexelcolor(
    void *texelDest,
    uint32 colorIndex, eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 itemDepth,
    uint8 red, uint8 green, uint8 blue, uint8 alpha
)
{
    typedef PixelFormat::texeltemplate <PixelFormat::pixeldata32bit> pixel32_t;

    bool setColor = false;

    uint8 putred, putgreen, putblue, putalpha;

    // Respect color ordering.
    if ( colorOrder == COLOR_RGBA )
    {
        putred = red;
        putgreen = green;
        putblue = blue;
        putalpha = alpha;
    }
    else if ( colorOrder == COLOR_BGRA )
    {
        putred = blue;
        putgreen = green;
        putblue = red;
        putalpha = alpha;
    }
    else if ( colorOrder == COLOR_ABGR )
    {
        putred = alpha;
        putgreen = blue;
        putblue = green;
        putalpha = red;
    }
    else
    {
        assert( 0 );
    }

    if (rasterFormat == RASTER_1555)
    {
        if (itemDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 5;
                uint16 green : 5;
                uint16 blue : 5;
                uint16 alpha : 1;
            };

            typedef PixelFormat::texeltemplate <pixel_t> pixel1555_t;

            pixel1555_t *dstData = (pixel1555_t*)texelDest;

            // Scale the color values.
            uint8 redScaled =       scalecolor(putred, 255, 31);
            uint8 greenScaled =     scalecolor(putgreen, 255, 31);
            uint8 blueScaled =      scalecolor(putblue, 255, 31);
            uint8 alphaScaled =     ( putalpha != 0 ) ? ( 1 ) : ( 0 );

            dstData->setcolor(colorIndex, redScaled, greenScaled, blueScaled, alphaScaled);

            setColor = true;
        }
    }
    else if (rasterFormat == RASTER_565)
    {
        if (itemDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 5;
                uint16 green : 6;
                uint16 blue : 5;
            };

            pixel_t *dstData = ( (pixel_t*)texelDest + colorIndex );

            // Scale the color values.
            uint8 redScaled =       scalecolor(putred, 255, 31);
            uint8 greenScaled =     scalecolor(putgreen, 255, 63);
            uint8 blueScaled =      scalecolor(putblue, 255, 31);

            dstData->red = redScaled;
            dstData->green = greenScaled;
            dstData->blue = blueScaled;

            setColor = true;
        }
    }
    else if (rasterFormat == RASTER_4444)
    {
        if (itemDepth == 16)
        {
            struct pixel_t
            {
                uint16 red : 4;
                uint16 green : 4;
                uint16 blue : 4;
                uint16 alpha : 4;
            };

            typedef PixelFormat::texeltemplate <pixel_t> pixel4444_t;

            pixel4444_t *dstData = (pixel4444_t*)texelDest;

            // Scale the color values.
            uint8 redScaled =       scalecolor(putred, 255, 15);
            uint8 greenScaled =     scalecolor(putgreen, 255, 15);
            uint8 blueScaled =      scalecolor(putblue, 255, 15);
            uint8 alphaScaled =     scalecolor(putalpha, 255, 15);

            dstData->setcolor(colorIndex, redScaled, greenScaled, blueScaled, alphaScaled);

            setColor = true;
        }
    }
    else if (rasterFormat == RASTER_8888)
    {
        if (itemDepth == 32)
        {
            pixel32_t *dstData = (pixel32_t*)texelDest;

            dstData->setcolor(colorIndex, putred, putgreen, putblue, putalpha);

            setColor = true;
        }
    }
    else if (rasterFormat == RASTER_888)
    {
        if (itemDepth == 32)
        {
            struct pixel_t
            {
                uint8 red;
                uint8 green;
                uint8 blue;
                uint8 unused;
            };

            pixel_t *dstData = ( (pixel_t*)texelDest + colorIndex );

            // Put the color values.
            dstData->red = putred;
            dstData->green = putgreen;
            dstData->blue = putblue;

            setColor = true;
        }
        else if (itemDepth == 24)
        {
            struct pixel_t
            {
                uint8 red;
                uint8 green;
                uint8 blue;
            };

            pixel_t *dstData = ( (pixel_t*)texelDest + colorIndex );

            // Put the color values.
            dstData->red = putred;
            dstData->green = putgreen;
            dstData->blue = putblue;

            setColor = true;
        }
    }

    return setColor;
}

inline double unpackcolor( uint8 color )
{
    return ( (double)color / 255.0 );
}

inline uint8 packcolor( double color )
{
    return (uint8)( color * 255.0 );
}

inline bool calculateHasAlpha( const pixelDataTraversal& pixelData )
{
    assert( pixelData.compressionType == RWCOMPRESS_NONE );

    bool hasAlpha = false;

    // Decide whether we even can have alpha.
    // Otherwise there is no point in going through the pixels.
    eRasterFormat rasterFormat = pixelData.rasterFormat;
    ePaletteType paletteType = pixelData.paletteType;
    eColorOrdering colorOrder = pixelData.colorOrder;

    if (rasterFormat == RASTER_1555 || rasterFormat == RASTER_4444 || rasterFormat == RASTER_8888)
    {
        // Alright, the raster can have alpha.
        // If we are palettized, we can just check the palette colors.
        if (paletteType != PALETTE_NONE)
        {
            // Determine whether we REALLY use all palette indice.
            uint32 palItemCount = pixelData.paletteSize;

            bool *usageFlags = new bool[ palItemCount ];

            for ( uint32 n = 0; n < palItemCount; n++ )
            {
                usageFlags[ n ] = false;
            }

            // Loop through all pixels of the image.
            {
                const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

                uint32 texWidth = mipLayer.width;
                uint32 texHeight = mipLayer.height;

                uint32 texItemCount = ( texWidth * texHeight );

                const void *texelData = mipLayer.texels;

                uint32 depth = pixelData.depth;

                for ( uint32 n = 0; n < texItemCount; n++ )
                {
                    uint8 palIndex;

                    bool hasIndex = getpaletteindex(texelData, paletteType, palItemCount, depth, n, palIndex);

                    if ( hasIndex && palIndex < palItemCount )
                    {
                        usageFlags[ palIndex ] = true;
                    }
                }
            }

            const void *palColorSource = pixelData.paletteData;

            uint32 palFormatDepth = Bitmap::getRasterFormatDepth(rasterFormat);

            for (uint32 n = 0; n < palItemCount; n++)
            {
                if ( usageFlags[ n ] == true )
                {
                    uint8 r, g, b, a;

                    bool hasColor = browsetexelcolor(palColorSource, PALETTE_NONE, NULL, 0, n, rasterFormat, colorOrder, palFormatDepth, r, g, b, a);

                    if (hasColor && a != 255)
                    {
                        hasAlpha = true;
                        break;
                    }
                }
            }

            // Free memory.
            delete [] usageFlags;
        }
        else
        {
            // We have to process the entire image. Oh boy.
            // For that, we decide based on the main raster only.
            const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

            const void *texelSource = mipLayer.texels;

            uint32 mipWidth = mipLayer.width;
            uint32 mipHeight = mipLayer.height;

            uint32 mipDepth = pixelData.depth;

            uint32 imageItemCount = ( mipWidth * mipHeight );

            for (uint32 n = 0; n < imageItemCount; n++)
            {
                uint8 r, g, b, a;

                bool hasColor = browsetexelcolor(texelSource, PALETTE_NONE, NULL, 0, n, rasterFormat, colorOrder, mipDepth, r, g, b, a);

                if (hasColor && a != 255)
                {
                    hasAlpha = true;
                    break;
                }
            }
        }
    }

    return hasAlpha;
}

};