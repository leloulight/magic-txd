#ifndef _PIXELFORMAT_INTERNAL_INCLUDE_
#define _PIXELFORMAT_INTERNAL_INCLUDE_

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

    if (paletteType == PALETTE_4BIT ||
        paletteType == PALETTE_4BIT_LSB)
    {
        if (itemDepth == 4)
        {
            if (paletteType == PALETTE_4BIT_LSB)
            {
                PixelFormat::palette4bit_lsb *srcData = (PixelFormat::palette4bit_lsb*)texelSource;

                srcData->getvalue(colorIndex, paletteIndex);

                couldGetIndex = true;
            }
            else
            {
                PixelFormat::palette4bit *srcData = (PixelFormat::palette4bit*)texelSource;

                srcData->getvalue(colorIndex, paletteIndex);

                couldGetIndex = true;
            }
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

AINLINE void setpaletteindex(
    void *dstTexels, uint32 itemIndex, uint32 dstDepth, ePaletteType dstPaletteType,
    uint8 palIndex
)
{
    if ( dstDepth == 4 )
    {
        if ( dstPaletteType == PALETTE_4BIT )
        {
            ( (PixelFormat::palette4bit*)dstTexels )->setvalue(itemIndex, palIndex);
        }
        else if ( dstPaletteType == PALETTE_4BIT_LSB )
        {
            ( (PixelFormat::palette4bit_lsb*)dstTexels )->setvalue(itemIndex, palIndex);
        }
        else
        {
            assert( 0 );
        }
    }
    else if ( dstDepth == 8 )
    {
        ( (PixelFormat::palette8bit*)dstTexels )->setvalue(itemIndex, palIndex);
    }
    else
    {
        assert( 0 );
    }
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

    if (paletteType != PALETTE_NONE)
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

inline eColorModel getColorModelFromRasterFormat( eRasterFormat rasterFormat )
{
    eColorModel usedColorModel;

    if ( rasterFormat == RASTER_1555 ||
            rasterFormat == RASTER_565 ||
            rasterFormat == RASTER_4444 ||
            rasterFormat == RASTER_8888 ||
            rasterFormat == RASTER_888 ||
            rasterFormat == RASTER_555 )
    {
        usedColorModel = COLORMODEL_RGBA;
    }
    else if ( rasterFormat == RASTER_LUM ||
              rasterFormat == RASTER_LUM_ALPHA )
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
        throw RwException( "unknown color model for raster format" );
    }

    return usedColorModel;
}

template <typename texel_t>
struct colorModelDispatcher
{
    // TODO: make every color request through this struct.

    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    uint32 depth;

    const void *paletteData;
    uint32 paletteSize;
    ePaletteType paletteType;

    eColorModel usedColorModel;

    AINLINE colorModelDispatcher( eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, const void *paletteData, uint32 paletteSize, ePaletteType paletteType )
    {
        this->rasterFormat = rasterFormat;
        this->colorOrder = colorOrder;
        this->depth = depth;

        this->paletteData = paletteData;
        this->paletteSize = paletteSize;
        this->paletteType = paletteType;

        // Determine the color model of our requests.
        this->usedColorModel = getColorModelFromRasterFormat( rasterFormat );
    }
    
private:
    AINLINE colorModelDispatcher( const colorModelDispatcher& right )
    {
        throw RwException( "cloning color model dispatcher makes no sense" );
    }

public:
    AINLINE eColorModel getColorModel( void ) const
    {
        return this->usedColorModel;
    }

    AINLINE bool getRGBA( texel_t *texelSource, unsigned int index, uint8& red, uint8& green, uint8& blue, uint8& alpha ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success =
                browsetexelcolor(
                    texelSource, this->paletteType, this->paletteData, this->paletteSize,
                    index,
                    this->rasterFormat, this->colorOrder, this->depth,
                    red, green, blue, alpha
                );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            uint8 lum, a;

            success = this->getLuminance( texelSource, index, lum, a );

            if ( success )
            {
                red = lum;
                green = lum;
                blue = lum;
                alpha = a;
            }
        }
        else
        {
            throw RwException( "tried to fetch RGBA from unsupported color model" );
        }

        return success;
    }

    AINLINE bool setRGBA( texel_t *texelSource, unsigned int index, uint8 red, uint8 green, uint8 blue, uint8 alpha ) const
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
                    texelSource, index,
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

    AINLINE bool setLuminance( texel_t *texelSource, unsigned int index, uint8 lum, uint8 alpha ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->setRGBA( texelSource, index, lum, lum, lum, 255 );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            eRasterFormat rasterFormat = this->rasterFormat;
            uint32 depth = this->depth;
            
            if ( rasterFormat == RASTER_LUM )
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
                else if ( depth == 4 )
                {
                    PixelFormat::palette4bit *lumData = (PixelFormat::palette4bit*)texelSource;

                    uint8 scaledLum = ( lum * 15 / 255 );

                    lumData->setvalue( index, scaledLum );

                    success = true;
                }
            }
            else if ( rasterFormat == RASTER_LUM_ALPHA )
            {
                if ( depth == 8 )
                {
                    struct pixel_t
                    {
                        uint8 lum : 4;
                        uint8 alpha : 4;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    srcData->lum = ( lum * 15 / 255 );
                    srcData->alpha = ( alpha * 15 / 255 );

                    success = true;
                }
                else if ( depth == 16 )
                {
                    struct pixel_t
                    {
                        uint8 lum;
                        uint8 alpha;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    srcData->lum = lum;
                    srcData->alpha = alpha;

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

    AINLINE bool getLuminance( texel_t *texelSource, unsigned int index, uint8& lum, uint8& alpha ) const
    {
        eColorModel model = this->usedColorModel;

        bool success = false;

        if ( model == COLORMODEL_LUMINANCE )
        {
            eRasterFormat rasterFormat = this->rasterFormat;
            uint32 depth = this->depth;
            
            if ( rasterFormat == RASTER_LUM )
            {
                if ( depth == 8 )
                {
                    struct pixel_t
                    {
                        uint8 lum;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    lum = srcData->lum;
                    alpha = 255;

                    success = true;
                }
                else if ( depth == 4 )
                {
                    PixelFormat::palette4bit *lumData = (PixelFormat::palette4bit*)texelSource;

                    uint8 scaledLum;

                    lumData->getvalue( index, scaledLum );

                    lum = ( scaledLum * 255 / 15 );
                    alpha = 255;

                    success = true;
                }
            }
            else if ( rasterFormat == RASTER_LUM_ALPHA )
            {
                if ( depth == 8 )
                {
                    struct pixel_t
                    {
                        uint8 lum : 4;
                        uint8 alpha : 4;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    lum = ( srcData->lum * 255 / 15 );
                    alpha = ( srcData->alpha * 255 / 15 );

                    success = true;
                }
                else if ( depth == 16 )
                {
                    struct pixel_t
                    {
                        uint8 lum;
                        uint8 alpha;
                    };

                    pixel_t *srcData = ( (pixel_t*)texelSource + index );

                    lum = srcData->lum;
                    alpha = srcData->alpha;
                }
            }
        }
        else
        {
            throw RwException( "tried to get luminance from unsupported color model" );
        }

        return success;
    }

    AINLINE void setColor( texel_t *texelSource, unsigned int index, const abstractColorItem& colorItem ) const
    {
        eColorModel model = colorItem.model;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->setRGBA( texelSource, index, colorItem.rgbaColor.r, colorItem.rgbaColor.g, colorItem.rgbaColor.b, colorItem.rgbaColor.a );
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            success = this->setLuminance( texelSource, index, colorItem.luminance.lum, colorItem.luminance.alpha );
        }
        else
        {
            throw RwException( "invalid color model in abstract color item" );
        }
    }

    AINLINE void getColor( texel_t *texelSource, unsigned int index, abstractColorItem& colorItem ) const
    {
        eColorModel model = this->usedColorModel;

        colorItem.model = model;

        bool success = false;

        if ( model == COLORMODEL_RGBA )
        {
            success = this->getRGBA( texelSource, index, colorItem.rgbaColor.r, colorItem.rgbaColor.g, colorItem.rgbaColor.b, colorItem.rgbaColor.a );

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
            success = this->getLuminance( texelSource, index, colorItem.luminance.lum, colorItem.luminance.alpha );

            if ( !success )
            {
                colorItem.luminance.lum = 0;
                colorItem.luminance.alpha = 0;
            }
        }
        else
        {
            throw RwException( "invalid color model for getting abstract color item" );
        }
    }

    AINLINE void clearColor( texel_t *texelSource, unsigned int index )
    {
        // TODO.
        this->setLuminance( texelSource, index, 0, 0 );
    }
};

template <typename srcColorDispatcher, typename dstColorDispatcher>
inline void copyTexelDataEx(
    const void *srcTexels, void *dstTexels,
    srcColorDispatcher& fetchDispatch, dstColorDispatcher& putDispatch,
    uint32 srcWidth, uint32 srcHeight,
    uint32 srcOffX, uint32 srcOffY,
    uint32 dstOffX, uint32 dstOffY,
    uint32 srcRowSize, uint32 dstRowSize
)
{
    // If we are not a palette, then we have to process colors.
    for ( uint32 row = 0; row < srcHeight; row++ )
    {
        const void *srcRow = getConstTexelDataRow( srcTexels, srcRowSize, row + srcOffY );
        void *dstRow = getTexelDataRow( dstTexels, dstRowSize, row + dstOffY );

        for ( uint32 col = 0; col < srcWidth; col++ )
        {
            abstractColorItem colorItem;

            fetchDispatch.getColor( srcRow, col + srcOffX, colorItem );

            // Just put the color inside.
            putDispatch.setColor( dstRow, col + dstOffX, colorItem );
        }
    }
}

// Move color items from one array position to another array at position.
AINLINE void moveTexels(
    const void *srcTexels, void *dstTexels,
    uint32 srcTexelX, uint32 srcTexelY, uint32 dstTexelX, uint32 dstTexelY,
    uint32 texelCountX, uint32 texelCountY,
    uint32 mipWidth, uint32 mipHeight,
    eRasterFormat srcRasterFormat, uint32 srcItemDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstItemDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, uint32 dstPaletteSize
)
{
    if ( srcPaletteType != PALETTE_NONE )
    {
        assert( dstPaletteType != PALETTE_NONE );

        // Move palette texels.
        ConvertPaletteDepthEx(
            srcTexels, dstTexels,
            srcTexelX, srcTexelY, dstTexelX, dstTexelY,
            mipWidth, mipHeight,
            texelCountX, texelCountY,
            srcPaletteType, dstPaletteType,
            srcPaletteSize,
            srcItemDepth, dstItemDepth,
            srcRowAlignment, dstRowAlignment
        );
    }
    else
    {
        assert( dstPaletteType == PALETTE_NONE );

        // Move color items.
        colorModelDispatcher <const void> fetchDispatch( srcRasterFormat, srcColorOrder, srcItemDepth, NULL, 0, PALETTE_NONE );
        colorModelDispatcher <void> putDispatch( dstRasterFormat, dstColorOrder, dstItemDepth, NULL, 0, PALETTE_NONE );

        uint32 srcRowSize = getRasterDataRowSize( mipWidth, srcItemDepth, srcRowAlignment );
        uint32 dstRowSize = getRasterDataRowSize( mipWidth, dstItemDepth, dstRowAlignment );

        copyTexelDataEx(
            srcTexels, dstTexels,
            fetchDispatch, putDispatch,
            texelCountX, texelCountY,
            srcTexelX, srcTexelY,
            dstTexelX, dstTexelY,
            srcRowSize, dstRowSize
        );
    }
}

inline double unpackcolor( uint8 color )
{
    return ( (double)color / 255.0 );
}

inline uint8 packcolor( double color )
{
    return (uint8)( color * 255.0 );
}

inline bool doesRasterFormatNeedConversion(
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder, ePaletteType srcPaletteType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder, ePaletteType dstPaletteType
)
{
    // Returns true if the source raster format needs to be converted
    // to become the destination raster format. This is useful if you want
    // to directly acquire texels instead of passing them into a conversion
    // routine.

    if ( srcRasterFormat != dstRasterFormat || srcDepth != dstDepth || srcColorOrder != dstColorOrder || srcPaletteType != dstPaletteType )
    {
        return true;
    }

    // TODO: add optimizations to this decision making.
    // Like RGBA8888 32bit can be directly acquired to RGB8888 32bit.

    return false;
}

inline bool doesPixelDataNeedConversion(
    const pixelDataTraversal& pixelData,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType
)
{
    // This function is supposed to decide whether the information stored in pixelData, which is
    // reflected by the source format, requires expensive conversion to reach the destination format.
    // pixelData is expected to be raw uncompressed texture data.
    
    // If the raster format has changed, there is no way around conversion.
    if ( doesRasterFormatNeedConversion(
             srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType,
             dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType
         ) )
    {
        return true;
    }

    // Then there is the possibility that the buffer has expanded, for any mipmap inside of pixelData.
    // A conversion will properly fix that.
    {
        size_t numberOfMipmaps = pixelData.mipmaps.size();

        for ( size_t n = 0; n < numberOfMipmaps; n++ )
        {
            const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

            bool doesRequireNewTexelBuffer =
                shouldAllocateNewRasterBuffer(
                    mipLayer.mipWidth,
                    srcDepth, srcRowAlignment,
                    dstDepth, dstRowAlignment
                );

            if ( doesRequireNewTexelBuffer )
            {
                // If we require a new texel buffer, we kinda have to convert stuff.
                // The conversion routine is an all-in-one fix, that should not be called too often.
                return true;
            }
        }
    }

    // We prefer if there is no conversion required.
    return false;
}

};

#endif //_PIXELFORMAT_INTERNAL_INCLUDE_