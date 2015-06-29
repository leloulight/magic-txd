namespace rw
{

AINLINE bool getpaletteindex(
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

AINLINE bool browsetexelcolor(
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

AINLINE uint8 scalecolor(uint8 color, uint32 curMax, uint32 newMax)
{
    return (uint8)( (double)color / (double)curMax * (double)newMax );
}

AINLINE bool puttexelcolor(
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

template <typename texel_t>
struct colorModelDispatcher
{
    // TODO: make every color request through this struct.

    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    uint32 depth;

    texel_t *texelSource;

    const void *paletteData;
    uint32 paletteSize;
    ePaletteType paletteType;

    eColorModel usedColorModel;

    AINLINE colorModelDispatcher( texel_t *texelSource, eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, const void *paletteData, uint32 paletteSize, ePaletteType paletteType )
    {
        this->rasterFormat = rasterFormat;
        this->colorOrder = colorOrder;
        this->depth = depth;

        this->texelSource = texelSource;
       
        this->paletteData = paletteData;
        this->paletteSize = paletteSize;
        this->paletteType = paletteType;

        // Determine the color model of our requests.
        if ( rasterFormat == RASTER_1555 ||
             rasterFormat == RASTER_565 ||
             rasterFormat == RASTER_4444 ||
             rasterFormat == RASTER_8888 ||
             rasterFormat == RASTER_888 ||
             rasterFormat == RASTER_555 )
        {
            usedColorModel = COLORMODEL_RGBA;
        }
        else if ( rasterFormat == RASTER_LUM8 )
        {
            usedColorModel = COLORMODEL_LUMINANCE;
        }
        else if ( rasterFormat == RASTER_16 ||
                  rasterFormat == RASTER_24 ||
                  rasterFormat == RASTER_32 )
        {
            usedColorModel = COLORMODEL_DEPTH;
        }
        else
        {
            throw RwException( "unknown color model for color model dispatcher" );
        }
    }

    AINLINE eColorModel getColorModel( void ) const
    {
        return this->usedColorModel;
    }

    AINLINE bool getRGBA( unsigned int index, uint8& red, uint8& green, uint8& blue, uint8& alpha ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success =
                browsetexelcolor(
                    this->texelSource, this->paletteType, this->paletteData, this->paletteSize,
                    index,
                    this->rasterFormat, this->colorOrder, this->depth,
                    red, green, blue, alpha
                );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            uint8 lum;

            success = this->getLuminance( index, lum );

            if ( success )
            {
                red = lum;
                green = lum;
                blue = lum;
                alpha = lum;
            }
        }
        else
        {
            throw RwException( "tried to fetch RGBA from unsupported color model" );
        }

        return success;
    }

    AINLINE bool setRGBA( unsigned int index, uint8 red, uint8 green, uint8 blue, uint8 alpha ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            if ( this->paletteType != PALETTE_NONE )
            {
                throw RwException( "tried to set color to palette bitmap (unsupported)" );
            }

            success =
                puttexelcolor(
                    this->texelSource, index,
                    this->rasterFormat, this->colorOrder, this->depth,
                    red, green, blue, alpha
                );
        }
        else
        {
            throw RwException( "tried to set RGBA to unsupported color model" );
        }

        return success;
    }

    AINLINE bool setLuminance( unsigned int index, uint8 lum ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->setRGBA( index, lum, lum, lum, 255 );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            eRasterFormat rasterFormat = this->rasterFormat;
            uint32 depth = this->depth;
            
            const void *texelSource = this->texelSource;
            
            if ( rasterFormat == RASTER_LUM8 )
            {
                if ( depth == 8 )
                {
                    struct pixel_t
                    {
                        uint8 lum;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    srcData->lum = lum;

                    success = true;
                }
            }
        }
        else
        {
            throw RwException( "tried to set luminance to unsupported color model" );
        }

        return success;
    }

    AINLINE bool getLuminance( unsigned int index, uint8& lum ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_LUMINANCE )
        {
            eRasterFormat rasterFormat = this->rasterFormat;
            uint32 depth = this->depth;
            
            const void *texelSource = this->texelSource;
            
            if ( rasterFormat == RASTER_LUM8 )
            {
                if ( depth == 8 )
                {
                    struct pixel_t
                    {
                        uint8 lum;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    lum = srcData->lum;

                    success = true;
                }
            }
        }
        else
        {
            throw RwException( "tried to get luminance from unsupported color model" );
        }

        return success;
    }

    AINLINE void setColor( unsigned int index, const abstractColorItem& colorItem ) const
    {
        eColorModel model = colorItem.model;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->setRGBA( index, colorItem.rgbaColor.r, colorItem.rgbaColor.g, colorItem.rgbaColor.b, colorItem.rgbaColor.a );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            success = this->setLuminance( index, colorItem.lumColor );
        }
        else
        {
            throw RwException( "invalid color model in abstract color item" );
        }
    }

    AINLINE void getColor( unsigned int index, abstractColorItem& colorItem ) const
    {
        eColorModel model = this->usedColorModel;

        colorItem.model = model;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->getRGBA( index, colorItem.rgbaColor.r, colorItem.rgbaColor.g, colorItem.rgbaColor.b, colorItem.rgbaColor.a );

            if ( !success )
            {
                colorItem.rgbaColor.r = 0;
                colorItem.rgbaColor.g = 0;
                colorItem.rgbaColor.b = 0;
                colorItem.rgbaColor.a = 0;
            }
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            success = this->getLuminance( index, colorItem.lumColor );

            if ( !success )
            {
                colorItem.lumColor = 0;
            }
        }
        else
        {
            throw RwException( "invalid color model for getting abstract color item" );
        }
    }

    AINLINE void clearColor( unsigned int index )
    {
        // TODO.
        this->setLuminance( index, 0 );
    }
};

inline double unpackcolor( uint8 color )
{
    return ( (double)color / 255.0 );
}

inline uint8 packcolor( double color )
{
    return (uint8)( color * 255.0 );
}

inline bool mipmapCalculateHasAlpha(
    uint32 layerWidth, uint32 layerHeight, uint32 mipWidth, uint32 mipHeight, const void *texelSource, uint32 texelDataSize,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize
)
{
    bool hasAlpha = false;

    // Decide whether we even can have alpha.
    // Otherwise there is no point in going through the pixels.
    if (rasterFormat == RASTER_1555 || rasterFormat == RASTER_4444 || rasterFormat == RASTER_8888)
    {
        // Alright, the raster can have alpha.
        // If we are palettized, we can just check the palette colors.
        if (paletteType != PALETTE_NONE)
        {
            // Determine whether we REALLY use all palette indice.
            uint32 palItemCount = paletteSize;

            bool *usageFlags = new bool[ palItemCount ];

            for ( uint32 n = 0; n < palItemCount; n++ )
            {
                usageFlags[ n ] = false;
            }

            // Loop through all pixels of the image.
            {
                uint32 texWidth = mipWidth;
                uint32 texHeight = mipHeight;

                uint32 texItemCount = ( texWidth * texHeight );

                const void *texelData = texelSource;

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

            const void *palColorSource = paletteData;

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
            uint32 imageItemCount = ( mipWidth * mipHeight );

            for (uint32 n = 0; n < imageItemCount; n++)
            {
                uint8 r, g, b, a;

                bool hasColor = browsetexelcolor(texelSource, PALETTE_NONE, NULL, 0, n, rasterFormat, colorOrder, depth, r, g, b, a);

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

inline bool calculateHasAlpha( const pixelDataTraversal& pixelData )
{
    assert( pixelData.compressionType == RWCOMPRESS_NONE );

    const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

    // We assume that the first mipmap shares the same qualities like any other mipmap.
    // It is the base layer after all.
    return mipmapCalculateHasAlpha(
        mipLayer.mipWidth, mipLayer.mipHeight, mipLayer.width, mipLayer.height, mipLayer.texels, mipLayer.dataSize,
        pixelData.rasterFormat, pixelData.depth, pixelData.colorOrder,
        pixelData.paletteType, pixelData.paletteData, pixelData.paletteSize
    );
}

};