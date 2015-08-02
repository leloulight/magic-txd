#include <d3d9.h>

#include "txdread.d3d.genmip.hxx"

namespace rw
{

inline uint32 getD3DTextureDataRowAlignment( void )
{
    // We somehow always align our texture data rows by DWORD.
    // This is meant to speed up the access of rows, by the hardware driver.
    // In Direct3D, valid values are 1, 2, 4, or 8.
    return sizeof( DWORD );
}

inline uint32 getD3DRasterDataRowSize( uint32 texWidth, uint32 depth )
{
    return getRasterDataRowSize( texWidth, depth, getD3DTextureDataRowAlignment() );
}

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
inline void getDefaultRasterFormatString( eRasterFormat rasterFormat, uint32 itemDepth, ePaletteType paletteType, eColorOrdering colorOrder, std::string& formatOut )
{
    // Put info about pixel type.
    bool isColorOrderImportant = false;
    {
        if ( rasterFormat == RASTER_DEFAULT )
        {
            formatOut += "undefined";
        }
        else if ( rasterFormat == RASTER_1555 )
        {
            formatOut += "1555";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_565 )
        {
            formatOut += "565";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            formatOut += "4444";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_LUM )
        {
            if ( itemDepth == 8 )
            {
                formatOut += "LUM8";
            }
            else if ( itemDepth == 4 )
            {
                formatOut += "LUM4";
            }
            else
            {
                formatOut += "LUM";
            }
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            formatOut += "8888";

            isColorOrderImportant = true;
        }
        else if ( rasterFormat == RASTER_888 )
        {
            formatOut += "888";

            if ( itemDepth == 24 )
            {
                formatOut += " 24bit";
            }

            isColorOrderImportant = true;
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

            isColorOrderImportant = true;
        }
        else
        {
            formatOut += "unknown";
        }
    }

    // Put info about palette type.
    if ( paletteType != PALETTE_NONE )
    {
        if ( paletteType == PALETTE_4BIT ||
             paletteType == PALETTE_4BIT_LSB )
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
    if ( isColorOrderImportant )
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