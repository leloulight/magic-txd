/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.task.cpp
*  PURPOSE:     Runtime for quick parallel execution sheduling
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

BEGIN_NATIVE_EXECUTIVE

static CExecThread *shedulerThread = NULL;

struct hyperSignal : public hazardPreventionInterface
{
    void TerminateHazard( void )
    {
        // Set the event to signalled state, and force it.
        SetEvent( this->pingEvent );
    }

    bool wantsToTerminate;

    AINLINE hyperSignal( void )
    {
        // Create the ping event that makes the sheduler thread wait until there is necessary activity.
        pingEvent = CreateEventW( NULL, false, false, NULL );
        isWaiting = false;
        wasSignaled = false;
        wantsToTerminate = false;

        InitializeCriticalSection( &pingLock );
    }

    AINLINE ~hyperSignal( void )
    {
        DeleteCriticalSection( &pingLock );

        CloseHandle( pingEvent );
    }

    AINLINE void Ping( void )
    {
        if ( isWaiting || !wasSignaled )
        {
            EnterCriticalSection( &pingLock );

            SetEvent( pingEvent );

            wasSignaled = true;

            LeaveCriticalSection( &pingLock );
        }
    }

    AINLINE void WaitForSignal( CExecutiveManager *manager )
    {
        hazardousSituation situation( manager, this );

        isWaiting = true;

        WaitForSingleObject( pingEvent, INFINITE );

        // We need to check for hazard conditions.
        manager->CheckHazardCondition();

        isWaiting = false;

        EnterCriticalSection( &pingLock );

        wasSignaled = false;

        LeaveCriticalSection( &pingLock );
    }

    HANDLE pingEvent;
    bool isWaiting;
    bool wasSignaled;

    CRITICAL_SECTION pingLock;
};

static hyperSignal shedulerPingEvent;

struct shedulerThreadItem
{
    CExecTask *task;
};

// Maintenance variables for hyper-sheduling.
static SynchronizedHyperQueue <shedulerThreadItem> sheduledItems( 256 );

// Potentially unsafe routine, but should be hyper-fast.
static AINLINE bool ProcessSheduleItem( void )
{
    shedulerThreadItem currentSheduling;

    bool hasSheduledItem = sheduledItems.GetSheduledItem( currentSheduling );

    if ( hasSheduledItem )
    {
        // Run the task till it yields.
        CExecTask *theTask = currentSheduling.task;
        
        theTask->runtimeFiber->resume();

        // We finished one iteration of this task.
        InterlockedDecrement( &theTask->usageCount );
    }

    return hasSheduledItem;
}

static void __stdcall TaskShedulerThread( CExecThread *threadInfo, void *param )
{
    while ( true )
    {
        shedulerPingEvent.WaitForSignal( threadInfo->manager );

        while ( ProcessSheduleItem() );
    }
}

void CExecutiveManager::InitializeTasks( void )
{
#if 0
    // Initialize synchronization objects.
    shedulerThread = CreateThread( TaskShedulerThread, NULL );

    if ( shedulerThread )
    {
        // Start the thread.
        shedulerThread->Resume();
    }
#else
    // TODO: make this object based, so the user can create a task sheduler on demand.
    // Then we can re-enable this logic.
    shedulerThread = NULL;
#endif
}

void CExecutiveManager::ShutdownTasks( void )
{
#if 0
    if ( shedulerThread )
    {
        // Shutdown synchronization objects.
        TerminateThread( shedulerThread );

        CloseThread( shedulerThread );

        shedulerThread = NULL;
    }
#endif  
}

struct execWrapStruct
{
    CExecTask *theTask;
    void *userdata;
};

static void __stdcall TaskFiberWrap( CFiber *theFiber, void *memPtr )
{
    // Initialize the runtime, as the pointer is only temporary.
    CExecTask *theTask = NULL;
    void *userdata = NULL;

    {
        execWrapStruct& theInfo = *(execWrapStruct*)memPtr;

        theTask = theInfo.theTask;
        userdata = theInfo.userdata;
    }

    // Wait for the next sheduling. We are set up by now.
    theFiber->yield();

    theTask->callback( theTask, userdata );
}

CExecTask* CExecutiveManager::CreateTask( CExecTask::taskexec_t callback, void *userdata, size_t stackSize )
{
    // Create the underlying fiber.
    execWrapStruct info;
    info.theTask = NULL;
    info.userdata = userdata;

    CFiber *theFiber = CreateFiber( TaskFiberWrap, &info, stackSize );

    if ( !theFiber )
        return NULL;

    CExecTask *task = new CExecTask( this, theFiber );

    // Set up runtime task members.
    task->callback = callback;
    
    // Finalize the information to pass to the fiber.
    info.theTask = task;

    // Make a first pulse into the task so it can set itself up.
    theFiber->resume();

    // The task is ready to be used.
    return task;
}

void CExecutiveManager::CloseTask( CExecTask *task )
{
    // We must wait for the task to finish.
    task->WaitForFinish();

    CloseFiber( task->runtimeFiber );

    delete task;
}

void CExecTask::Execute( void )
{
    // Make sure the task is initialized.
    if ( !isInitialized )
    {
        // Ping the task for initialization on the main thread.
        runtimeFiber->resume();

        isInitialized = true;
    }

    // Make sure the thread is not marked as finished.
    InterlockedIncrement( &this->usageCount );

    // Use a circling queue for this as found in the R* streaming runtime.
    shedulerThreadItem theItem;
    theItem.task = this;

    sheduledItems.AddItem( theItem );

    // Notify the sheduler that there are items to process.
    shedulerPingEvent.Ping();
}

void CExecTask::WaitForFinish( void )
{
    CFiber *theFiber = runtimeFiber;

    if ( !runtimeFiber->is_terminated() )
    {
        // Spin lock until the task has finished.
        while ( !IsFinished() );
#if 0
        {
            ProcessSheduleItem();
        }
#endif
    }
}

END_NATIVE_EXECUTIVE