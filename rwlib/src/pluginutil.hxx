#ifndef _PLUGIN_UTILITIES_
#define _PLUGIN_UTILITIES_

#include <PluginHelpers.h>

#if 0

struct _rwInterfaceFactoryMeta
{
    typedef RwInterfaceFactory_t factoryType;

    inline factoryType& ResolveFactoryLink( lua_config& cfgStruct )
    {
        return cfgStruct.globalStateFactory;
    }

    template <typename structType>
    inline RwInterfacePluginOffset_t RegisterPlugin( lua_config *cfgStruct, unsigned int pluginId )
    {
        return cfgStruct->globalStateFactory.RegisterStructPlugin <structType> ( pluginId );
    }
};


template <typename structType, typename factoryType, typename hostType>
struct rwInterfaceFactoryMeta : public factoryMetaDefault <hostType, factoryType, structType, _rwInterfaceFactoryMeta>
{
};

struct _rwInterfaceDependantStructFactoryMeta
{
    typedef RwInterfaceFactory_t factoryType;

    inline factoryType& ResolveFactoryLink( lua_config& cfgStruct )
    {
        return cfgStruct.globalStateFactory;
    }

    template <typename structType>
    inline RwInterfacePluginOffset_t RegisterPlugin( lua_config *cfgStruct, unsigned int pluginId )
    {
        return cfgStruct->globalStateFactory.RegisterDependantStructPlugin <structType> ( pluginId );
    }
};

template <typename structType, typename factoryType, typename hostType>
struct globalStateDependantStructFactoryMeta : public factoryMetaDefault <hostType, factoryType, structType, _globalStateDependantStructFactoryMeta>
{
};

#endif

#endif //_PLUGIN_UTILITIES_