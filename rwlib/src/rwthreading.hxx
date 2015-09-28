// RenderWare Threading shared include.

#include <CExecutiveManager.h>

#include "pluginutil.hxx"

namespace rw
{

struct threadingEnvironment
{
    inline void Initialize( Interface *engineInterface )
    {
        this->nativeMan = NativeExecutive::CExecutiveManager::Create();
    }

    inline void Shutdown( Interface *engineInterface )
    {
        if ( NativeExecutive::CExecutiveManager *nativeMan = this->nativeMan )
        {
            NativeExecutive::CExecutiveManager::Delete( nativeMan );

            this->nativeMan = NULL;
        }
    }

    NativeExecutive::CExecutiveManager *nativeMan;   // (optional) NativeExecutive library handle.
};

typedef PluginDependantStructRegister <threadingEnvironment, RwInterfaceFactory_t> threadingEnvRegister_t;

extern threadingEnvRegister_t threadingEnv;

// Quick function to return the native executive.
inline NativeExecutive::CExecutiveManager* GetNativeExecutive( const EngineInterface *engineInterface )
{
    const threadingEnvironment *threadEnv = threadingEnv.GetConstPluginStruct( engineInterface );

    if ( threadEnv )
    {
        return threadEnv->nativeMan;
    }

    return NULL;
}

// Private API.
void PurgeActiveThreadingObjects( EngineInterface *engineInterface );

};