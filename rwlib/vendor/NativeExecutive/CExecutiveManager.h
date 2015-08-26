/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.h
*  PURPOSE:     MTA thread and fiber execution manager for workload smoothing
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _EXECUTIVE_MANAGER_
#define _EXECUTIVE_MANAGER_

#include <MemoryUtils.h>
#include <rwlist.hpp>

// Namespace simplification definitions.
#define BEGIN_NATIVE_EXECUTIVE      namespace NativeExecutive {
#define END_NATIVE_EXECUTIVE        }

BEGIN_NATIVE_EXECUTIVE

// Forward declarations.
class CExecThread;
class CFiber;
class CExecTask;

namespace ExecutiveManager
{
    // Function used by the system for performance measurements.
    AINLINE double GetPerformanceTimer( void )
    {
        LONGLONG counterFrequency, currentCount;

        QueryPerformanceFrequency( (LARGE_INTEGER*)&counterFrequency );
        QueryPerformanceCounter( (LARGE_INTEGER*)&currentCount );

        return (long double)currentCount / (long double)counterFrequency;
    }

    typedef StaticPluginClassFactory <CExecThread> threadPluginContainer_t;
};

END_NATIVE_EXECUTIVE

#include "CExecutiveManager.thread.h"
#include "CExecutiveManager.fiber.h"
#include "CExecutiveManager.task.h"
#include "CExecutiveManager.rwlock.h"

BEGIN_NATIVE_EXECUTIVE

#define DEFAULT_GROUP_MAX_EXEC_TIME     16

struct fiberTerminationException
{
    fiberTerminationException( CFiber *fiber )
    {
        this->fiber = fiber;
    }

    CFiber *fiber;
};

class CExecutiveGroup;

class CExecutiveManager abstract
{
    friend class CExecutiveGroup;

protected:
    // We do not want the runtime to create us.
    // Instead we expose a factory that has to create us.
    CExecutiveManager                   ( void );
    ~CExecutiveManager                  ( void );

public:
    // Public factory API.
    static CExecutiveManager* Create( void );
    static void Delete( CExecutiveManager *manager );

private:
    void            PurgeActiveObjects  ( void );

public:
    CExecThread*    CreateThread        ( CExecThread::threadEntryPoint_t proc, void *userdata, size_t stackSize = 0 );
    void            TerminateThread     ( CExecThread *thread );    // DANGEROUS function!
    void            JoinThread          ( CExecThread *thread );    // safe function :)
    CExecThread*    GetCurrentThread    ( void );
    void            CloseThread         ( CExecThread *thread );

    void            CheckHazardCondition( void );

    void            InitThreads         ( void );
    void            ShutdownThreads     ( void );

    CFiber*         CreateFiber         ( CFiber::fiberexec_t proc, void *userdata, size_t stackSize = 0 );
    void            TerminateFiber      ( CFiber *fiber );
    void            CloseFiber          ( CFiber *fiber );

    void            PushFiber           ( CFiber *fiber );
    void            PopFiber            ( void );
    CFiber*         GetCurrentFiber     ( void );

    CExecutiveGroup*    CreateGroup     ( void );

    void            DoPulse             ( void );

    void            InitializeTasks     ( void );
    void            ShutdownTasks       ( void );

    CExecTask*      CreateTask          ( CExecTask::taskexec_t proc, void *userdata, size_t stackSize = 0 );
    void            CloseTask           ( CExecTask *task );

    // Methods for managing synchronization objects.
    CReadWriteLock* CreateReadWriteLock ( void );
    void            CloseReadWriteLock  ( CReadWriteLock *theLock );

    size_t          GetReadWriteLockStructSize  ( void );
    CReadWriteLock* CreatePlacedReadWriteLock   ( void *mem );
    void            ClosePlacedReadWriteLock    ( CReadWriteLock *theLock );

    CReentrantReadWriteLock*    CreateReentrantReadWriteLock( void );
    void                        CloseReentrantReadWriteLock ( CReentrantReadWriteLock *theLock );

    size_t                      GetReentrantReadWriteLockStructSize ( void );
    CReentrantReadWriteLock*    CreatePlacedReentrantReadWriteLock  ( void *mem );
    void                        ClosePlacedReentrantReadWriteLock   ( CReentrantReadWriteLock *theLock );

    // DO NOT ACCESS the following fields from your runtime.
    // These MUST ONLY be accessed from the NativeExecutive library!

    ExecutiveManager::threadPluginContainer_t threadPlugins;

    CRITICAL_SECTION threadPluginsLock;

    bool isTerminating;     // if true then no new objects are allowed to spawn anymore.

    RwList <CExecThread> threads;
    RwList <CFiber> fibers;
    RwList <CExecTask> tasks;
    RwList <CExecutiveGroup> groups;

    CExecutiveGroup *defGroup;      // default group that all fibers are put into at the beginning.

    double frameTime;
    double frameDuration;

    double GetFrameDuration( void )
    {
        return frameDuration;
    }
};

// Exception that gets thrown by threads when they terminate.
struct threadTerminationException : public std::exception
{
    inline threadTerminationException( CExecThread *theThread ) : std::exception( "thread termination" )
    {
        this->terminatedThread = theThread;
    }

    inline ~threadTerminationException( void )
    {
        return;
    }

    CExecThread *terminatedThread;
};

class CExecutiveGroup
{
    friend class CExecutiveManager;

    inline void AddFiberNative( CFiber *fiber )
    {
        LIST_APPEND( fibers.root, fiber->groupNode );

        fiber->group = this;
    }

public:
    CExecutiveGroup( CExecutiveManager *manager )
    {
        LIST_CLEAR( fibers.root );

        this->manager = manager;
        this->maximumExecutionTime = DEFAULT_GROUP_MAX_EXEC_TIME;
        this->totalFrameExecutionTime = 0;

        this->perfMultiplier = 1.0f;

        LIST_APPEND( manager->groups.root, managerNode );
    }

    ~CExecutiveGroup( void )
    {
        while ( !LIST_EMPTY( fibers.root ) )
        {
            CFiber *fiber = LIST_GETITEM( CFiber, fibers.root.next, groupNode );

            manager->CloseFiber( fiber );
        }

        LIST_REMOVE( managerNode );
    }

    inline void AddFiber( CFiber *fiber )
    {
        LIST_REMOVE( fiber->groupNode );
        
        AddFiberNative( fiber );
    }

    inline void SetMaximumExecutionTime( double ms )        { maximumExecutionTime = ms; }
    inline double GetMaximumExecutionTime( void ) const     { return maximumExecutionTime; }

    void DoPulse( void )
    {
        totalFrameExecutionTime = 0;
    }

    CExecutiveManager *manager;

    RwList <CFiber> fibers;                         // all fibers registered to this group.

    RwListEntry <CExecutiveGroup> managerNode;      // node in list of all groups.

    // Per frame settings
    // These times are given in milliseconds.
    double totalFrameExecutionTime;
    double maximumExecutionTime;

    double perfMultiplier;

    void SetPerfMultiplier( double mult )
    {
        perfMultiplier = mult;
    }
};

END_NATIVE_EXECUTIVE

#include "CExecutiveManager.hazards.h"

#endif //_EXECUTIVE_MANAGER_