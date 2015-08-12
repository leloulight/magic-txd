#include "StdInc.h"

#include "rwdriver.hxx"

#include "pluginutil.hxx"

namespace rw
{

inline void SafeDeleteType( EngineInterface *engineInterface, RwTypeSystem::typeInfoBase *theType )
{
    if ( theType )
    {
        engineInterface->typeSystem.DeleteType( theType );
    }
}

struct nativeDriverTypeInterface : public RwTypeSystem::typeInterface
{
    inline nativeDriverTypeInterface( EngineInterface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->isRegistered = false;
        this->driverMemSize = 0;
        this->driverImpl = NULL;

        // Every driver has to export functionality for instanced objects.
        this->driverType = NULL;
        this->rasterType = NULL;
        this->geometryType = NULL;
        this->materialType = NULL;
        this->swapChainType = NULL;
    }

    inline ~nativeDriverTypeInterface( void )
    {
        // Delete our driver types.
        SafeDeleteType( engineInterface, this->rasterType );
        SafeDeleteType( engineInterface, this->geometryType );
        SafeDeleteType( engineInterface, this->materialType );
        SafeDeleteType( engineInterface, this->swapChainType );

        SafeDeleteType( engineInterface, this->driverType );

        if ( this->isRegistered )
        {
            LIST_REMOVE( this->node );

            this->isRegistered = false;
        }
    }

    // Helper macro for defining driver object type interfaces (Geometry, etc)
    struct driver_obj_constr_params
    {
        void *driverObj;
    };

#define NATIVE_DRIVER_OBJ_TYPEINTERFACE( canonicalName ) \
    struct nativeDriver##canonicalName##TypeInterface : public RwTypeSystem::typeInterface \
    { \
        void Construct( void *mem, EngineInterface *engineInterface, void *construction_params ) const override \
        { \
            driver_obj_constr_params *params = (driver_obj_constr_params*)construction_params; \
            driverType->driverImpl->On##canonicalName##Construct( engineInterface, params->driverObj, mem, this->objMemSize ); \
        } \
        void CopyConstruct( void *mem, const void *srcMem ) const override \
        { \
            driverType->driverImpl->On##canonicalName##CopyConstruct( driverType->engineInterface, mem, srcMem, this->objMemSize ); \
        } \
        void Destruct( void *mem ) const override \
        { \
            driverType->driverImpl->On##canonicalName##Destroy( driverType->engineInterface, mem, this->objMemSize ); \
        } \
        size_t GetTypeSize( EngineInterface *engineInterface, void *construction_params ) const override \
        { \
            return this->objMemSize; \
        } \
        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override \
        { \
            return this->objMemSize; \
        } \
        size_t objMemSize; \
        nativeDriverTypeInterface *driverType; \
    }; \
    nativeDriver##canonicalName##TypeInterface _driverType##canonicalName;

    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Raster );
    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Geometry );
    NATIVE_DRIVER_OBJ_TYPEINTERFACE( Material );

    struct swapchain_const_params
    {
        Driver *driverObj;
        Window *sysWnd;
        uint32 frameCount;
    };

    // Special swap chain type object.
    struct nativeDriverSwapChainTypeInterface : public RwTypeSystem::typeInterface
    {
        void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
        {
            swapchain_const_params *params = (swapchain_const_params*)construct_params;

            Driver *theDriver = params->driverObj;
            Window *sysWnd = params->sysWnd;

            // First we construct the swap chain.
            DriverSwapChain *swapChain = new (mem) DriverSwapChain( engineInterface, theDriver, sysWnd );

            this->driverType->driverImpl->SwapChainConstruct(
                engineInterface,
                theDriver->GetImplementation(),
                swapChain->GetImplementation(),
                sysWnd, params->frameCount
            );
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            throw RwException( "cannot clone swap chains" );
        }

        void Destruct( void *mem ) const override
        {
            DriverSwapChain *swapChain = (DriverSwapChain*)mem;

            // First destroy the implementation.
            this->driverType->driverImpl->SwapChainDestroy(
                this->driverType->engineInterface,
                swapChain->GetImplementation()
            );

            // Now destroy the actual thing.
            swapChain->~DriverSwapChain();
        }

        size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override
        {
            return this->objMemSize + sizeof( DriverSwapChain );
        }

        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override
        {
            return this->objMemSize + sizeof( DriverSwapChain );
        }

        size_t objMemSize;
        nativeDriverTypeInterface *driverType;
    };
    nativeDriverSwapChainTypeInterface _driverSwapChainType;

    void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
    {
        // First construct the driver.
        Driver *theDriver = new (mem) Driver( engineInterface );

        this->driverImpl->OnDriverConstruct(
            engineInterface,
            theDriver->GetImplementation(), this->driverMemSize
        );
    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {
        const Driver *srcDriver = (const Driver*)srcMem;

        // First clone the driver abstract object.
        Driver *newDriver = new (mem) Driver( *srcDriver );

        this->driverImpl->OnDriverCopyConstruct(
            this->engineInterface,
            newDriver->GetImplementation(),
            srcDriver + 1,
            this->driverMemSize
        );
    }

    void Destruct( void *mem ) const override
    {
        // First destroy the implementation.
        Driver *theDriver = (Driver*)mem;

        this->driverImpl->OnDriverDestroy(
            this->engineInterface,
            theDriver->GetImplementation(), this->driverMemSize
        );

        // Now the abstract thing.
        theDriver->~Driver();
    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override
    {
        return this->driverMemSize + sizeof( Driver );
    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override
    {
        return this->driverMemSize + sizeof( Driver );
    }

    EngineInterface *engineInterface;
    size_t driverMemSize;
    nativeDriverImplementation *driverImpl;

    // Private manager data.
    RwTypeSystem::typeInfoBase *driverType;         // actual driver type
    RwTypeSystem::typeInfoBase *rasterType;         // type of instanced raster
    RwTypeSystem::typeInfoBase *geometryType;       // type of instanced geometry
    RwTypeSystem::typeInfoBase *materialType;       // type of instanced material
    RwTypeSystem::typeInfoBase *swapChainType;      // type of windowing system buffer

    bool isRegistered;
    RwListEntry <nativeDriverTypeInterface> node;
};

struct driverEnvironment
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->baseDriverType =
            engineInterface->typeSystem.RegisterAbstractType <nativeDriverImplementation> ( "driver" );

        LIST_CLEAR( this->driverTypeList.root );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Delete all sub driver types.
        while ( !LIST_EMPTY( this->driverTypeList.root ) )
        {
            nativeDriverTypeInterface *item = LIST_GETITEM( nativeDriverTypeInterface, this->driverTypeList.root.next, node );

            delete item;
        }

        if ( RwTypeSystem::typeInfoBase *baseDriverType = this->baseDriverType )
        {
            engineInterface->typeSystem.DeleteType( baseDriverType );
        }
    }

    RwTypeSystem::typeInfoBase *baseDriverType;

    RwList <nativeDriverTypeInterface> driverTypeList;

    inline nativeDriverTypeInterface* FindDriverTypeInterface( nativeDriverImplementation *impl ) const
    {
        LIST_FOREACH_BEGIN( nativeDriverTypeInterface, this->driverTypeList.root, node )

            if ( item->driverImpl == impl )
            {
                return item;
            }

        LIST_FOREACH_END

        return NULL;
    }

    static inline nativeDriverTypeInterface* GetDriverTypeInterface( Driver *theDriver )
    {
        GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( theDriver );

        if ( rtObj )
        {
            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            if ( typeInfo )
            {
                return dynamic_cast <nativeDriverTypeInterface*> ( typeInfo->tInterface );
            }
        }

        return NULL;
    }
};

static PluginDependantStructRegister <driverEnvironment, RwInterfaceFactory_t> driverEnvRegister;

bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf, size_t driverObjSize )
{
    driverEnvironment *driverEnv = driverEnvRegister.GetPluginStruct( engineInterface );

    bool success = false;

    if ( driverEnv )
    {
        // First create the type interface that will manage the new driver.
        // This is required to properly register the driver into our DTS.
        nativeDriverTypeInterface *driverTypeInterface = new nativeDriverTypeInterface( engineInterface );

        if ( driverTypeInterface )
        {
            // Set up the type interface.
            driverTypeInterface->driverImpl = driverIntf;
            driverTypeInterface->driverMemSize = driverObjSize;

            // Initialize the sub types.
            driverTypeInterface->_driverTypeRaster.driverType = driverTypeInterface;
            driverTypeInterface->_driverTypeRaster.objMemSize = props.rasterMemSize;

            driverTypeInterface->_driverTypeGeometry.driverType = driverTypeInterface;
            driverTypeInterface->_driverTypeGeometry.objMemSize = props.geomMemSize;

            driverTypeInterface->_driverTypeMaterial.driverType = driverTypeInterface;
            driverTypeInterface->_driverTypeMaterial.objMemSize = props.matMemSize;

            driverTypeInterface->_driverSwapChainType.driverType = driverTypeInterface;
            driverTypeInterface->_driverSwapChainType.objMemSize = props.swapChainMemSize;

            try
            {
                // Register the driver type.
                RwTypeSystem::typeInfoBase *driverType = engineInterface->typeSystem.RegisterType( typeName, driverTypeInterface, driverEnv->baseDriverType );
                
                if ( driverType )
                {
                    driverTypeInterface->driverType = driverType;

                    // Create sub types.
                    // Here we use the type hierachy to define types that belong to each other in a system.
                    // Previously at RwObject we used the type hierarchy to define types that inherit from each other.
                    driverTypeInterface->rasterType = engineInterface->typeSystem.RegisterType( "raster", &driverTypeInterface->_driverTypeRaster, driverType );
                    driverTypeInterface->geometryType = engineInterface->typeSystem.RegisterType( "geometry", &driverTypeInterface->_driverTypeGeometry, driverType );  
                    driverTypeInterface->materialType = engineInterface->typeSystem.RegisterType( "material", &driverTypeInterface->_driverTypeMaterial, driverType );
                    driverTypeInterface->swapChainType = engineInterface->typeSystem.RegisterType( "swapchain", &driverTypeInterface->_driverSwapChainType, driverType );

                    // Register our driver.
                    LIST_INSERT( driverEnv->driverTypeList.root, driverTypeInterface->node );

                    driverTypeInterface->isRegistered = true;

                    success = true;
                }
            }
            catch( ... )
            {
                delete driverTypeInterface;

                throw;
            }

            if ( !success )
            {
                delete driverTypeInterface;
            }
        }
    }

    return success;
}

bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf )
{
    driverEnvironment *driverEnv = driverEnvRegister.GetPluginStruct( engineInterface );

    bool success = false;

    if ( driverEnv )
    {
        nativeDriverTypeInterface *typeInterface = driverEnv->FindDriverTypeInterface( driverIntf );

        if ( typeInterface )
        {
            delete typeInterface;

            success = true;
        }
    }

    return success;
}

// Public driver API.
Driver* CreateDriver( Interface *intf, const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    Driver *driverOut = NULL;

    driverEnvironment *driverEnv = driverEnvRegister.GetPluginStruct( engineInterface );

    if ( driverEnv )
    {
        // Get a the type of the driver we want to create.
        RwTypeSystem::typeInfoBase *driverType = engineInterface->typeSystem.FindTypeInfo( typeName, driverEnv->baseDriverType );

        if ( driverType )
        {
            // Construct the object and return it.
            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, driverType, NULL );

            if ( rtObj )
            {
                // Lets return stuff.
                driverOut = (Driver*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return driverOut;
}

void DestroyDriver( Interface *intf, Driver *theDriver )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( theDriver );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

// Driver interface.
DriverSwapChain* Driver::CreateSwapChain( Window *outputWindow, uint32 frameCount )
{
    DriverSwapChain *swapChainOut = NULL;

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    if ( nativeDriverTypeInterface *driverInfo = driverEnvironment::GetDriverTypeInterface( this ) )
    {
        // Make the friggin swap chain.
        if ( RwTypeSystem::typeInfoBase *swapChainType = driverInfo->swapChainType )
        {
            // Create it.
            nativeDriverTypeInterface::swapchain_const_params params;
            params.driverObj = this;
            params.sysWnd = outputWindow;
            params.frameCount = frameCount;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, swapChainType, &params );

            if ( rtObj )
            {
                swapChainOut = (DriverSwapChain*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }
        }
    }

    return swapChainOut;
}

void Driver::DestroySwapChain( DriverSwapChain *swapChain )
{
    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    // Simply generically destroy it.
    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( swapChain );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

// Registration of driver implementations.
extern void registerD3D12DriverImplementation( void );

void registerDriverEnvironment( void )
{
    // Put our structure into the engine interface.
    driverEnvRegister.RegisterPlugin( engineFactory );

    // TODO: register all driver implementations.
    registerD3D12DriverImplementation();
}

};