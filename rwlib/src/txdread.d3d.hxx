#include <d3d9.h>

#include "txdread.d3d.genmip.hxx"

namespace rw
{

inline uint32 getD3DPaletteCount(ePaletteType paletteType)
{
    uint32 reqPalCount = 0;

    if (paletteType == PALETTE_4BIT)
    {
        reqPalCount = 32;
    }
    else if (paletteType == PALETTE_8BIT)
    {
        reqPalCount = 256;
    }
    else
    {
        assert( 0 );
    }

    return reqPalCount;
}

// Function to get a default raster format string.
inline void getDefaultRasterFormatString( eRasterFormat rasterFormat, ePaletteType paletteType, eColorOrdering colorOrder, std::string& formatOut )
{
    // Put info about pixel type.
    {
        if ( rasterFormat == RASTER_DEFAULT )
        {
            formatOut += "undefined";
        }
        else if ( rasterFormat == RASTER_1555 )
        {
            formatOut += "1555";
        }
        else if ( rasterFormat == RASTER_565 )
        {
            formatOut += "565";
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            formatOut += "4444";
        }
        else if ( rasterFormat == RASTER_LUM8 )
        {
            formatOut += "LUM8";
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            formatOut += "8888";
        }
        else if ( rasterFormat == RASTER_888 )
        {
            formatOut += "888";
        }
        else if ( rasterFormat == RASTER_16 )
        {
            formatOut += "depth_16";
        }
        else if ( rasterFormat == RASTER_24 )
        {
            formatOut += "depth_24";
        }
        else if ( rasterFormat == RASTER_32 )
        {
            formatOut += "depth_32";
        }
        else if ( rasterFormat == RASTER_555 )
        {
            formatOut += "555";
        }
        else
        {
            formatOut += "unknown";
        }
    }

    // Put info about palette type.
    if ( paletteType != PALETTE_NONE )
    {
        if ( paletteType == PALETTE_4BIT )
        {
            formatOut += " PAL4";
        }
        else if ( paletteType == PALETTE_8BIT )
        {
            formatOut += " PAL8";
        }
        else
        {
            formatOut += " PAL";
        }
    }

    // Put info about color order
    {
        if ( colorOrder == COLOR_RGBA )
        {
            formatOut += " RGBA";
        }
        else if ( colorOrder == COLOR_BGRA )
        {
            formatOut += " BGRA";
        }
        else if ( colorOrder == COLOR_ABGR )
        {
            formatOut += " ABGR";
        }
    }
}

};