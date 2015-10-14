// RenderWare warning dispatching and reporting.
// Turns out warnings are a complicated topic that deserves its own source module.
#include "StdInc.h"

#include "rwinterface.hxx"

#include "rwthreading.hxx"

using namespace NativeExecutive;

namespace rw
{

struct warningHandlerThreadEnv
{
    // The purpose of the warning handler stack is to fetch warning output requests and to reroute them
    // so that they make more sense.
    std::vector <WarningHandler*> warningHandlerStack;
};

struct warningHandlerThreadEnvPluginInterface : public threadPluginInterface
{
    bool OnPluginConstruct( CExecThread *theThread, threadPluginOffset pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor pluginId ) override
    {
        void *objMem = pluginId.RESOLVE_STRUCT <void> ( theThread, pluginOffset );

        if ( !objMem )
            return false;

        warningHandlerThreadEnv *env = new (objMem) warningHandlerThreadEnv();

        return ( env != NULL );
    }

    void OnPluginDestruct( CExecThread *theThread, threadPluginOffset pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor pluginId ) override
    {
        warningHandlerThreadEnv *warningEnv = pluginId.RESOLVE_STRUCT <warningHandlerThreadEnv> ( theThread, pluginOffset );

        if ( !warningEnv )
            return;

        warningEnv->~warningHandlerThreadEnv();
    }

    bool OnPluginAssign( CExecThread *dstThread, const CExecThread *srcThread, threadPluginOffset pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor pluginId ) override
    {
        const warningHandlerThreadEnv *srcEnv = pluginId.RESOLVE_STRUCT <warningHandlerThreadEnv> ( srcThread, pluginOffset );
        warningHandlerThreadEnv *dstEnv = pluginId.RESOLVE_STRUCT <warningHandlerThreadEnv> ( dstThread, pluginOffset );

        *dstEnv = *srcEnv;
        return true;
    }
};

struct warningHandlerPlugin
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->_warningEnvThreadPluginOffset = ExecutiveManager::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;

        // Register the per-thread warning handler environment.
        CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

        if ( nativeMan )
        {
            this->_warningEnvThreadPluginOffset =
                nativeMan->RegisterThreadPlugin( sizeof( warningHandlerThreadEnv ), &_warningEnvThreadPluginIntf );
        }
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister the thread env, if registered.
        if ( ExecutiveManager::threadPluginContainer_t::IsOffsetValid( this->_warningEnvThreadPluginOffset ) )
        {
            CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

            if ( nativeMan )
            {
                nativeMan->UnregisterThreadPlugin( this->_warningEnvThreadPluginOffset );
            }
        }
    }

    inline warningHandlerThreadEnv* GetWarningHandlers( CExecThread *theThread ) const
    {
        return ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <warningHandlerThreadEnv> ( theThread, this->_warningEnvThreadPluginOffset );
    }

    warningHandlerThreadEnvPluginInterface _warningEnvThreadPluginIntf;
    threadPluginOffset _warningEnvThreadPluginOffset;
};

static PluginDependantStructRegister <warningHandlerPlugin, RwInterfaceFactory_t> warningHandlerPluginRegister;

void Interface::PushWarning( std::string&& message )
{
    EngineInterface *engineInterface = (EngineInterface*)this;

    scoped_rwlock_writer <rwlock> lock( GetReadWriteLock( engineInterface ) );

    const rwConfigBlock& cfgBlock = GetConstEnvironmentConfigBlock( engineInterface );

    if ( cfgBlock.GetWarningLevel() > 0 )
    {
        // If we have a warning handler, we redirect the message to it instead.
        // The warning handler is supposed to be an internal class that only the library has access to.
        WarningHandler *currentWarningHandler = NULL;
        {
            CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

            if ( nativeMan )
            {
                CExecThread *curThread = nativeMan->GetCurrentThread();

                if ( curThread )
                {
                    warningHandlerPlugin *whandlerEnv = warningHandlerPluginRegister.GetPluginStruct( engineInterface );

                    if ( whandlerEnv )
                    {
                        warningHandlerThreadEnv *threadEnv = whandlerEnv->GetWarningHandlers( curThread );

                        if ( threadEnv )
                        {
                            if ( !threadEnv->warningHandlerStack.empty() )
                            {
                                currentWarningHandler = threadEnv->warningHandlerStack.back();
                            }
                        }
                    }
                }
            }
        }

        if ( currentWarningHandler )
        {
            // Give it the warning.
            currentWarningHandler->OnWarningMessage( std::move( message ) );
        }
        else
        {
            // Else we just post the warning to the runtime.
            if ( WarningManagerInterface *warningMan = cfgBlock.GetWarningManager() )
            {
                warningMan->OnWarning( std::move( message ) );
            }
        }
    }
}

void GlobalPushWarningHandler( EngineInterface *engineInterface, WarningHandler *theHandler )
{
    warningHandlerPlugin *whandlerEnv = warningHandlerPluginRegister.GetPluginStruct( engineInterface );

    if ( whandlerEnv )
    {
        CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

        if ( nativeMan )
        {
            CExecThread *curThread = nativeMan->GetCurrentThread();

            if ( curThread )
            {
                warningHandlerThreadEnv *threadEnv = whandlerEnv->GetWarningHandlers( curThread );

                if ( threadEnv )
                {
                    threadEnv->warningHandlerStack.push_back( theHandler );
                }
            }
        }
    }
}

void GlobalPopWarningHandler( EngineInterface *engineInterface )
{
    warningHandlerPlugin *whandlerEnv = warningHandlerPluginRegister.GetPluginStruct( engineInterface );

    if ( whandlerEnv )
    {
        CExecutiveManager *nativeMan = GetNativeExecutive( engineInterface );

        if ( nativeMan )
        {
            CExecThread *curThread = nativeMan->GetCurrentThread();

            if ( curThread )
            {
                warningHandlerThreadEnv *threadEnv = whandlerEnv->GetWarningHandlers( curThread );

                if ( threadEnv )
                {
                    assert( threadEnv->warningHandlerStack.empty() == false );

                    threadEnv->warningHandlerStack.pop_back();
                }
            }
        }
    }
}

void registerWarningHandlerEnvironment( void )
{
    warningHandlerPluginRegister.RegisterPlugin( engineFactory );
}

};