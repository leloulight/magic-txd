#include "StdInc.h"

#include "rwdriver.d3d12.hxx"

namespace rw
{

void d3d12DriverInterface::RasterInstance( Interface *engineInterface, void *driverObjMem, void *objMem, Raster *sysRaster )
{
    if ( sysRaster->hasNativeDataOfType( "Direct3D9" ) == false )
    {
        throw RwException( "unsupported raster type for Direct3D 12 native raster instancing" );
    }

    // TODO.
}

void d3d12DriverInterface::RasterUninstance( Interface *engineInterface, void *driverObjMem, void *objMem )
{
    // TODO.
}

}