#ifndef _RENDERWARE_RASTER_INTERNALS_
#define _RENDERWARE_RASTER_INTERNALS_

#include "pluginutil.hxx"

namespace rw
{

// Internal raster plugins for consistency management.
struct rasterConsistencyEnv
{
    struct dynamic_rwlock
    {
        inline void Initialize( Raster *ras )
        {
            CreatePlacedReadWriteLock( ras->engineInterface, this );
        }

        inline void Shutdown( Raster *ras )
        {
            ClosePlacedReadWriteLock( ras->engineInterface, (rwlock*)this );
        }
    };

    inline void Initialize( EngineInterface *intf )
    {
        size_t rwlock_struct_size = GetReadWriteLockStructSize( intf );

        this->_rasterConsistencyPluginOffset =
            intf->rasterPluginFactory.RegisterDependantStructPlugin <dynamic_rwlock> ( EngineInterface::rasterPluginFactory_t::ANONYMOUS_PLUGIN_ID, rwlock_struct_size );
    }

    inline void Shutdown( EngineInterface *intf )
    {
        if ( EngineInterface::rasterPluginFactory_t::IsOffsetValid( this->_rasterConsistencyPluginOffset ) )
        {
            intf->rasterPluginFactory.UnregisterPlugin( this->_rasterConsistencyPluginOffset );
        }
    }

    EngineInterface::rasterPluginFactory_t::pluginOffset_t _rasterConsistencyPluginOffset;
};

typedef PluginDependantStructRegister <rasterConsistencyEnv, RwInterfaceFactory_t> rasterConsistencyRegister_t;

extern rasterConsistencyRegister_t rasterConsistencyRegister;

inline rwlock* GetRasterLock( const rw::Raster *ras )
{
    rasterConsistencyEnv *consisEnv = rasterConsistencyRegister.GetPluginStruct( (EngineInterface*)ras->engineInterface );

    if ( consisEnv )
    {
        return (rwlock*)EngineInterface::rasterPluginFactory_t::RESOLVE_STRUCT <rwlock> ( ras, consisEnv->_rasterConsistencyPluginOffset );
    }

    return NULL;
}

};

#endif //_RENDERWARE_RASTER_INTERNALS_