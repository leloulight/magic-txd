#include "StdInc.h"

#include "txdread.size.hxx"

namespace rw
{

struct resizeFilterBlurPlugin : public rasterResizeFilterInterface
{
    void GetSupportedFiltering( resizeFilteringCaps& capsOut ) const override
    {
        capsOut.supportsMagnification = false;
        capsOut.supportsMinification = true;
        capsOut.magnify2D = false;
        capsOut.minify2D = true;
    }

    void MagnifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 dstX, uint32 dstY
    ) const override
    {
        // TODO.
        throw RwException( "not supported yet" );
    }

    void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& colorItem
    ) const override
    {
        eColorModel model = srcBmp.getColorModel();

        if ( model == COLORMODEL_RGBA )
        {
            uint32 redSumm = 0;
            uint32 greenSumm = 0;
            uint32 blueSumm = 0;
            uint32 alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < minScaleY; y++ )
            {
                for ( uint32 x = 0; x < minScaleX; x++ )
                {
                    abstractColorItem srcColorItem;

                    bool hasColor = srcBmp.fetchcolor(
                        x + minX, y + minY,
                        srcColorItem
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        redSumm += srcColorItem.rgbaColor.r;
                        greenSumm += srcColorItem.rgbaColor.g;
                        blueSumm += srcColorItem.rgbaColor.b;
                        alphaSumm += srcColorItem.rgbaColor.a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.rgbaColor.r = std::min( redSumm / addCount, 255u );
                colorItem.rgbaColor.g = std::min( greenSumm / addCount, 255u );
                colorItem.rgbaColor.b = std::min( blueSumm / addCount, 255u );
                colorItem.rgbaColor.a = std::min( alphaSumm / addCount, 255u );

                colorItem.model = COLORMODEL_RGBA;
            }
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            uint32 lumSumm = 0;
            uint32 alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < minScaleY; y++ )
            {
                for ( uint32 x = 0; x < minScaleX; x++ )
                {
                    abstractColorItem srcColorItem;

                    bool hasColor = srcBmp.fetchcolor(
                        x + minX, y + minY,
                        srcColorItem
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        lumSumm += srcColorItem.luminance.lum;
                        alphaSumm += srcColorItem.luminance.alpha;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.luminance.lum = std::min( lumSumm / addCount, 255u );
                colorItem.luminance.alpha = std::min( alphaSumm / addCount, 255u );

                colorItem.model = COLORMODEL_LUMINANCE;
            }
        }
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        RegisterResizeFiltering( engineInterface, "blur", this );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        UnregisterResizeFiltering( engineInterface, this );
    }
};

static PluginDependantStructRegister <resizeFilterBlurPlugin, RwInterfaceFactory_t> resizeFilterBlurPluginRegister;

void registerRasterSizeBlurPlugin( void )
{
    resizeFilterBlurPluginRegister.RegisterPlugin( engineFactory );
}

};