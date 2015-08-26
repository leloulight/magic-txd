/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.hazards.hxx
*  PURPOSE:     Thread hazard management internals, to prevent deadlocks
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _STACK_HAZARD_MANAGEMENT_INTERNALS_
#define _STACK_HAZARD_MANAGEMENT_INTERNALS_

#include "CExecutiveManager.fiber.hxx"

BEGIN_NATIVE_EXECUTIVE

// Struct that is registered at hazardous objects, basically anything that hosts CPU time.
// This cannot be a dependant struct.
struct stackObjectHazardRegistry abstract
{
    inline void Initialize( CExecutiveManager *manager )
    {
        this->hazardStack.Init();

        this->rwlockHazards = manager->CreateReadWriteLock();
    }

    inline void Shutdown( CExecutiveManager *manager )
    {
        manager->CloseReadWriteLock( this->rwlockHazards );

        this->hazardStack.Shutdown();
    }

private:
    struct hazardStackEntry
    {
        hazardPreventionInterface *intf;
    };

    iterativeGrowableArray <hazardStackEntry, 8, 0, size_t> hazardStack;

    // Lock that is used to safely manage the hazard stack.
    CReadWriteLock *rwlockHazards;

public:
    inline void PushHazard( hazardPreventionInterface *intf )
    {
        hazardStackEntry entry;
        entry.intf = intf;

        CReadWriteWriteContextSafe <CReadWriteLock> hazardCtx( this->rwlockHazards );

        this->hazardStack.AddItem( entry );
    }

    inline void PopHazard( void )
    {
        CReadWriteWriteContextSafe <CReadWriteLock> hazardCtx( this->rwlockHazards );

        this->hazardStack.Pop();
    }

    inline void PurgeHazards( CExecutiveManager *manager )
    {
        CReadWriteLock *rwlock = this->rwlockHazards;

        while ( true )
        {
            hazardStackEntry entry;

            bool gotEntry;
            {
                CReadWriteWriteContextSafe <CReadWriteLock> hazardCtx( rwlock );

                gotEntry = hazardStack.Pop( entry );
            }

            if ( gotEntry )
            {
                // Process the hazard.
                entry.intf->TerminateHazard();
            }
            else
            {
                break;
            }
        }
    }
};

// Then we need an environment that takes care of all hazardous objects.
struct executiveHazardManagerEnv
{
private:
    struct stackObjectHazardRegistry_fiber : public stackObjectHazardRegistry
    {
        inline void Initialize( CFiber *fiber )
        {
            stackObjectHazardRegistry::Initialize( fiber->manager );
        }

        inline void Shutdown( CFiber *fiber )
        {
            stackObjectHazardRegistry::Shutdown( fiber->manager );
        }
    };

    struct stackObjectHazardRegistry_thread : public stackObjectHazardRegistry
    {
        inline void Initialize( CExecThread *thread )
        {
            stackObjectHazardRegistry::Initialize( thread->manager );
        }

        inline void Shutdown( CExecThread *thread )
        {
            stackObjectHazardRegistry::Shutdown( thread->manager );
        }
    };

public:
    // We want to register the hazard object struct in threads and fibers.
    fiberFactory_t::pluginOffset_t _fiberHazardOffset;
    ExecutiveManager::threadPluginContainer_t::pluginOffset_t _threadHazardOffset;

    inline void Initialize( CExecutiveManager *manager )
    {
        fiberFactory_t::pluginOffset_t fiberPluginOff = fiberFactory_t::INVALID_PLUGIN_OFFSET;

        privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

        if ( fiberEnv )
        {
            fiberPluginOff = 
                fiberEnv->fiberFact.RegisterDependantStructPlugin <stackObjectHazardRegistry_fiber> ( fiberFactory_t::ANONYMOUS_PLUGIN_ID );
        }

        this->_fiberHazardOffset = fiberPluginOff;

        this->_threadHazardOffset =
            manager->threadPlugins.RegisterDependantStructPlugin <stackObjectHazardRegistry_thread> ( ExecutiveManager::threadPluginContainer_t::ANONYMOUS_PLUGIN_ID );
    }

    inline void Shutdown( CExecutiveManager *manager )
    {
        if ( ExecutiveManager::threadPluginContainer_t::IsOffsetValid( this->_threadHazardOffset ) )
        {
            manager->threadPlugins.UnregisterPlugin( this->_threadHazardOffset );
        }

        if ( fiberFactory_t::IsOffsetValid( this->_fiberHazardOffset ) )
        {
            privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

            if ( fiberEnv )
            {
                fiberEnv->fiberFact.UnregisterPlugin( this->_fiberHazardOffset );
            }
        }
    }

private:
    inline stackObjectHazardRegistry* GetFiberHazardRegistry( CFiber *fiber )
    {
        stackObjectHazardRegistry *reg = NULL;

        if ( fiberFactory_t::IsOffsetValid( this->_fiberHazardOffset ) )
        {
            reg = fiberFactory_t::RESOLVE_STRUCT <stackObjectHazardRegistry_fiber> ( fiber, this->_fiberHazardOffset );
        }

        return reg;
    }

    inline stackObjectHazardRegistry* GetThreadHazardRegistry( CExecThread *thread )
    {
        stackObjectHazardRegistry *reg = NULL;

        if ( ExecutiveManager::threadPluginContainer_t::IsOffsetValid( this->_threadHazardOffset ) )
        {
            reg = ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <stackObjectHazardRegistry_thread> ( thread, this->_threadHazardOffset );
        }

        return reg;
    }

public:
    inline void PurgeThreadHazards( CExecThread *theThread )
    {
        CExecutiveManager *execManager = theThread->manager;

        // First the thread stack.
        {
            stackObjectHazardRegistry *reg = this->GetThreadHazardRegistry( theThread );

            if ( reg )
            {
                reg->PurgeHazards( execManager );
            }
        }

        // Now the fiber stack.
        {
            threadFiberStackIterator fiberIter( theThread );

            while ( fiberIter.IsEnd() == false )
            {
                CFiber *curFiber = fiberIter.Resolve();

                if ( curFiber )
                {
                    stackObjectHazardRegistry *reg = this->GetFiberHazardRegistry( curFiber );

                    if ( reg )
                    {
                        reg->PurgeHazards( execManager );
                    }
                }

                fiberIter.Increment();
            }
        }
    }

    inline stackObjectHazardRegistry* GetThreadCurrentHazardRegistry( CExecThread *theThread )
    {
        // First we try any active fiber.
        CFiber *currentFiber = theThread->GetCurrentFiber();

        if ( currentFiber )
        {
            return this->GetFiberHazardRegistry( currentFiber );
        }

        return this->GetThreadHazardRegistry( theThread );
    }

    inline stackObjectHazardRegistry* GetCurrentHazardRegistry( CExecutiveManager *manager )
    {
        return this->GetThreadHazardRegistry( manager->GetCurrentThread() );
    }
};

typedef PluginDependantStructRegister <executiveHazardManagerEnv, executiveManagerFactory_t> executiveHazardManagerEnvRegister_t;

extern executiveHazardManagerEnvRegister_t executiveHazardManagerEnvRegister;

END_NATIVE_EXECUTIVE

#endif //_STACK_HAZARD_MANAGEMENT_INTERNALS_