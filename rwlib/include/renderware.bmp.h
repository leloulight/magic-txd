inline uint32 getRasterDataRowSizeAligned( uint32 rowSize, uint32 alignment )
{
    if ( alignment != 0 )
    {
        return ALIGN_SIZE( rowSize, alignment );
    }

    return rowSize;
}

inline uint32 getRasterDataRawRowSize( uint32 planeWidth, uint32 depth )
{
    return ( ALIGN_SIZE( planeWidth * depth, 8u ) / 8u );
}

inline uint32 getRasterDataRowSize( uint32 planeWidth, uint32 depth, uint32 alignment )
{
    uint32 rowSizeWithoutPadding = getRasterDataRawRowSize( planeWidth, depth );

    return getRasterDataRowSizeAligned( rowSizeWithoutPadding, alignment );
}

inline uint32 getRasterDataSizeByRowSize( uint32 rowSize, uint32 height )
{
    return ( rowSize * height );
}

inline uint32 getPaletteRowAlignment( void )
{
    return 0;
}

inline uint32 getPaletteDataSize( uint32 paletteCount, uint32 depth )
{
    return getRasterDataRowSize( paletteCount, depth, getPaletteRowAlignment() );
}

inline uint32 getPackedRasterDataSize( uint32 itemCount, uint32 depth )
{
    return ( ALIGN_SIZE( itemCount * depth, 8u ) / 8u );
}

inline void* getTexelDataRow( void *texelData, uint32 rowSize, uint32 n )
{
    return (char*)texelData + rowSize * n;
}

inline const void* getConstTexelDataRow( const void *texelData, uint32 rowSize, uint32 n )
{
    return (const char*)texelData + rowSize * n;
}

inline bool shouldAllocateNewRasterBuffer( uint32 mipWidth, uint32 srcDepth, uint32 srcRowAlignment, uint32 dstDepth, uint32 dstRowAlignment )
{
    // If the depth changed, an item will take a different amount of space.
    // This means that items cannot be placed at the positions where they belong to (swapping) so get off.
    if ( srcDepth != dstDepth )
    {
        return true;
    }

    // Assuming the depth is the same, then the alignment may change the size of the resulting
    // texel data. If it does, then we have to reallocate.
    if ( srcRowAlignment != dstRowAlignment )
    {
        uint32 rowSizeWithoutPadding = getRasterDataRawRowSize( mipWidth, srcDepth );   // depth is the same!

        uint32 srcRowSize = getRasterDataRowSizeAligned( rowSizeWithoutPadding, srcRowAlignment );
        uint32 dstRowSize = getRasterDataRowSizeAligned( rowSizeWithoutPadding, dstRowAlignment );

        if ( srcRowSize != dstRowSize )
        {
            return true;
        }
    }

    return false;
}

enum eColorModel
{
    COLORMODEL_RGBA,
    COLORMODEL_LUMINANCE,
    COLORMODEL_DEPTH
};

struct abstractColorItem
{
    eColorModel model;
    union
    {
        struct
        {
            uint8 r, g, b, a;
        } rgbaColor;
        struct
        {
            uint8 lum;
            uint8 alpha;
        } luminance;
    };
};

// Bitmap software rendering includes.
struct Bitmap
{
    inline Bitmap( void )
    {
        this->width = 0;
        this->height = 0;
        this->depth = 32;
        this->rowAlignment = 4; // good measure.
        this->rowSize = 0;
        this->rasterFormat = RASTER_8888;
        this->texels = NULL;
        this->dataSize = 0;

        this->colorOrder = COLOR_RGBA;

        this->bgRed = 0;
        this->bgGreen = 0;
        this->bgBlue = 0;
        this->bgAlpha = 1.0;
    }

    inline Bitmap( uint32 depth, eRasterFormat theFormat, eColorOrdering colorOrder )
    {
        this->width = 0;
        this->height = 0;
        this->depth = depth;
        this->rowAlignment = 4;
        this->rowSize = 0;
        this->rasterFormat = theFormat;
        this->texels = NULL;
        this->dataSize = 0;

        this->colorOrder = colorOrder;

        this->bgRed = 0;
        this->bgGreen = 0;
        this->bgBlue = 0;
        this->bgAlpha = 1.0;
    }

private:
    inline void assignFrom( const Bitmap& right )
    {
        this->width = right.width;
        this->height = right.height;
        this->depth = right.depth;
        this->rowAlignment = right.rowAlignment;
        this->rowSize = right.rowSize;
        this->rasterFormat = right.rasterFormat;

        // Copy texels.
        void *origTexels = right.texels;
        void *newTexels = NULL;

        if ( origTexels )
        {
            newTexels = new uint8[ right.dataSize ];

            memcpy( newTexels, origTexels, right.dataSize );
        }

        this->texels = newTexels;
        this->dataSize = right.dataSize;

        this->colorOrder = right.colorOrder;

        this->bgRed = right.bgRed;
        this->bgGreen = right.bgGreen;
        this->bgBlue = right.bgBlue;
        this->bgAlpha = right.bgAlpha;
    }

public:
    inline Bitmap( const Bitmap& right )
    {
        this->assignFrom( right );
    }

private:
    inline void moveFrom( Bitmap&& right )
    {
        this->width = right.width;
        this->height = right.height;
        this->depth = right.depth;
        this->rowAlignment = right.rowAlignment;
        this->rowSize = right.rowSize;
        this->rasterFormat = right.rasterFormat;

        // Move over texels.
        this->texels = right.texels;
        this->dataSize = right.dataSize;

        this->colorOrder = right.colorOrder;
        
        this->bgRed = right.bgRed;
        this->bgGreen = right.bgGreen;
        this->bgBlue = right.bgBlue;
        this->bgAlpha = right.bgAlpha;

        // Default the moved from object.
        right.texels = NULL;
    }

public:
    inline Bitmap( Bitmap&& right )
    {
        this->moveFrom( std::move( right ) );
    }

private:
    inline void clearTexelData( void )
    {
        // If we have texel data, deallocate it.
        if ( void *ourTexels = this->texels )
        {
            delete [] ourTexels;

            this->texels = NULL;
        }
    }

public:
    inline void operator =( const Bitmap& right )
    {
        this->clearTexelData();

        this->assignFrom( right );
    }

    inline void operator =( Bitmap&& right )
    {
        this->clearTexelData();

        this->moveFrom( std::move( right ) );
    }

    inline ~Bitmap( void )
    {
        this->clearTexelData();
    }

    inline static uint32 getRasterFormatDepth( eRasterFormat format )
    {
        // Returns the default raster format depth.
        // This one is standardized to be used by palette colors.

        uint32 theDepth = 0;

        if ( format == RASTER_8888 || format == RASTER_888 || format == RASTER_32 )
        {
            theDepth = 32;
        }
        else if ( format == RASTER_1555 || format == RASTER_565 || format == RASTER_4444 || format == RASTER_555 || format == RASTER_16 )
        {
            theDepth = 16;
        }
        else if ( format == RASTER_24 )
        {
            theDepth = 24;
        }
        else if ( format == RASTER_LUM )
        {
            theDepth = 8;
        }
        // NEW formats :3
        else if ( format == RASTER_LUM_ALPHA )
        {
            theDepth = 16;
        }

        return theDepth;
    }

    inline static uint32 getRasterImageDataSize( uint32 width, uint32 height, uint32 depth, uint32 rowAlignment )
    {
        uint32 rowSize = getRasterDataRowSize( width, depth, rowAlignment );

        return getRasterDataSizeByRowSize( rowSize, height );
    }

    inline void setImageData( void *theTexels, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment, uint32 width, uint32 height, uint32 dataSize, bool assignData = false )
    {
        this->width = width;
        this->height = height;
        this->depth = depth;
        this->rowAlignment = rowAlignment;
        this->rowSize = getRasterDataRowSize( width, depth, rowAlignment );
        this->rasterFormat = theFormat;

        // Deallocate texels if we already have some.
        if ( void *origTexels = this->texels )
        {
            delete [] origTexels;

            this->texels = NULL;
        }

        if ( assignData == false )
        {
            // Copy the texels.
            if ( dataSize != 0 )
            {
                void *newTexels = new uint8[ dataSize ];

                memcpy( newTexels, theTexels, dataSize );

                this->texels = newTexels;
            }
        }
        else
        {
            // Just give us the data.
            this->texels = theTexels;
        }
        this->dataSize = dataSize;

        this->colorOrder = colorOrder;
    }

    inline void setImageDataSimple( void *theTexels, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment, uint32 width, uint32 height )
    {
        uint32 rowSize = getRasterDataRowSize( width, depth, rowAlignment );

        uint32 dataSize = getRasterDataSizeByRowSize( rowSize, height );

        setImageData( theTexels, theFormat, colorOrder, depth, rowAlignment, width, height, dataSize );
    }

    void setSize( uint32 width, uint32 height );

    inline void getSize( uint32& width, uint32& height ) const
    {
        width = this->width;
        height = this->height;
    }

    inline uint32 getWidth( void ) const
    {
        return this->width;
    }

    inline uint32 getHeight( void ) const
    {
        return this->height;
    }

    inline uint32 getRowAlignment( void ) const
    {
        return this->rowAlignment;
    }

    inline void enlargePlane( uint32 reqWidth, uint32 reqHeight )
    {
        bool needsResize = false;

        uint32 curWidth = this->width;
        uint32 curHeight = this->height;

        if ( curWidth < reqWidth )
        {
            curWidth = reqWidth;

            needsResize = true;
        }

        if ( curHeight < reqHeight )
        {
            curHeight = reqHeight;

            needsResize = true;
        }

        if ( needsResize )
        {
            this->setSize( curWidth, curHeight );
        }
    }

    inline uint32 getDepth( void ) const
    {
        return this->depth;
    }

    inline eRasterFormat getFormat( void ) const
    {
        return this->rasterFormat;
    }

    inline eColorOrdering getColorOrder( void ) const
    {
        return this->colorOrder;
    }

    inline uint32 getDataSize( void ) const
    {
        return this->dataSize;
    }

	inline void *getTexelsData(void) const
	{
		return this->texels;
	}
    
    inline void* copyPixelData( void ) const
    {
        void *newPixels = NULL;
        void *ourPixels = this->texels;

        if ( ourPixels )
        {
            newPixels = new uint8[ this->dataSize ];

            memcpy( newPixels, ourPixels, this->dataSize );
        }

        return newPixels;
    }

    inline void setBgColor( double red, double green, double blue, double alpha = 1.0 )
    {
        this->bgRed = red;
        this->bgGreen = green;
        this->bgBlue = blue;
        this->bgAlpha = alpha;
    }

    inline void getBgColor( double& red, double& green, double& blue ) const
    {
        red = this->bgRed;
        green = this->bgGreen;
        blue = this->bgBlue;
    }

    bool browsecolor(uint32 x, uint32 y, uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut) const;
    bool browselum(uint32 x, uint32 y, uint8& lum, uint8& a) const;
    bool browsecolorex(uint32 x, uint32 y, abstractColorItem& colorItem ) const;

    eColorModel getColorModel( void ) const;

    enum eBlendMode
    {
        BLEND_MODULATE,
        BLEND_ADDITIVE
    };

    enum eShadeMode
    {
        SHADE_SRCALPHA,
        SHADE_INVSRCALPHA,
        SHADE_ZERO,
        SHADE_ONE
    };

    struct sourceColorPipeline abstract
    {
        virtual uint32 getWidth( void ) const = 0;
        virtual uint32 getHeight( void ) const = 0;

        virtual void fetchcolor( uint32 x, uint32 y, double& red, double& green, double& blue, double& alpha ) = 0;
    };

    void draw(
        sourceColorPipeline& colorSource,
        uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
        eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
    );

    void drawBitmap(
        const Bitmap& theBitmap, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
        eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
    );

private:
    uint32 width, height;
    uint32 depth;
    uint32 rowAlignment;
    uint32 rowSize;
    eRasterFormat rasterFormat;
    void *texels;
    uint32 dataSize;

    eColorOrdering colorOrder;

    double bgRed, bgGreen, bgBlue, bgAlpha;
};