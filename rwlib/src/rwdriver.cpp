#include "StdInc.h"

#include "rwdriver.hxx"

#include "pluginutil.hxx"

namespace rw
{

struct nativeDriverTypeInterface : public RwTypeSystem::typeInterface
{
    void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
    {

    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {

    }

    void Destruct( void *mem ) const override
    {

    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override
    {

    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override
    {

    }

    size_t driverMemSize;
    nativeDriverImplementation *driverImpl;
};

struct driverEnvironment
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->baseDriverType =
            engineInterface->typeSystem.RegisterAbstractType <nativeDriverImplementation> ( "driver" );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( RwTypeSystem::typeInfoBase *baseDriverType = this->baseDriverType )
        {
            engineInterface->typeSystem.DeleteType( baseDriverType );
        }
    }

    RwTypeSystem::typeInfoBase *baseDriverType;
};

static PluginDependantStructRegister <driverEnvironment, RwInterfaceFactory_t> driverEnvRegister;

bool RegisterDriver( EngineInterface *engineInterface, const char *typeName, const driverConstructionProps& props, nativeDriverImplementation *driverIntf )
{
    return false;
}

bool UnregisterDriver( EngineInterface *engineInterface, nativeDriverImplementation *driverIntf )
{
    return false;
}

void registerDriverEnvironment( void )
{
    // Put our structure into the engine interface.
    driverEnvRegister.RegisterPlugin( engineFactory );

    // TODO: register all driver implementations.
}

};