/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.thread.cpp
*  PURPOSE:     Thread abstraction layer for MTA
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

BEGIN_NATIVE_EXECUTIVE

#ifdef _WIN32

struct nativeThreadPlugin
{
    struct nativeThreadPluginInterface *manager;
    CExecThread *self;
    HANDLE hThread;
    mutable CRITICAL_SECTION threadLock;
    volatile eThreadStatus status;
    Fiber *terminationReturn;   // if not NULL, the thread yields to this state when it successfully terminated.
    volatile bool hasThreadBeenInitialized;

    RwListEntry <nativeThreadPlugin> node;
};

// Safe critical sections.
namespace LockSafety
{
    static AINLINE void EnterLockSafely( CRITICAL_SECTION& theSection )
    {
        EnterCriticalSection( &theSection );
    }

    static AINLINE void LeaveLockSafely( CRITICAL_SECTION& theSection )
    {
        LeaveCriticalSection( &theSection );
    }
}

// Struct for exception safety.
// Should be used on the C++ stack.
struct nativeLock
{
    CRITICAL_SECTION& critical_section;

    AINLINE nativeLock( CRITICAL_SECTION& theSection ) : critical_section( theSection )
    {
        LockSafety::EnterLockSafely( critical_section );
    }

    AINLINE ~nativeLock( void )
    {
        LockSafety::LeaveLockSafely( critical_section );
    }
};

struct nativeThreadPluginInterface : public ExecutiveManager::threadPluginContainer_t::pluginInterface
{
    RwList <nativeThreadPlugin> runningThreads;
    mutable CRITICAL_SECTION runningThreadListLock;

    volatile DWORD tlsCurrentThreadStruct;

    bool isTerminating;

    inline nativeThreadPluginInterface( void )
    {
        LIST_CLEAR( runningThreads.root );

        tlsCurrentThreadStruct = TlsAlloc();

        isTerminating = false;

        InitializeCriticalSection( &runningThreadListLock );
    }

    inline ~nativeThreadPluginInterface( void )
    {
        DeleteCriticalSection( &runningThreadListLock );

        if ( tlsCurrentThreadStruct != TLS_OUT_OF_INDEXES )
        {
            TlsFree( tlsCurrentThreadStruct );
        }
    }

    inline void TlsSetCurrentThreadInfo( nativeThreadPlugin *info )
    {
        DWORD currentThreadSlot = tlsCurrentThreadStruct;

        if ( currentThreadSlot != TLS_OUT_OF_INDEXES )
        {
            TlsSetValue( currentThreadSlot, info );
        }
    }

    inline nativeThreadPlugin* TlsGetCurrentThreadInfo( void )
    {
        nativeThreadPlugin *plugin = NULL;

        {
            DWORD currentThreadSlot = tlsCurrentThreadStruct;

            if ( currentThreadSlot != TLS_OUT_OF_INDEXES )
            {
                plugin = (nativeThreadPlugin*)TlsGetValue( currentThreadSlot );
            }
        }

        return plugin;
    }

    // This is a C++ proto. We must leave into a ASM proto to finish operation.
    static DWORD WINAPI _ThreadProcCPP( LPVOID param )
    {
        // Get the thread plugin information.
        nativeThreadPlugin *info = (nativeThreadPlugin*)param;

        CExecThread *threadInfo = info->self;

        // Put our executing thread information into our TLS value.
        info->manager->TlsSetCurrentThreadInfo( info );

        // Make sure we intercept termination requests!
        try
        {
            {
                nativeLock lock( info->threadLock );

                // We are properly initialized now.
                info->hasThreadBeenInitialized = true;
            }

            // Enter the routine.
            threadInfo->entryPoint( threadInfo, threadInfo->userdata );

            // Terminate our thread.
            threadInfo->Terminate();
        }
        catch( ... )
        {
            // We are terminated.
            info->status = THREAD_TERMINATED;
        }

        return ERROR_SUCCESS;
    }

    static void __stdcall _ThreadTerminationProto( void )
    {
        CExecThread *thisThread;

        __asm
        {
            mov thisThread,ebx
        }

        throw threadTerminationException( thisThread );
    }

    typedef struct _NT_TIB
    {
        PVOID ExceptionList;
        PVOID StackBase;
        PVOID StackLimit;
    } NT_TIB;

    void RtlTerminateThread( nativeThreadPlugin *threadInfo )
    {
        CExecThread *theThread = threadInfo->self;

        assert( theThread->isRemoteThread == false );

        // If we are not the current thread, we must do certain precautions.
        bool isCurrentThread = theThread->IsCurrent();

        if ( !isCurrentThread )
        {
            // Suspend our thread.
            // We must not use locking functions for that.
            DWORD suspendCount = SuspendThread( threadInfo->hThread );
        }

        // Set our status to terminating.
        threadInfo->status = THREAD_TERMINATING;

        // Depends on whether we are the current thread or not.
        if ( isCurrentThread )
        {
            // Just do the termination.
            throw threadTerminationException( theThread );
        }
        else
        {
            // Only do termination if the thread has been initialized.
            if ( threadInfo->hasThreadBeenInitialized )
            {
                // NOTE: this is a fucking insecure hack that changes the executing thread of a thread runtime.
                // It is written with the hope that no code makes assumptions about a certain thread having to execute
                // termination logic. If this feature will be required, I will still have certain work-arounds!

                // It works, so do not fuck around with it.

                // Create a fiber that we have to walk down the thread context with.
                Fiber terminationFiber;
                Fiber returnFiber;

                // Get the complete WIN32 x86 context.
                CONTEXT currentContext;
                currentContext.ContextFlags = ( CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_SEGMENTS );

                BOOL threadContextGet = GetThreadContext( threadInfo->hThread, &currentContext );

                if ( threadContextGet == TRUE )
                {
                    // Put our context into the fiber.
                    terminationFiber.ebp = currentContext.Ebp;
                    terminationFiber.edi = currentContext.Edi;
                    terminationFiber.esi = currentContext.Esi;
                    terminationFiber.esp = currentContext.Esp;

                    // Grab exception information of that thread, too!
                    // This especially is quite an ugly hack.
                    {
                        LDT_ENTRY fsSegmentEntry;

                        BOOL selectorGet = GetThreadSelectorEntry( threadInfo->hThread, currentContext.SegFs, &fsSegmentEntry );
                        
                        assert( selectorGet == TRUE );

                        // Construct the real pointer to NT_TIB.
                        DWORD fsPointer =
                            ( fsSegmentEntry.BaseLow ) |
                            ( fsSegmentEntry.HighWord.Bytes.BaseMid << 16 ) |
                            ( fsSegmentEntry.HighWord.Bytes.BaseHi << 24 );

                        const NT_TIB *threadInfoBlock = (NT_TIB*)fsPointer;

                        terminationFiber.stack_base = (unsigned int*)threadInfoBlock->StackBase;
                        terminationFiber.stack_limit = (unsigned int*)threadInfoBlock->StackLimit;
                        terminationFiber.except_info = (void*)threadInfoBlock->ExceptionList;
                    }

                    // Modify it so we point to the termination routine.
                    terminationFiber.ebx = (unsigned int)theThread;
                    terminationFiber.eip = (unsigned int)_ThreadTerminationProto;
                        
                    // Make the thread now that it should jump back here at termination.
                    threadInfo->terminationReturn = &returnFiber;

                    // Jump!!!
                    ExecutiveFiber::eswitch( &returnFiber, &terminationFiber );

                    // If we return here, the thread must be terminated.
                    assert( threadInfo->status == THREAD_TERMINATED );
                }
            }

            // We can now terminate the handle.
            TerminateThread( threadInfo->hThread, ERROR_SUCCESS );

            // The thread is now terminated.
            threadInfo->status = THREAD_TERMINATED;
        }

        // If we were the current thread, we cannot reach this point.
        assert( isCurrentThread == false );
    }

    bool OnPluginConstruct( CExecThread *thread, ExecutiveManager::threadPluginContainer_t::pluginOffset_t pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor id );
    void OnPluginDestroy( CExecThread *thread, ExecutiveManager::threadPluginContainer_t::pluginOffset_t pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor id );
};

// This is a native ASM routine. In this there can be no language objects, so its perfect for
// ASM hacks.
__declspec(naked) void _ThreadProcNative( void )
{
    __asm
    {
        // Call the C++ thread runtime with our argument.
        mov ebx,[esp+4] // nativeThreadPlugin type
        push ebx
        call nativeThreadPluginInterface::_ThreadProcCPP

        // Check for a termination fiber.
        mov edx,[ebx]nativeThreadPlugin.terminationReturn

        test edx,edx
        jz NoTerminationReturn

        // Since we have a termination return fiber, leave to it.
        mov eax,edx
        jmp ExecutiveFiber::leave

NoTerminationReturn:
        // We return to the WinNT thread dispatcher.
        ret
    }
}

bool nativeThreadPluginInterface::OnPluginConstruct( CExecThread *thread, ExecutiveManager::threadPluginContainer_t::pluginOffset_t pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor id )
{
    nativeThreadPlugin *info = ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( thread, pluginOffset );

    // If we are not a remote thread...
    HANDLE hOurThread = NULL;

    if ( !thread->isRemoteThread )
    {
        // ... create a local thread!
        DWORD threadIdOut;

        HANDLE hThread = ::CreateThread( NULL, (SIZE_T)thread->stackSize, (LPTHREAD_START_ROUTINE)_ThreadProcNative, info, CREATE_SUSPENDED, &threadIdOut );

        if ( hThread == NULL )
            return false;

        hOurThread = hThread;
    }
    info->hThread = hOurThread;

    // NOTE: we initialize remote threads in the GetCurrentThread routine!

    // Give ourselves a self reference pointer.
    info->self = thread;
    info->manager = this;

    // This field is used by the runtime dispatcher to execute a "controlled return"
    // from different threads.
    info->terminationReturn = NULL;

    info->hasThreadBeenInitialized = false;

    // We assume the thread is (always) running if its a remote thread.
    // Otherwise we know that it starts suspended.
    info->status = ( !thread->isRemoteThread ) ? THREAD_SUSPENDED : THREAD_RUNNING;

    {
        nativeLock lock( this->runningThreadListLock );

        LIST_INSERT( runningThreads.root, info->node );
    }

    // Set up synchronization object.
    InitializeCriticalSection( &info->threadLock );
    return true;
}

void nativeThreadPluginInterface::OnPluginDestroy( CExecThread *thread, ExecutiveManager::threadPluginContainer_t::pluginOffset_t pluginOffset, ExecutiveManager::threadPluginContainer_t::pluginDescriptor id )
{
    nativeThreadPlugin *info = ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( thread, pluginOffset );

    // Only terminate if we have not acquired a remote thread.
    if ( !thread->isRemoteThread )
    {
        // Terminate the thread.
        thread->Terminate();
    }

    // Delete synchronization object.
    DeleteCriticalSection( &info->threadLock );

    {
        nativeLock lock( this->runningThreadListLock );

        LIST_REMOVE( runningThreads.root );
    }

    CloseHandle( info->hThread );
}

#endif

// todo: add other OSes too when it becomes necessary.

struct privateNativeThreadEnvironment
{
    nativeThreadPluginInterface _nativePluginInterface;

    ExecutiveManager::threadPluginContainer_t::pluginOffset_t nativePluginOffset;

    inline void Initialize( CExecutiveManager *manager )
    {
        this->nativePluginOffset =
            manager->threadPlugins.RegisterPlugin( sizeof( nativeThreadPlugin ), THREAD_PLUGIN_NATIVE, &_nativePluginInterface );
    }

    inline void Shutdown( CExecutiveManager *manager )
    {
        // Notify ourselves that we are terminating.
        _nativePluginInterface.isTerminating = true;

        // Shutdown all currently yet active threads.
        while ( !LIST_EMPTY( manager->threads.root ) )
        {
            CExecThread *thread = LIST_GETITEM( CExecThread, manager->threads.root.next, managerNode );

            manager->CloseThread( thread );
        }

        if ( ExecutiveManager::threadPluginContainer_t::IsOffsetValid( this->nativePluginOffset ) )
        {
            manager->threadPlugins.UnregisterPlugin( this->nativePluginOffset );
        }
    }
};

static PluginDependantStructRegister <privateNativeThreadEnvironment, executiveManagerFactory_t> privateNativeThreadEnvironmentRegister;

inline nativeThreadPlugin* GetNativeThreadPlugin( CExecutiveManager *manager, CExecThread *theThread )
{
    privateNativeThreadEnvironment *nativeThreadEnv = privateNativeThreadEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

    if ( nativeThreadEnv )
    {
        return ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( theThread, nativeThreadEnv->nativePluginOffset );
    }

    return NULL;
}

inline const nativeThreadPlugin* GetConstNativeThreadPlugin( const CExecutiveManager *manager, const CExecThread *theThread )
{
    const privateNativeThreadEnvironment *nativeThreadEnv = privateNativeThreadEnvironmentRegister.GetConstPluginStruct( (const CExecutiveManagerNative*)manager );

    if ( nativeThreadEnv )
    {
        return ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <nativeThreadPlugin> ( theThread, nativeThreadEnv->nativePluginOffset );
    }

    return NULL;
}

CExecThread::CExecThread( CExecutiveManager *manager, bool isRemoteThread, void *userdata, size_t stackSize, threadEntryPoint_t entryPoint )
{
    this->manager = manager;
    this->isRemoteThread = isRemoteThread;
    this->userdata = userdata;
    this->stackSize = stackSize;
    this->entryPoint = entryPoint;

    LIST_INSERT( manager->threads.root, managerNode );
}

CExecThread::~CExecThread( void )
{
    LIST_REMOVE( managerNode );
}

eThreadStatus CExecThread::GetStatus( void ) const
{
    eThreadStatus status = THREAD_TERMINATED;

#ifdef _WIN32
    const nativeThreadPlugin *info = GetConstNativeThreadPlugin( this->manager, this );

    if ( info )
    {
        nativeLock lock( info->threadLock );

        status = info->status;
    }
#endif

    return status;
}

// WARNING: terminating threads in general is very naughty and causes shit to go haywire!
// No matter what thread state, this function guarrantees to terminate a thread cleanly according to
// C++ stack unwinding logic!
// Termination of a thread is allowed to be executed by another thread (e.g. the "main" thread).
bool CExecThread::Terminate( void )
{
    bool returnVal = false;

#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( this->manager, this );

    if ( info && info->status != THREAD_TERMINATED )
    {
        // We cannot terminate a terminating thread.
        if ( info->status != THREAD_TERMINATING )
        {
            nativeLock lock( info->threadLock );

            if ( info->status != THREAD_TERMINATED )
            {
                // Termination depends on what kind of thread we face.
                if ( this->isRemoteThread )
                {
                    // Remote threads must be killed just like that.
                    BOOL success = TerminateThread( info->hThread, ERROR_SUCCESS );

                    if ( success == TRUE )
                    {
                        // Put the status as terminated.
                        info->status = THREAD_TERMINATED;

                        // Return true.
                        returnVal = true;
                    }
                }
                else
                {
                    privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)this );

                    if ( nativeEnv )
                    {
                        // User-mode threads have to be cleanly terminated.
                        // This means going down the exception stack.
                        nativeEnv->_nativePluginInterface.RtlTerminateThread( info );

                        // We may not actually get here!
                    }

                    // We have successfully terminated the thread.
                    returnVal = true;
                }
            }
        }
    }
#endif

    return returnVal;
}

bool CExecThread::Suspend( void )
{
    bool returnVal = false;

#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( this->manager, this );

    // We cannot suspend a remote thread.
    if ( !isRemoteThread )
    {
        if ( info && info->status == THREAD_RUNNING )
        {
            nativeLock lock( info->threadLock );

            if ( info->status == THREAD_RUNNING )
            {
                BOOL success = SuspendThread( info->hThread );

                if ( success == TRUE )
                {
                    info->status = THREAD_SUSPENDED;

                    returnVal = true;
                }
            }
        }
    }
#endif

    return returnVal;
}

bool CExecThread::Resume( void )
{
    bool returnVal = false;

#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( this->manager, this );

    // We cannot resume a remote thread.
    if ( !isRemoteThread )
    {
        if ( info && info->status == THREAD_SUSPENDED )
        {
            nativeLock lock( info->threadLock );

            if ( info->status == THREAD_SUSPENDED )
            {
                BOOL success = ResumeThread( info->hThread );

                if ( success == TRUE )
                {
                    info->status = THREAD_RUNNING;

                    returnVal = true;
                }
            }
        }
    }
#endif

    return returnVal;
}

bool CExecThread::IsCurrent( void )
{
    return ( this->manager->GetCurrentThread() == this );
}

void CExecThread::Lock( void )
{
#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( this->manager, this );

    if ( info )
    {
        LockSafety::EnterLockSafely( info->threadLock );
    }
#else
    assert( 0 );
#endif
}

void CExecThread::Unlock( void )
{
#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( this->manager, this );

    if ( info )
    {
        LockSafety::LeaveLockSafely( info->threadLock );
    }
#else
    assert( 0 );
#endif
}

struct threadObjectConstructor
{
    inline threadObjectConstructor( CExecutiveManager *manager, bool isRemoteThread, void *userdata, size_t stackSize, CExecThread::threadEntryPoint_t entryPoint )
    {
        this->manager = manager;
        this->isRemoteThread = isRemoteThread;
        this->userdata = userdata;
        this->stackSize = stackSize;
        this->entryPoint = entryPoint;
    }

    inline CExecThread* Construct( void *mem ) const
    {
        return new (mem) CExecThread( this->manager, this->isRemoteThread, this->userdata, this->stackSize, this->entryPoint );
    }

    CExecutiveManager *manager;
    bool isRemoteThread;
    void *userdata;
    size_t stackSize;
    CExecThread::threadEntryPoint_t entryPoint;
};

CExecThread* CExecutiveManager::CreateThread( CExecThread::threadEntryPoint_t entryPoint, void *userdata, size_t stackSize )
{
    // No point in creating threads if we have no native implementation.
    if ( privateNativeThreadEnvironmentRegister.IsRegistered() == false )
        return NULL;

    CExecThread *threadInfo = NULL;

    // Construct the thread.
    {
        // Make sure we synchronize access to plugin containers!
        // This only has to happen when the API has to be thread-safe.
        nativeLock lock( threadPluginsLock );
        
        try
        {
            threadObjectConstructor threadConstruct( this, false, userdata, stackSize, entryPoint );

            threadInfo = this->threadPlugins.ConstructTemplate( ExecutiveManager::moduleAllocator, threadConstruct );
        }
        catch( ... )
        {
            // TODO: add an exception that can be thrown if the construction of threads failed.
            threadInfo = NULL;
        }
    }

    if ( !threadInfo )
    {
        // Could always happen.
        return NULL;
    }

    // Return the thread.
    return threadInfo;
}

void CExecutiveManager::TerminateThread( CExecThread *thread )
{
    thread->Terminate();
}

void CExecutiveManager::JoinThread( CExecThread *thread )
{
#ifdef _WIN32
    nativeThreadPlugin *info = GetNativeThreadPlugin( thread->manager, thread );

    if ( info )
    {
        // Wait for completion of the thread.
        while ( info->status != THREAD_TERMINATED )
        {
            WaitForSingleObject( info->hThread, INFINITE );
        }
    }
#endif
}

CExecThread* CExecutiveManager::GetCurrentThread( void )
{
    CExecThread *currentThread = NULL;

#ifdef _WIN32
    {
        // Get our native interface (if available).
        privateNativeThreadEnvironment *nativeEnv = privateNativeThreadEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)this );
        
        if ( nativeEnv )
        {
            HANDLE hRunningThread = ::GetCurrentThread();

            // If we have an accelerated TLS slot, try to get the handle from it.
            if ( nativeThreadPlugin *tlsInfo = nativeEnv->_nativePluginInterface.TlsGetCurrentThreadInfo() )
            {
                currentThread = tlsInfo->self;
            }
            else
            {
                nativeLock lock( nativeEnv->_nativePluginInterface.runningThreadListLock );

                // Else we have to go the slow way by checking every running thread information in existance.
                LIST_FOREACH_BEGIN( nativeThreadPlugin, nativeEnv->_nativePluginInterface.runningThreads.root, node )
                    if ( item->hThread == hRunningThread )
                    {
                        currentThread = item->self;
                        break;
                    }
                LIST_FOREACH_END
            }

            // If we have not found a thread handle representing this native thread, we should create one.
            if ( currentThread == NULL && nativeEnv->_nativePluginInterface.isTerminating == false )
            {
                // Create the thread.
                CExecThread *newThreadInfo = NULL;
                {
                    nativeLock lock( threadPluginsLock );

                    try
                    {
                        threadObjectConstructor threadConstruct( this, true, NULL, 0, NULL );

                        newThreadInfo = this->threadPlugins.ConstructTemplate( ExecutiveManager::moduleAllocator, threadConstruct );
                    }
                    catch( ... )
                    {
                        newThreadInfo = NULL;
                    }
                }

                if ( newThreadInfo )
                {
                    bool successPluginCreation = false;

                    // Our plugin must have been successfully intialized to continue.
                    if ( nativeThreadPlugin *plugInfo = GetNativeThreadPlugin( this, newThreadInfo ) )
                    {
                        // Open another thread handle and put it into our native plugin.
                        HANDLE newHandle = NULL;

                        BOOL successClone = DuplicateHandle(
                            GetCurrentProcess(), hRunningThread,
                            GetCurrentProcess(), &newHandle,
                            0, FALSE, DUPLICATE_SAME_ACCESS
                        );

                        if ( successClone == TRUE )
                        {
                            // Put the new handle into our plugin structure.
                            plugInfo->hThread = newHandle;

                            // Set our plugin information into our Tls slot (if available).
                            nativeEnv->_nativePluginInterface.TlsSetCurrentThreadInfo( plugInfo );

                            // Return it.
                            currentThread = newThreadInfo;

                            successPluginCreation = true;
                        }
                    }
                    
                    if ( successPluginCreation == false )
                    {
                        // Delete the thread object again.
                        {
                            nativeLock lock( threadPluginsLock );

                            this->threadPlugins.Destroy( ExecutiveManager::moduleAllocator, newThreadInfo );
                        }
                    }
                }
            }
        }
    }
#endif

    return currentThread;
}

void CExecutiveManager::CloseThread( CExecThread *thread )
{
    // Only allow this from the current thread if we are a remote thread.
    if ( GetCurrentThread() == thread )
    {
        if ( !thread->isRemoteThread )
            return;
    }

    // Kill the thread.
    {
#ifdef _WIN32
        nativeLock lock( threadPluginsLock );
#endif

        this->threadPlugins.Destroy( ExecutiveManager::moduleAllocator, thread );
    }
}

void CExecutiveManager::InitThreads( void )
{
    LIST_CLEAR( threads.root );

#ifdef _WIN32
    InitializeCriticalSection( &threadPluginsLock );
#endif
}

void CExecutiveManager::ShutdownThreads( void )
{
#ifdef _WIN32
    DeleteCriticalSection( &threadPluginsLock );
#endif
}

void registerThreadPlugin( void )
{
    privateNativeThreadEnvironmentRegister.RegisterPlugin( executiveManagerFactory );
}

END_NATIVE_EXECUTIVE