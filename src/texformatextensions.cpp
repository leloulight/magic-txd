#include "mainwindow.h"

#include <d3d9.h>

inline unsigned char rgbToLuminance( rw::uint8 r, rw::uint8 g, rw::uint8 b )
{
    unsigned int colorSumm = ( r + g + b );

    return ( colorSumm / 3 );
}

struct a8l8FormatExtension : rw::d3dpublic::nativeTextureFormatHandler
{
    const char* GetFormatName( void ) const override
    {
        return "A8L8";
    }

    struct pixel_t
    {
        unsigned char lum;
        unsigned char alpha;
    };

    size_t GetFormatTextureDataSize( unsigned int width, unsigned int height ) const override
    {
        return ( width * height * sizeof(pixel_t) );
    }

    void GetTextureRWFormat( rw::eRasterFormat& rasterFormatOut, unsigned int& depthOut, rw::eColorOrdering& colorOrderOut ) const
    {
        rasterFormatOut = rw::RASTER_8888;
        depthOut = 32;
        colorOrderOut = rw::COLOR_BGRA;
    }

    void ConvertToRW(
        const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t texDataSize,
        void *texOut
    ) const override
    {
        const rw::eRasterFormat rasterFormat = rw::RASTER_8888;
        const unsigned int depth = 32;
        const rw::eColorOrdering colorOrder = rw::COLOR_BGRA;

        // do the conversion.
        unsigned int texItemCount = ( texMipWidth * texMipHeight );

        const pixel_t *encodedColors = (pixel_t*)texData;

        for ( unsigned int n = 0; n < texItemCount; n++ )
        {
            // We are simply a pixel_t.
            const pixel_t *theTexel = encodedColors + n;

            rw::PutTexelRGBA( texOut, n, rasterFormat, depth, colorOrder, theTexel->lum, theTexel->lum, theTexel->lum, theTexel->alpha );
        }

        // Alright, we are done!
    }

    void ConvertFromRW(
        unsigned int texMipWidth, unsigned int texMipHeight,
        const void *texelSource, rw::eRasterFormat rasterFormat, unsigned int depth, rw::eColorOrdering colorOrder, rw::ePaletteType paletteType, const void *paletteData, unsigned int paletteSize,
        void *texOut
    ) const override
    {
        // We write stuff.
        unsigned int texItemCount = ( texMipWidth * texMipHeight );

        pixel_t *encodedColors = (pixel_t*)texOut;

        for ( unsigned int n = 0; n < texItemCount; n++ )
        {
            // Get the color as RGBA and convert to closely matching luminance value.
            rw::uint8 r, g, b, a;

            rw::BrowseTexelRGBA(
                texelSource, n,
                rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
                r, g, b, a
            );

            rw::uint8 lumVal = rgbToLuminance( r, g, b );

            pixel_t *theTexel = ( encodedColors + n );

            theTexel->lum = lumVal;
            theTexel->alpha = a;
        }

        // Done!
    }
};

// If your format is not used by the engine, you can be sure that a more fitting standard way for it exists
// that is bound natively by the library ;)
static a8l8FormatExtension a8l8Format;

void MainWindow::initializeNativeFormats( void )
{
    // Register a basic format that we want to test things on.
    // We only can do that if the engine has the Direct3D9 native texture loaded.
    rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( this->rwEngine, "Direct3D9" );

    if ( driverIntf )
    {
        driverIntf->RegisterFormatHandler( (DWORD)D3DFMT_A8L8, &a8l8Format );
    }
}

void MainWindow::shutdownNativeFormats( void )
{
    // We want to unregister our basic formats again.
    rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( this->rwEngine, "Direct3D9" );

    if ( driverIntf )
    {
        driverIntf->UnregisterFormatHandler( (DWORD)D3DFMT_A8L8 );
    }
}