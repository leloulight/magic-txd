#ifndef _RENDERWARE_DRIVER_FRAMEWORK_
#define _RENDERWARE_DRIVER_FRAMEWORK_

namespace rw
{

// Driver properties that have to be passed during driver registration.
struct driverConstructionProps
{
    size_t rasterMemSize;
    size_t geomMemSize;
    size_t matMemSize;
};

/*
    This is the driver interface that every render device has to implement.
    Is is a specialization of the original Criterion pipeline design
*/
struct nativeDriverImplementation
{
    inline nativeDriverImplementation( void )
    {
        this->managerData.isRegistered = false;

        // Every driver has to export functionality for instanced objects.
        this->managerData.driverType = NULL;
        this->managerData.rasterType = NULL;
        this->managerData.geometryType = NULL;
        this->managerData.materialType = NULL;
    }

    inline ~nativeDriverImplementation( void )
    {
        if ( this->managerData.isRegistered )
        {
            LIST_REMOVE( this->managerData.node );

            this->managerData.isRegistered = false;
        }
    }

    // Instanced type creation.
    virtual void OnRasterConstruct( Interface *engineInterface, void *objMem, size_t memSize ) = 0;
    virtual void OnRasterCopyConstruct( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) = 0;
    virtual void OnRasterDestroy( Interface *engineInterface, void *objMem, size_t memSize ) = 0;

    virtual void OnGeometryConstruct( Interface *engineInterface, void *objMem, size_t memSize ) = 0;
    virtual void OnGeometryCopyConstruct( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) = 0;
    virtual void OnGeometryDestroy( Interface *engineInterface, void *objMem, size_t memSizue ) = 0;

    virtual void OnMaterialConstruct( Interface *engineInterface, void *objMem, size_t memSize ) = 0;
    virtual void OnMaterialCopyConstruct( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) = 0;
    virtual void OnMaterialDestroy( Interface *engineInterface, void *objMem, size_t memSize ) = 0;

    // Raster instancing.
    virtual void RasterInstance( Interface *engineInterface, void *objMem, Raster *sysRaster ) = 0;
    virtual void RasterUninstance( Interface *engineInterface, void *objMem ) = 0;

    // Geometry instancing.
    virtual void GeometryInstance( Interface *engineInterface, void *objMem, Geometry *sysGeom ) = 0;
    virtual void GeometryUninstance( Interface *engineInterface, void *objMem ) = 0;

    // Material instancing.
    virtual void MaterialInstance( Interface *engineInterface, void *objMem, Material *sysMat ) = 0;
    virtual void MateríalUninstance( Interface *engineInterface, void *objMem ) = 0;

    // Private manager data.
    struct
    {
        RwTypeSystem::typeInfoBase *driverType;         // actual driver type
        RwTypeSystem::typeInfoBase *rasterType;         // type of instanced raster
        RwTypeSystem::typeInfoBase *geometryType;       // type of instanced geometry
        RwTypeSystem::typeInfoBase *materialType;       // type of instanced material

        bool isRegistered;
        RwListEntry <nativeDriverImplementation> node;
    } managerData;
};

// Driver registration API.
bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf );
bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf );

};

#endif //_RENDERWARE_DRIVER_FRAMEWORK_