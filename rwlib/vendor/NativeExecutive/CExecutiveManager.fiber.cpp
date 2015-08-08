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

#ifdef _WIN32
#include <excpt.h>
#include <windows.h>

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
    (EXCEPTION_REGISTRATION*)0xFFFFFFFF,
    _defaultHandler
};
#endif

BEGIN_NATIVE_EXECUTIVE

#pragma warning(disable:4733)

static void __declspec(naked) _retHandler( FiberStatus *userdata )
{
	__asm
	{
		// Set thread status to terminated
		mov eax,[esp+4]
        mov [eax]FiberStatus.status,FIBER_TERMINATED

        // Cache the termination function
        mov edx,[eax]FiberStatus.termcb

		mov ecx,eax

        // Apply registers
        mov eax,[eax]FiberStatus.callee
        mov ebx,[eax]Fiber.ebx
        mov edi,[eax]Fiber.edi
        mov esi,[eax]Fiber.esi
        mov esp,[eax]Fiber.esp
        mov ebp,[eax]Fiber.ebp

        push [eax]Fiber.eip // The return address
		push ecx			// Push userdata as preparation

#ifdef _WIN32
        // Save the termination function.
        push edx

        // Apply exception and stack info
        mov ecx,[eax]Fiber.stack_base
        mov edx,[eax]Fiber.stack_limit
        mov fs:[4],ecx
        mov fs:[8],edx
        mov ecx,[eax]Fiber.except_info
        mov fs:[0],ecx

        // Pop the termination function.
        pop edx
#endif

		// Terminate our thread
        call edx
        add esp,4

        ret
	}
}

// Global memory allocation functions.
static ExecutiveFiber::memalloc_t fiberMemAlloc = NULL;
static ExecutiveFiber::memfree_t fiberMemFree = NULL;

Fiber* ExecutiveFiber::newfiber( FiberStatus *userdata, size_t stackSize, FiberProcedure proc, FiberStatus::termfunc_t termcb )
{
    Fiber *env = (Fiber*)fiberMemAlloc( sizeof(Fiber) );

    // Allocate stack memory
    if ( stackSize == 0 )
        stackSize = 2 << 17;    // about 128 kilobytes of stack space (is this enough?)

    unsigned int *stack = (unsigned int*)fiberMemAlloc( stackSize );
    env->stack_base = (unsigned int*)( (unsigned int)stack + stackSize );
    env->stack_limit = stack;

    stack = env->stack_base;

    // Once entering, the first argument should be the thread
    *--stack = (unsigned int)userdata;
    *--stack = (unsigned int)exit;
    *--stack = (unsigned int)userdata;
    *--stack = (unsigned int)&_retHandler;
    env->esp = (unsigned int)stack;

    env->eip = (unsigned int)proc;
    
#ifdef _WIN32
    env->except_info = &_baseException;
#endif

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

void __declspec(naked) ExecutiveFiber::eswitch( Fiber *from, Fiber *to )
{
    __asm
    {
        // Save current environment
        mov eax,[esp+4]
        mov [eax]Fiber.ebx,ebx
        mov [eax]Fiber.edi,edi
        mov [eax]Fiber.esi,esi
        add esp,4
        mov [eax]Fiber.esp,esp
        mov ebx,[esp-4]
        mov [eax]Fiber.eip,ebx
        mov [eax]Fiber.ebp,ebp

#ifdef _WIN32
        // Save exception and stack info
        mov ebx,fs:[0]
        mov ecx,fs:[4]
        mov edx,fs:[8]
        mov [eax]Fiber.stack_base,ecx
        mov [eax]Fiber.stack_limit,edx
        mov [eax]Fiber.except_info,ebx
#endif

        // Apply registers
        mov eax,[esp+4]
        mov ebx,[eax]Fiber.ebx
        mov edi,[eax]Fiber.edi
        mov esi,[eax]Fiber.esi
        mov esp,[eax]Fiber.esp
        mov ebp,[eax]Fiber.ebp

#ifdef _WIN32
        // Apply exception and stack info
        mov ecx,[eax]Fiber.stack_base
        mov edx,[eax]Fiber.stack_limit
        mov fs:[4],ecx
        mov fs:[8],edx
        mov ecx,[eax]Fiber.except_info
        mov fs:[0],ecx
#endif

        jmp dword ptr[eax]Fiber.eip
    }
}

// For use with yielding
void __declspec(naked) ExecutiveFiber::qswitch( Fiber *from, Fiber *to )
{
    __asm
    {
        // Save current environment
        mov eax,[esp+4]         // grab the "from" context
        mov [eax]Fiber.ebx,ebx
        mov [eax]Fiber.edi,edi
        mov [eax]Fiber.esi,esi
        add esp,4
        mov [eax]Fiber.esp,esp
        mov ebx,[esp-4]
        mov [eax]Fiber.eip,ebx
        mov [eax]Fiber.ebp,ebp

#ifdef _WIN32
        // Save exception info
        mov ebx,fs:[0]
        mov [eax]Fiber.except_info,ebx
#endif

        // Apply registers
        mov eax,[esp+4]         // grab the "to" context 
        mov ebx,[eax]Fiber.ebx
        mov edi,[eax]Fiber.edi
        mov esi,[eax]Fiber.esi
        mov esp,[eax]Fiber.esp
        mov ebp,[eax]Fiber.ebp

#ifdef _WIN32
        // Apply exception and stack info
        mov ecx,[eax]Fiber.stack_base
        mov edx,[eax]Fiber.stack_limit
        mov fs:[4],ecx
        mov fs:[8],edx
        mov ecx,[eax]Fiber.except_info
        mov fs:[0],ecx
#endif

        jmp dword ptr[eax]Fiber.eip
    }
}

// For use with one-sided context switching.
// Do not use this routine in complicated C++ prototypes, as the CRT may break!
void __declspec(naked) ExecutiveFiber::leave( Fiber *to )
{
    // To call this function, jump to it using ASM.
    // Put the first argument into EAX.
    __asm
    {
        // Apply registers
        //mov eax,[esp]           // grab the "to" context 
        mov ebx,[eax]Fiber.ebx
        mov edi,[eax]Fiber.edi
        mov esi,[eax]Fiber.esi
        mov esp,[eax]Fiber.esp
        mov ebp,[eax]Fiber.ebp

#ifdef _WIN32
        // Apply exception and stack info
        mov ecx,[eax]Fiber.stack_base
        mov edx,[eax]Fiber.stack_limit
        mov fs:[4],ecx
        mov fs:[8],edx
        mov ecx,[eax]Fiber.except_info
        mov fs:[0],ecx
#endif

        jmp dword ptr[eax]Fiber.eip
    }
}

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
    typedef growableArray <CFiber*, 2, 0, fiberArrayAllocManager, unsigned int> fiberArray;

    // Fiber stacks per thread!
    fiberArray fiberStack;
};

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

    ExecutiveManager::threadPluginContainer_t::pluginOffset_t threadFiberStackPluginOffset;
};

static PluginDependantStructRegister <privateFiberEnvironment, executiveManagerFactory_t> privateFiberEnvironmentRegister;

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

CFiber* CExecutiveManager::GetCurrentFiber( void )
{
    CFiber *currentFiber = NULL;

    CExecThread *currentThread = GetCurrentThread();

    if ( threadFiberStackPluginInfo *info = GetThreadFiberStackPlugin( this, currentThread ) )
    {
        unsigned int fiberCount = info->fiberStack.GetCount();

        return ( fiberCount != 0 ) ? ( info->fiberStack.Get( fiberCount - 1 ) ) : ( NULL );
    }

    return currentFiber;
}

void registerFiberPlugin( void )
{
    privateFiberEnvironmentRegister.RegisterPlugin( executiveManagerFactory );
}

END_NATIVE_EXECUTIVE