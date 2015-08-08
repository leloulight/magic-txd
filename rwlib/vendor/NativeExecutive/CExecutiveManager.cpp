/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.cpp
*  PURPOSE:     MTA thread and fiber execution manager for workload smoothing
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

BEGIN_NATIVE_EXECUTIVE

static void __cdecl _FiberTerm( FiberStatus *status )
{
    CFiber *fiber = (CFiber*)status;

    fiber->manager->TerminateFiber( fiber );
}

static void __stdcall _FiberProc( FiberStatus *status )
{
    CFiber *fiber = (CFiber*)status;

    try
    {
        fiber->yield();

        fiber->callback( fiber, fiber->userdata );
    }
    catch( fiberTerminationException& )
    {
        // The runtime requested the fiber to terminate.
    }
    catch( ... )
    {
        // An unexpected error was triggered.
    }
}

static void* fiberMemAllocate( size_t memSize )
{
    return malloc( memSize );
}

static void fiberMemFree( void *ptr )
{
    return free( ptr );
}

namespace ExecutiveManager
{
    nativeExecutiveAllocator moduleAllocator;
};

// Factory API.
executiveManagerFactory_t executiveManagerFactory;

// Make sure we register all plugins.
extern void registerThreadPlugin( void );
extern void registerFiberPlugin( void );

static bool _hasInitialized = false;

CExecutiveManager* CExecutiveManager::Create( void )
{
    if ( !_hasInitialized )
    {
        // Register all plugins.
        registerFiberPlugin();
        registerThreadPlugin();

        _hasInitialized = true;
    }

    CExecutiveManagerNative *resultObj = executiveManagerFactory.Construct( ExecutiveManager::moduleAllocator );

    if ( resultObj )
    {
        // Initialize sub modules
        resultObj->InitializeTasks();
    }

    return resultObj;
}

void CExecutiveManager::Delete( CExecutiveManager *manager )
{
    // Shutdown sub modules.
    manager->ShutdownTasks();

    executiveManagerFactory.Destroy( ExecutiveManager::moduleAllocator, (CExecutiveManagerNative*)manager );
}

CExecutiveManager::CExecutiveManager( void )
{
    LIST_CLEAR( fibers.root );
    LIST_CLEAR( groups.root );

    // Set up runtime callbacks.
    ExecutiveFiber::setmemfuncs( fiberMemAllocate, fiberMemFree );

    defGroup = new CExecutiveGroup( this );

    frameTime = ExecutiveManager::GetPerformanceTimer();
    frameDuration = 0;

    // Initialize core modules.
    InitThreads();
}

CExecutiveManager::~CExecutiveManager( void )
{
    // Destroy all groups.
    while ( !LIST_EMPTY( groups.root ) )
    {
        CExecutiveGroup *group = LIST_GETITEM( CExecutiveGroup, groups.root.next, managerNode );

        delete group;
    }

    // Destroy all executive runtimes.
    while ( !LIST_EMPTY( fibers.root ) )
    {
        CFiber *fiber = LIST_GETITEM( CFiber, fibers.root.next, node );

        CloseFiber( fiber );
    }

    // Shutdown core modules.
    ShutdownThreads();
}

CFiber* CExecutiveManager::CreateFiber( CFiber::fiberexec_t proc, void *userdata, size_t stackSize )
{
    CFiber *fiber = new CFiber( this, NULL, NULL );

    // Make sure we have an appropriate stack size.
    if ( stackSize != 0 )
    {
        // At least two hundred bytes.
        stackSize = std::max( (size_t)200, stackSize );
    }

    Fiber *runtime = ExecutiveFiber::newfiber( fiber, stackSize, _FiberProc, _FiberTerm );

    // Set first step into it, so the fiber can set itself up.
    fiber->runtime = runtime;
    fiber->resume();

    // Set the function that should be executed next cycle.
    fiber->callback = proc;
    fiber->userdata = userdata;

    // Add it to our manager list.
    LIST_APPEND( fibers.root, fiber->node );

    // Add it to the default fiber group.
    // The user can add it to another if he wants.
    defGroup->AddFiberNative( fiber );

    // Return it.
    return fiber;
}

static void _FiberExceptTerminate( CFiber *fiber )
{
    throw fiberTerminationException( fiber );
}

void CExecutiveManager::TerminateFiber( CFiber *fiber )
{
    if ( !fiber->runtime )
        return;

    Fiber *env = fiber->runtime;

    if ( fiber->status != FIBER_TERMINATED )
    {
        // Throw an exception on the fiber
        env->pushdata( fiber );
        env->eip = (unsigned int)_FiberExceptTerminate;

        // We want to eventually return back
        fiber->resume();
    }
	else
	{
		// Threads clean their environments after themselves
        ExecutiveFiber::closefiber( env );

		fiber->runtime = NULL;
	}
}

void CExecutiveManager::CloseFiber( CFiber *fiber )
{
    TerminateFiber( fiber );

    LIST_REMOVE( fiber->node );
    LIST_REMOVE( fiber->groupNode );

    delete fiber;
}

CExecutiveGroup* CExecutiveManager::CreateGroup( void )
{
    CExecutiveGroup *group = new CExecutiveGroup( this );

    return group;
}

void CExecutiveManager::DoPulse( void )
{
    {
        HighPrecisionMathWrap mathWrap;

        // Update frame timer.
        double timeNow = ExecutiveManager::GetPerformanceTimer();

        frameDuration = timeNow - frameTime;
        frameTime = timeNow;
    }

    LIST_FOREACH_BEGIN( CExecutiveGroup, groups.root, managerNode )
        item->DoPulse();
    LIST_FOREACH_END
}

END_NATIVE_EXECUTIVE