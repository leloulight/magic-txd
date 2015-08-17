#ifndef _RENDERWARE_DRIVER_FRAMEWORK_
#define _RENDERWARE_DRIVER_FRAMEWORK_

#include "rwdriver.immbuf.hxx"

namespace rw
{

// Driver properties that have to be passed during driver registration.
struct driverConstructionProps
{
    size_t rasterMemSize;
    size_t geomMemSize;
    size_t matMemSize;
    size_t swapChainMemSize;
};

/*
    This is the driver interface that every render device has to implement.
    Is is a specialization of the original Criterion pipeline design
*/
struct nativeDriverImplementation
{
    // Driver object creation.
    virtual void OnDriverConstruct( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) = 0;
    virtual void OnDriverCopyConstruct( Interface *engineInterface, void *driverObjMem, const void *srcDriverObjMem, size_t driverMemSize ) = 0;
    virtual void OnDriverDestroy( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) = 0;

    // Helper macros.
#define NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) \
    void On##canonicalName##Construct( Interface *engineInterface, void *driverObjMem, void *objMem, size_t memSize )
#define NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) \
    void On##canonicalName##CopyConstruct( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
#define NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) \
    void On##canonicalName##Destroy( Interface *engineInterface, void *objMem, size_t memSize )

#define NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( canonicalName ) \
    virtual NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) = 0;

#define NATIVE_DRIVER_DEFINE_CONSTRUCT_FORWARD( canonicalName ) \
    NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ); \
    NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ); \
    NATIVE_DRIVER_OBJ_DESTROY( canonicalName );

    // Instanced type creation.
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Raster );
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Geometry );
    NATIVE_DRIVER_DEFINE_CONSTRUCT_VIRTUAL( Material );

    // Default implementation macros.
#define NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( canonicalName, className, driverClassName ) \
    NATIVE_DRIVER_OBJ_CONSTRUCT( canonicalName ) override \
    { \
        new (objMem) className( this, engineInterface, (driverClassName*)driverObjMem ); \
    } \
    NATIVE_DRIVER_OBJ_COPYCONSTRUCT( canonicalName ) override \
    { \
        new (objMem) className( *(const className*)srcObjMem ); \
    } \
    NATIVE_DRIVER_OBJ_DESTROY( canonicalName ) override \
    { \
        ((className*)objMem)->~className(); \
    }

    // Instancing method helpers.
#define NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ) \
    void canonicalName##Instance( Interface *engineInterface, void *driverObjMem, void *objMem, canonicalName *sysObj )
#define NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName ) \
    void canonicalName##Uninstance( Interface *engineInterface, void *driverObjMem, void *objMem )

#define NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( canonicalName ) \
    virtual NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ) = 0; \
    virtual NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName ) = 0;

#define NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( canonicalName ) \
    NATIVE_DRIVER_OBJ_INSTANCE( canonicalName ); \
    NATIVE_DRIVER_OBJ_UNINSTANCE( canonicalName );

    // Instancing declarations.
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Raster );
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Geometry );
    NATIVE_DRIVER_DEFINE_INSTANCING_VIRTUAL( Material );

    // Drivers need swap chains for windowing system output.
#define NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() \
    void SwapChainConstruct( Interface *engineInterface, void *driverObjMem, void *objMem, Window *sysWnd, uint32 frameCount )
#define NATIVE_DRIVER_SWAPCHAIN_DESTROY() \
    void SwapChainDestroy( Interface *engineInterface, void *objMem )

    virtual NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() = 0;
    virtual NATIVE_DRIVER_SWAPCHAIN_DESTROY() = 0;
};

// Driver registration API.
bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf, size_t driverObjSize );
bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf );

};

#endif //_RENDERWARE_DRIVER_FRAMEWORK_