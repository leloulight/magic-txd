#ifndef _PLUGIN_HELPERS_
#define _PLUGIN_HELPERS_

// Helper structure to handle plugin dependant struct registration.
template <typename structType, typename factoryType>
class PluginDependantStructRegister
{
    typedef typename factoryType::pluginOffset_t pluginOffset_t;

    factoryType *hostFactory;

    pluginOffset_t structPluginOffset;

    unsigned int pluginId;

public:
    inline PluginDependantStructRegister( factoryType& theFactory, unsigned int pluginId = factoryType::ANONYMOUS_PLUGIN_ID )
    {
        this->hostFactory = NULL;
        this->pluginId = pluginId;
        this->structPluginOffset = factoryType::INVALID_PLUGIN_OFFSET;

        RegisterPlugin( theFactory );
    }

    inline PluginDependantStructRegister( unsigned int pluginId = factoryType::ANONYMOUS_PLUGIN_ID )
    {
        this->hostFactory = NULL;
        this->pluginId = pluginId;
        this->structPluginOffset = factoryType::INVALID_PLUGIN_OFFSET;
    }

    inline ~PluginDependantStructRegister( void )
    {
        if ( this->hostFactory && factoryType::IsOffsetValid( this->structPluginOffset ) )
        {
            this->hostFactory->UnregisterPlugin( this->structPluginOffset );
        }
    }

    inline void RegisterPlugin( factoryType& theFactory )
    {
        this->structPluginOffset =
            theFactory.RegisterDependantStructPlugin <structType> ( this->pluginId );

        this->hostFactory = &theFactory;
    }

    inline structType* GetPluginStruct( typename factoryType::hostType_t *hostObj )
    {
        return factoryType::RESOLVE_STRUCT <structType> ( hostObj, this->structPluginOffset );
    }

    inline const structType* GetConstPluginStruct( const typename factoryType::hostType_t *hostObj )
    {
        return factoryType::RESOLVE_STRUCT <const structType> ( hostObj, this->structPluginOffset );
    }

    inline bool IsRegistered( void ) const
    {
        return ( factoryType::IsOffsetValid( this->structPluginOffset ) == true );
    }
};

// Helper structure for registering a plugin struct that exists depending on runtime conditions.
// It does otherwise use the same semantics as a dependant struct register.


template <typename hostType, typename factoryType, typename structType, typename metaInfoType>
struct factoryMetaDefault
{
    typedef factoryType factoryType;

    typedef factoryType endingPointFactory_t;
    typedef typename endingPointFactory_t::pluginOffset_t endingPointPluginOffset_t;

    endingPointPluginOffset_t endingPointPluginOffset;

    metaInfoType metaInfo;

    inline void Initialize( hostType *namespaceObj )
    {
        // Register our plugin.
        this->endingPointPluginOffset =
            metaInfo.RegisterPlugin <structType> ( namespaceObj, endingPointFactory_t::ANONYMOUS_PLUGIN_ID );
    }

    inline void Shutdown( hostType *namespaceObj )
    {
        // Unregister our plugin again.
        if ( this->endingPointPluginOffset != endingPointFactory_t::INVALID_PLUGIN_OFFSET )
        {
            endingPointFactory_t& endingPointFactory =
                metaInfo.ResolveFactoryLink( *namespaceObj );

            endingPointFactory.UnregisterPlugin( this->endingPointPluginOffset );
        }
    }
};

// Some really obscure thing I have found no use for yet.
// It can be used to determine plugin registration on factory construction, based on the environment.
template <typename hostType, typename factoryType, typename structType, typename metaInfoType, typename registerConditionalType>
struct factoryMetaConditional
{
    typedef factoryType factoryType;

    typedef factoryType endingPointFactory_t;
    typedef typename endingPointFactory_t::pluginOffset_t endingPointPluginOffset_t;

    endingPointPluginOffset_t endingPointPluginOffset;

    metaInfoType metaInfo;

    inline void Initialize( hostType *namespaceObj )
    {
        endingPointPluginOffset_t thePluginOffset = endingPointFactory_t::INVALID_PLUGIN_OFFSET;

        if ( registerConditionalType::MeetsCondition( namespaceObj ) )
        {
            // Register our plugin.
            thePluginOffset =
                metaInfo.RegisterPlugin <structType> ( namespaceObj, endingPointFactory_t::ANONYMOUS_PLUGIN_ID );
        }

        this->endingPointPluginOffset = thePluginOffset;
    }

    inline void Shutdown( hostType *namespaceObj )
    {
        // Unregister our plugin again.
        if ( this->endingPointPluginOffset != endingPointFactory_t::INVALID_PLUGIN_OFFSET )
        {
            endingPointFactory_t& endingPointFactory =
                metaInfo.ResolveFactoryLink( *namespaceObj );

            endingPointFactory.UnregisterPlugin( this->endingPointPluginOffset );
        }
    }
};

// Helper structure to register a plugin on the interface of another plugin.
template <typename structType, typename factoryMetaType, typename hostFactoryType>
class PluginConnectingBridge
{
public:
    typedef typename factoryMetaType::factoryType endingPointFactory_t;
    typedef typename endingPointFactory_t::pluginOffset_t endingPointPluginOffset_t;

    typedef typename endingPointFactory_t::hostType_t endingPointType_t;

    typedef hostFactoryType hostFactory_t;
    typedef typename hostFactory_t::pluginOffset_t hostPluginOffset_t;

    typedef typename hostFactory_t::hostType_t hostType_t;

private:
    PluginDependantStructRegister <factoryMetaType, hostFactoryType> connectingBridgePlugin;

    typedef factoryMetaType hostFactoryDependantStruct;
    
public:

    inline PluginConnectingBridge( void )
    {
        return;
    }

    inline PluginConnectingBridge( hostFactory_t& hostFactory ) : connectingBridgePlugin( hostFactory )
    {
        return;
    }

    inline void RegisterPluginStruct( hostFactory_t& theFactory )
    {
        connectingBridgePlugin.RegisterPlugin( theFactory );
    }

    inline factoryMetaType* GetMetaStruct( hostType_t *host )
    {
        return connectingBridgePlugin.GetPluginStruct( host );
    }

    inline structType* GetPluginStructFromMetaStruct( factoryMetaType *metaStruct, endingPointType_t *endingPoint )
    {
        return endingPointFactory_t::RESOLVE_STRUCT <structType> ( endingPoint, metaStruct->endingPointPluginOffset );
    }

    inline structType* GetPluginStruct( hostType_t *host, endingPointType_t *endingPoint )
    {
        structType *resultStruct = NULL;
        {
            hostFactoryDependantStruct *metaStruct = GetMetaStruct( host );

            if ( metaStruct )
            {
                resultStruct = GetPluginStructFromMetaStruct( metaStruct, endingPoint );
            }
        }
        return resultStruct;
    }
};

#endif //_PLUGIN_HELPERS_