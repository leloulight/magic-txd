/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.hxx
*  PURPOSE:     Fiber environment internals
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_FIBER_INTERNALS_
#define _EXECUTIVE_MANAGER_FIBER_INTERNALS_

BEGIN_NATIVE_EXECUTIVE

// Management variables for the fiber-stack-per-thread extension.
struct threadFiberStackPluginInfo
{
    struct fiberArrayAllocManager
    {
        AINLINE void InitField( CFiber*& fiber )
        {
            fiber = NULL;
        }
    };
    typedef growableArray <CFiber*, 2, 0, fiberArrayAllocManager, size_t> fiberArray;

    // Fiber stacks per thread!
    fiberArray fiberStack;
};

typedef StaticPluginClassFactory <CFiber> fiberFactory_t;

struct privateFiberEnvironment
{
    inline privateFiberEnvironment( void )
    {
        this->threadFiberStackPluginOffset = ExecutiveManager::threadPluginContainer_t::INVALID_PLUGIN_OFFSET;
    }

    inline void Initialize( CExecutiveManager *manager )
    {
        this->threadFiberStackPluginOffset =
            manager->threadPlugins.RegisterStructPlugin <threadFiberStackPluginInfo> ( THREAD_PLUGIN_FIBER_STACK );
    }

    inline void Shutdown( CExecutiveManager *manager )
    {
        if ( ExecutiveManager::threadPluginContainer_t::IsOffsetValid( this->threadFiberStackPluginOffset ) )
        {
            manager->threadPlugins.UnregisterPlugin( this->threadFiberStackPluginOffset );
        }
    }

    inline void operator = ( const privateFiberEnvironment& right )
    {
        assert( 0 );
    }

    fiberFactory_t fiberFact;   // boys and girls, its fact!

    ExecutiveManager::threadPluginContainer_t::pluginOffset_t threadFiberStackPluginOffset;
};

typedef PluginDependantStructRegister <privateFiberEnvironment, executiveManagerFactory_t> privateFiberEnvironmentRegister_t;

extern privateFiberEnvironmentRegister_t privateFiberEnvironmentRegister;

END_NATIVE_EXECUTIVE

#endif //_EXECUTIVE_MANAGER_FIBER_INTERNALS_