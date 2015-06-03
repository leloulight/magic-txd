// Simple bitmap software rendering library for image processing from RenderWare textures.
#include <StdInc.h>

#include "pixelformat.hxx"

namespace rw
{

void Bitmap::setSize( uint32 width, uint32 height )
{
    uint32 oldWidth = this->width;
    uint32 oldHeight = this->height;

    // Allocate a new texel array.
    uint32 dataSize = getRasterImageDataSize( width, height, this->depth );

    void *oldTexels = this->texels;
    void *newTexels = NULL;

    if ( dataSize != 0 )
    {
        newTexels = new uint8[ dataSize ];

        eRasterFormat rasterFormat = this->rasterFormat;
        eColorOrdering colorOrder = this->colorOrder;
        uint32 rasterDepth = this->depth;

        // Do an image copy.
        for ( uint32 y = 0; y < height; y++ )
        {
            for ( uint32 x = 0; x < width; x++ )
            {
                uint32 colorIndex = ( y * width + x );

                uint8 srcRed = packcolor( this->bgRed );
                uint8 srcGreen = packcolor( this->bgGreen );
                uint8 srcBlue = packcolor( this->bgBlue );
                uint8 srcAlpha = 255;

                // Try to get a source color.
                if ( oldTexels != NULL && x < oldWidth && y < oldHeight )
                {
                    uint32 oldColorIndex = ( y * oldWidth + x );

                    browsetexelcolor(
                        oldTexels, PALETTE_NONE, NULL, 0, oldColorIndex, rasterFormat, colorOrder, rasterDepth,
                        srcRed, srcGreen, srcBlue, srcAlpha
                    );
                }

                // Put it into the new storage.
                puttexelcolor(
                    newTexels, colorIndex, rasterFormat, colorOrder, rasterDepth, srcRed, srcGreen, srcBlue, srcAlpha
                );
            }
        }
    }

    // Delete old data.
    if ( oldTexels )
    {
        delete oldTexels;
    }

    // Set new data.
    this->texels = newTexels;
    this->width = width;
    this->height = height;
    this->dataSize = dataSize;
}

static inline void fetchpackedcolor(
    void *texels, uint32 colorIndex, eRasterFormat theFormat, eColorOrdering colorOrder, uint32 itemDepth, double& redOut, double& greenOut, double& blueOut, double& alphaOut
)
{
    uint8 sourceRedPacked, sourceGreenPacked, sourceBluePacked, sourceAlphaPacked;
    browsetexelcolor(
        texels, PALETTE_NONE, NULL, 0, colorIndex, theFormat, colorOrder, itemDepth,
        sourceRedPacked, sourceGreenPacked, sourceBluePacked, sourceAlphaPacked
    );

    redOut = unpackcolor( sourceRedPacked );
    blueOut = unpackcolor( sourceBluePacked );
    greenOut = unpackcolor( sourceGreenPacked );
    alphaOut = unpackcolor( sourceAlphaPacked );
}

static inline void getblendfactor(
    double srcRed, double srcGreen, double srcBlue, double srcAlpha,
    double dstRed, double dstGreen, double dstBlue, double dstAlpha,
    Bitmap::eShadeMode shadeMode,
    double& blendRed, double& blendGreen, double& blendBlue, double& blendAlpha
)
{
    if ( shadeMode == Bitmap::SHADE_SRCALPHA )
    {
        blendRed = srcAlpha;
        blendGreen = srcAlpha;
        blendBlue = srcAlpha;
        blendAlpha = srcAlpha;
    }
    else if ( shadeMode == Bitmap::SHADE_INVSRCALPHA )
    {
        double invAlpha = ( 1.0 - srcAlpha );

        blendRed = invAlpha;
        blendGreen = invAlpha;
        blendBlue = invAlpha;
        blendAlpha = invAlpha;
    }
    else if ( shadeMode == Bitmap::SHADE_ONE )
    {
        blendRed = 1;
        blendGreen = 1;
        blendBlue = 1;
        blendAlpha = 1;
    }
    else if ( shadeMode == Bitmap::SHADE_ZERO )
    {
        blendRed = 0;
        blendGreen = 0;
        blendBlue = 0;
        blendAlpha = 0;
    }
    else
    {
        blendRed = 1;
        blendGreen = 1;
        blendBlue = 1;
        blendAlpha = 1;
    }
}

bool Bitmap::browsecolor(uint32 x, uint32 y, uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut) const
{
    bool hasColor = false;

    if ( x < this->width && y < this->height )
    {
        uint32 colorIndex = PixelFormat::coord2index( x, y, this->width );

        hasColor = browsetexelcolor(
            this->texels, PALETTE_NONE, NULL, 0, colorIndex, this->rasterFormat, this->colorOrder, this->depth,
            redOut, greenOut, blueOut, alphaOut
        );
    }

    return hasColor;
}

void Bitmap::draw(
    sourceColorPipeline& colorSource, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
    eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
)
{
    // Put parameters on stack.
    uint32 ourWidth = this->width;
    uint32 ourHeight = this->height;
    uint32 ourDepth = this->depth;
    void *ourTexels = this->texels;
    eRasterFormat ourFormat = this->rasterFormat;
    eColorOrdering ourOrder = this->colorOrder;

    uint32 theirWidth = colorSource.getWidth();
    uint32 theirHeight = colorSource.getHeight();

    // Calculate drawing parameters.
    double floatOffX = (double)offX;
    double floatOffY = (double)offY;
    double floatDrawWidth = (double)drawWidth;
    double floatDrawHeight = (double)drawHeight;
    double srcBitmapWidthStride = ( theirWidth / floatDrawWidth );
    double srcBitmapHeightStride = ( theirHeight / floatDrawHeight );

    // Perform the drawing.
    for ( double y = 0; y < floatDrawHeight; y++ )
    {
        for ( double x = 0; x < floatDrawWidth; x++ )
        {
            // Get the coordinates to draw on into this bitmap.
            uint32 sourceX = (uint32)( x + floatOffX );
            uint32 sourceY = (uint32)( y + floatOffY );

            // Get the coordinates from the bitmap that is being drawn from.
            uint32 targetX = (uint32)( x * srcBitmapWidthStride );
            uint32 targetY = (uint32)( y * srcBitmapHeightStride );

            // Check that all coordinates are inside of their dimensions.
            if ( targetX < theirWidth && targetY < theirHeight &&
                 sourceX < ourWidth && sourceY < ourHeight )
            {
                // Fetch the colors from both bitmaps.
                double sourceRed, sourceGreen, sourceBlue, sourceAlpha;
                double dstRed, dstGreen, dstBlue, dstAlpha;

                // Fetch from "source".
                uint32 srcColorIndex = ( sourceY * ourWidth + sourceX );
                {
                    fetchpackedcolor(
                        ourTexels, srcColorIndex, ourFormat, ourOrder, ourDepth,
                        sourceRed, sourceGreen, sourceBlue, sourceAlpha
                    );
                }

                // Fetch from "destination".
                uint32 dstColorIndex = ( targetY * theirWidth + targetX );
                {
                    colorSource.fetchcolor(dstColorIndex, dstRed, dstGreen, dstBlue, dstAlpha);
                }

                // Get the blend factors.
                double srcBlendRed, srcBlendGreen, srcBlendBlue, srcBlendAlpha;
                double dstBlendRed, dstBlendGreen, dstBlendBlue, dstBlendAlpha;
                
                getblendfactor(
                    sourceRed, sourceGreen, sourceBlue, sourceAlpha,
                    dstRed, dstGreen, dstBlue, dstAlpha, srcChannel,
                    srcBlendRed, srcBlendGreen, srcBlendBlue, srcBlendAlpha
                );
                getblendfactor(
                    sourceRed, sourceGreen, sourceBlue, sourceAlpha,
                    dstRed, dstGreen, dstBlue, dstAlpha, dstChannel,
                    dstBlendRed, dstBlendGreen, dstBlendBlue, dstBlendAlpha
                );

                // Get the blended colors.
                double srcBlendedRed = ( sourceRed * srcBlendRed );
                double srcBlendedGreen = ( sourceGreen * srcBlendGreen );
                double srcBlendedBlue = ( sourceBlue * srcBlendBlue );
                double srcBlendedAlpha = ( sourceAlpha * srcBlendAlpha );

                double dstBlendedRed = ( dstRed * dstBlendRed );
                double dstBlendedGreen = ( dstGreen * dstBlendGreen );
                double dstBlendedBlue = ( dstBlue * dstBlendBlue );
                double dstBlendedAlpha = ( dstAlpha * dstBlendAlpha );

                // Perform the color op.
                double resRed, resGreen, resBlue, resAlpha;
                {
                    if ( blendMode == BLEND_MODULATE )
                    {
                        resRed = srcBlendedRed * dstBlendedRed;
                        resGreen = srcBlendedGreen * dstBlendedGreen;
                        resBlue = srcBlendedBlue * dstBlendedBlue;
                        resAlpha = srcBlendedAlpha * dstBlendedAlpha;
                    }
                    else if ( blendMode == BLEND_ADDITIVE )
                    {
                        resRed = srcBlendedRed + dstBlendedRed;
                        resGreen = srcBlendedGreen + dstBlendedGreen;
                        resBlue = srcBlendedBlue + dstBlendedBlue;
                        resAlpha = srcBlendedAlpha + dstBlendedAlpha;
                    }
                    else
                    {
                        resRed = sourceRed;
                        resGreen = sourceGreen;
                        resBlue = sourceBlue;
                        resAlpha = sourceAlpha;
                    }
                }

                // Clamp the colors.
                resRed = std::max( 0.0, std::min( 1.0, resRed ) );
                resGreen = std::max( 0.0, std::min( 1.0, resGreen ) );
                resBlue = std::max( 0.0, std::min( 1.0, resBlue ) );
                resAlpha = std::max( 0.0, std::min( 1.0, resAlpha ) );

                // Write back the new color.
                {
                    puttexelcolor(
                        ourTexels, srcColorIndex, ourFormat, ourOrder, ourDepth,
                        packcolor( resRed ), packcolor( resGreen ), packcolor( resBlue ), packcolor( resAlpha )
                    );
                }
            }
        }
    }

    // We are finished!
}

void Bitmap::drawBitmap(
    const Bitmap& theBitmap, uint32 offX, uint32 offY, uint32 drawWidth, uint32 drawHeight,
    eShadeMode srcChannel, eShadeMode dstChannel, eBlendMode blendMode
)
{
    struct bitmapColorSource : public sourceColorPipeline
    {
        const Bitmap& bmpSource;

        uint32 theWidth;
        uint32 theHeight;
        uint32 theDepth;
        eRasterFormat theFormat;
        eColorOrdering theOrder;
        void *theTexels;

        inline bitmapColorSource( const Bitmap& bmp ) : bmpSource( bmp )
        {
            bmp.getSize(this->theWidth, this->theHeight);

            this->theDepth = bmp.getDepth();
            this->theFormat = bmp.getFormat();
            this->theOrder = bmp.getColorOrder();
            this->theTexels = bmp.texels;
        }

        uint32 getWidth( void ) const
        {
            return this->theWidth;
        }

        uint32 getHeight( void ) const
        {
            return this->theHeight;
        }

        void fetchcolor( uint32 colorIndex, double& red, double& green, double& blue, double& alpha )
        {
            fetchpackedcolor(
                this->theTexels, colorIndex, this->theFormat, this->theOrder, this->theDepth, red, green, blue, alpha
            );
        }
    };

    bitmapColorSource bmpSource( theBitmap );

    // Draw using general-purpose pipeline.
    this->draw( bmpSource, offX, offY, drawWidth, drawHeight, srcChannel, dstChannel, blendMode );
}

};