#include "StdInc.h"

#include "txdread.size.hxx"

namespace rw
{

inline uint8 linearInterpolateChannel( uint8 left, uint8 right, double mod )
{
    return packcolor(
        ( unpackcolor( right ) - unpackcolor( left ) ) * mod
    ) + left;
}

inline abstractColorItem linearInterpolateColorItem(
    const abstractColorItem& left, const abstractColorItem& right,
    double mod
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = linearInterpolateChannel( left.rgbaColor.r, right.rgbaColor.r, mod );
        colorOut.rgbaColor.g = linearInterpolateChannel( left.rgbaColor.g, right.rgbaColor.g, mod );
        colorOut.rgbaColor.b = linearInterpolateChannel( left.rgbaColor.b, right.rgbaColor.b, mod );
        colorOut.rgbaColor.a = linearInterpolateChannel( left.rgbaColor.a, right.rgbaColor.a, mod );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = linearInterpolateChannel( left.luminance.lum, right.luminance.lum, mod );
        colorOut.luminance.alpha = linearInterpolateChannel( left.luminance.alpha, right.luminance.alpha, mod );
    }
    else
    {
        throw RwException( "invalid color model in color linear interpolation" );
    }
    
    return colorOut;
}

inline abstractColorItem addColorItems(
    const abstractColorItem& left, const abstractColorItem& right
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = ( left.rgbaColor.r + right.rgbaColor.r );
        colorOut.rgbaColor.g = ( left.rgbaColor.g + right.rgbaColor.g );
        colorOut.rgbaColor.b = ( left.rgbaColor.b + right.rgbaColor.b );
        colorOut.rgbaColor.a = ( left.rgbaColor.a + right.rgbaColor.a );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = ( left.luminance.lum + right.luminance.lum );
        colorOut.luminance.alpha = ( left.luminance.alpha + right.luminance.alpha );
    }
    else
    {
        throw RwException( "invalid color model for color addition" );
    }

    return colorOut;
}

inline abstractColorItem subColorItems(
    const abstractColorItem& left, const abstractColorItem& right
)
{
    abstractColorItem colorOut;

    assert( left.model == right.model );

    colorOut.model = left.model;

    if ( left.model == COLORMODEL_RGBA )
    {
        colorOut.rgbaColor.r = ( left.rgbaColor.r - right.rgbaColor.r );
        colorOut.rgbaColor.g = ( left.rgbaColor.g - right.rgbaColor.g );
        colorOut.rgbaColor.b = ( left.rgbaColor.b - right.rgbaColor.b );
        colorOut.rgbaColor.a = ( left.rgbaColor.a - right.rgbaColor.a );
    }
    else if ( left.model == COLORMODEL_LUMINANCE )
    {
        colorOut.luminance.lum = ( left.luminance.lum - right.luminance.lum );
        colorOut.luminance.alpha = ( left.luminance.alpha - right.luminance.alpha );
    }
    else
    {
        throw RwException( "invalid color model for color addition" );
    }

    return colorOut;
}

struct resizeFilterLinearPlugin : public rasterResizeFilterInterface
{
    void GetSupportedFiltering( resizeFilteringCaps& capsOut ) const override
    {
        capsOut.supportsMagnification = true;
        capsOut.supportsMinification = false;
        capsOut.magnify2D = false;
        capsOut.minify2D = false;
    }

    void MagnifyFiltering(
        const resizeColorPipeline& srcBmp,
        uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 srcX, uint32 srcY
    ) const override
    {
        bool isTwoDimensional = ( magScaleX > 1 && magScaleY > 1 );

        if ( isTwoDimensional )
        {
            throw RwException( "linear filtering does not support two dimensional upscaling" );
        }

        // Get the first color to interpolate to.
        abstractColorItem interpolateSource;

        bool gotSrcColor = srcBmp.fetchcolor( srcX, srcY, interpolateSource );

        if ( !gotSrcColor )
        {
            throw RwException( "failed to get source color in linear filtering" );
        }

        // Get the second color to interpolate to.
        abstractColorItem interpolateTarget;

        uint32 scaleXOffset = ( magScaleX - 1 );
        uint32 scaleYOffset = ( magScaleY - 1 );

        uint32 vecX = std::min( 1u, scaleXOffset );
        uint32 vecY = std::min( 1u, scaleYOffset );

        bool gotTargetColor = srcBmp.fetchcolor( srcX + vecX, srcY + vecY, interpolateTarget );

        if ( !gotTargetColor )
        {
            // We will do a copy operation.
            interpolateTarget = interpolateSource;
        }

        uint32 intColorDist = ( scaleXOffset + scaleYOffset + 1 );
        double colorDist = (double)intColorDist;

        // Fill the destination with interpolated colors.
        for ( uint32 n = 0; n < intColorDist; n++ )
        {
            uint32 targetX = ( magX + vecX * n );
            uint32 targetY = ( magY + vecY * n );

            double interpMod = ( (double)n / colorDist );

            // Calculate interpolated color.
            abstractColorItem targetColor = linearInterpolateColorItem( interpolateSource, interpolateTarget, interpMod );

            dstBmp.putcolor( targetX, targetY, targetColor );
        }
    }

    void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& reducedColor
    ) const override
    {
        throw RwException( "linear filter plugin does not support minification" );
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterResizeFiltering( engineInterface, "linear", this );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterResizeFiltering( engineInterface, this );
    }
};

static PluginDependantStructRegister <resizeFilterLinearPlugin, RwInterfaceFactory_t> resizeFilterLinearPluginRegister;

void registerRasterResizeLinearPlugin( void )
{
    resizeFilterLinearPluginRegister.RegisterPlugin( engineFactory );
}

};