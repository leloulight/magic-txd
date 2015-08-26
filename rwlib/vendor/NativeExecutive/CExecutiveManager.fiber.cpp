/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.fiber.cpp
*  PURPOSE:     Executive manager fiber logic
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CExecutiveManager.fiber.hxx"

#ifdef _WIN32
#include <excpt.h>
#include <windows.h>

#include "CExecutiveManager.native.hxx"
#include "CExecutiveManager.hazards.hxx"

typedef struct _EXCEPTION_REGISTRATION
{
     struct _EXCEPTION_REGISTRATION* Next;
     void *Handler;
} EXCEPTION_REGISTRATION, *PEXCEPTION_REGISTRATION;

static EXCEPTION_DISPOSITION _defaultHandler( EXCEPTION_REGISTRATION *record, void *frame, CONTEXT *context, void *dispatcher )
{
    exit( 0x0DAB00B5 );
    return ExceptionContinueSearch;
}

static EXCEPTION_REGISTRATION _baseException =
{
    (EXCEPTION_REGISTRATION*)-1,
    _defaultHandler
};
#endif

BEGIN_NATIVE_EXECUTIVE

#pragma warning(disable:4733)

// Global memory allocation functions.
static ExecutiveFiber::memalloc_t fiberMemAlloc = NULL;
static ExecutiveFiber::memfree_t fiberMemFree = NULL;

Fiber* ExecutiveFiber::newfiber( FiberStatus *userdata, size_t stackSize, FiberProcedure proc, FiberStatus::termfunc_t termcb )
{
    Fiber *env = (Fiber*)fiberMemAlloc( sizeof(Fiber) );

    // Allocate stack memory
    if ( stackSize == 0 )
        stackSize = 2 << 17;    // about 128 kilobytes of stack space (is this enough?)

    void *stack = fiberMemAlloc( stackSize );
    env->stack_base = (char*)( (char*)stack + stackSize );
    env->stack_limit = stack;

    stack = env->stack_base;

    env->esp = (regType_t)stack;

#if defined(_M_IX86)
    // Once entering, the first argument should be the thread
    env->pushdata( userdata );
    env->pushdata( &exit );
    env->pushdata( userdata );
    env->pushdata( &_fiber86_retHandler );
    
    // We can enter the procedure directly.
    env->eip = (regType_t)proc;

#elif defined(_M_AMD64)
    // This is quite a complicated situation.
    env->pushdata( userdata );
    env->pushdata( &exit );     // if returning from start routine, exit.

    // We should enter a custom fiber routine.
    env->eip = (regType_t)_fiber64_procStart;
#endif

    env->except_info = &_baseException;

    userdata->termcb = termcb;

    env->stackSize = stackSize;
    return env;
}

Fiber* ExecutiveFiber::makefiber( void )
{
    Fiber *fiber = (Fiber*)fiberMemAlloc( sizeof(Fiber) );
    fiber->stackSize = 0;
    return fiber;
}

void ExecutiveFiber::closefiber( Fiber *fiber )
{
    if ( fiber->stackSize )
        fiberMemFree( fiber->stack_limit );

    fiberMemFree( fiber );
}

void ExecutiveFiber::setmemfuncs( ExecutiveFiber::memalloc_t malloc, ExecutiveFiber::memfree_t mfree )
{
    fiberMemAlloc = malloc;
    fiberMemFree = mfree;
}

void ExecutiveFiber::eswitch( Fiber *from, Fiber *to )
{
#if defined(_M_IX86)
    _fiber86_eswitch( from, to );
#elif defined(_M_AMD64)
    _fiber64_eswitch( from, to );
#else
#error missing fiber eswitch implementation for platform
#endif
}

// For use with yielding
void ExecutiveFiber::qswitch( Fiber *from, Fiber *to )
{
#if defined(_M_IX86)
    _fiber86_qswitch( from, to );
#elif defined(_M_AMD64)
    _fiber64_qswitch( from, to );
#else
#error missing fiber qswitch implementation for platform
#endif
}

privateFiberEnvironmentRegister_t privateFiberEnvironmentRegister;

inline threadFiberStackPluginInfo* GetThreadFiberStackPlugin( CExecutiveManager *manager, CExecThread *theThread )
{
    privateFiberEnvironment *fiberEnv = privateFiberEnvironmentRegister.GetPluginStruct( (CExecutiveManagerNative*)manager );

    if ( fiberEnv )
    {
        return ExecutiveManager::threadPluginContainer_t::RESOLVE_STRUCT <threadFiberStackPluginInfo> ( theThread, fiberEnv->threadFiberStackPluginOffset );
    }

    return NULL;
}

void CFiber::push_on_stack( void )
{
    manager->PushFiber( this );
}

void CFiber::pop_from_stack( void )
{
    manager->PopFiber();
}

bool CFiber::is_current_on_stack( void ) const
{
    return ( manager->GetCurrentFiber() == this );
}

void CFiber::yield_proc( void )
{
    bool needYield = false;
    {
        // We need very high precision math here.
        HighPrecisionMathWrap mathWrap;
        
        // Do some test with timing.
        double currentTimer = ExecutiveManager::GetPerformanceTimer();
        double perfMultiplier = group->perfMultiplier;

        needYield = ( currentTimer - resumeTimer ) > manager->GetFrameDuration() * perfMultiplier;
    }

    if ( needYield )
        yield();
}

void CExecutiveManager::PushFiber( CFiber *currentFiber )
{
    CExecThread *currentThread = GetCurrentThread();

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this, currentThread ) )
    {
        info->fiberStack.AddItem( currentFiber );
    }
}

void CExecutiveManager::PopFiber( void )
{
    CExecThread *currentThread = GetCurrentThread();

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this, currentThread ) )
    {
        info->fiberStack.Pop();
    }
}

threadFiberStackIterator::threadFiberStackIterator( CExecThread *thread )
{
    this->thread = thread;
    this->iter = 0;
}

threadFiberStackIterator::~threadFiberStackIterator( void )
{
    return;
}

bool threadFiberStackIterator::IsEnd( void ) const
{
    threadFiberStackPluginInfo *fiberObj = GetThreadFiberStackPlugin( this->thread->manager, this->thread );

    if ( fiberObj )
    {
        return ( fiberObj->fiberStack.GetCount() <= this->iter );
    }

    return true;
}

void threadFiberStackIterator::Increment( void )
{
    this->iter++;
}

CFiber* threadFiberStackIterator::Resolve( void ) const
{
    threadFiberStackPluginInfo *fiberObj = GetThreadFiberStackPlugin( this->thread->manager, this->thread );

    if ( fiberObj )
    {
        return fiberObj->fiberStack.Get( this->iter );
    }

    return NULL;
}

CFiber* CExecThread::GetCurrentFiber( void )
{
    CFiber *currentFiber = NULL;

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this->manager, this ) )
    {
        size_t fiberCount = info->fiberStack.GetCount();

        return ( fiberCount != 0 ) ? ( info->fiberStack.Get( fiberCount - 1 ) ) : ( NULL );
    }

    return currentFiber;
}

CFiber* CExecutiveManager::GetCurrentFiber( void )
{
    CExecThread *currentThread = GetCurrentThread();

    return currentThread->GetCurrentFiber();
}

void registerFiberPlugin( void )
{
    privateFiberEnvironmentRegister.RegisterPlugin( executiveManagerFactory );
}

END_NATIVE_EXECUTIVE