#ifndef _NATIVE_EXECUTIVE_READ_WRITE_LOCK_INTERNAL_
#define _NATIVE_EXECUTIVE_READ_WRITE_LOCK_INTERNAL_

#include <atomic>
#include <condition_variable>
#include <thread>

BEGIN_NATIVE_EXECUTIVE

// Actual implementation of CReadWriteLock.
struct CReadWriteLockNative : public CReadWriteLock
{
    inline CReadWriteLockNative( void )
    {
#ifdef _WIN32
        // We just have to initialize stuff once.
        InitializeSRWLock( &_nativeSRW );
#endif //_WIN32
    }

    inline ~CReadWriteLockNative( void )
    {
        return;
    }

    inline void EnterCriticalReadRegionNative( void )
    {
#ifdef _WIN32
        AcquireSRWLockShared( &_nativeSRW );
#else
#error Missing implementation
#endif //_WIN32
    }

    inline void LeaveCriticalReadRegionNative( void )
    {
#ifdef _WIN32
        ReleaseSRWLockShared( &_nativeSRW );
#else
#error Missing implementation
#endif //_WIN32
    }

    inline void EnterCriticalWriteRegionNative( void )
    {
#ifdef _WIN32
        AcquireSRWLockExclusive( &_nativeSRW );
#else
#error Missing implementation
#endif //_WIN32
    }

    inline void LeaveCriticalWriteRegionNative( void )
    {
#ifdef _WIN32
        ReleaseSRWLockExclusive( &_nativeSRW );
#else
#error Missing implementation
#endif //_WIN32
    }

    inline bool TryEnterCriticalReadRegionNative( void )
    {
#ifdef _WIN32
        return ( TryAcquireSRWLockShared( &_nativeSRW ) == TRUE );
#else
#error Missing implementation
#endif //_WIN32
    }

    inline bool TryEnterCriticalWriteRegionNative( void )
    {
#ifdef _WIN32
        return ( TryAcquireSRWLockExclusive( &_nativeSRW ) == TRUE );
#else
#error Missing implementation
#endif //_WIN32
    }

#ifdef _WIN32
    SRWLOCK _nativeSRW;
#endif //_WIN32
};

// Actual implementation of CReentrantReadWriteLock
class exclusive_lock
{
public:
    inline exclusive_lock( void )
#ifdef _WIN32
        : srwLock( SRWLOCK_INIT )
#else
        : lockIsTaken( false )
#endif
    {
        return;
    }

    inline void enter( void )
    {
#ifdef _WIN32
        AcquireSRWLockExclusive( &srwLock );
#else
        while ( true )
        {
            bool wantToTakeOrWasTaken = true;

            bool hasTaken = this->lockIsTaken.compare_exchange_strong( wantToTakeOrWasTaken, true );

            if ( hasTaken )
            {
                break;
            }

            std::unique_lock <std::mutex> ul_isLocked( this->mutex_isLocked );

            isLocked.wait( ul_isLocked );
        }
#endif
    }

    inline void leave( void )
    {
#ifdef _WIN32
        ReleaseSRWLockExclusive( &srwLock );
#else
        this->lockIsTaken.store( false );

        isLocked.notify_one();
#endif
    }

private:
#ifdef _WIN32
    SRWLOCK srwLock;
#else
    std::mutex mutex_isLocked;
    std::condition_variable isLocked;
    std::atomic <bool> lockIsTaken;
#endif
};

class ctxlock_reentrant
{
public:
    typedef void* context_t;
    
    inline ctxlock_reentrant( void )
    {
        this->contextCount = 0;
        this->currentContext = NULL;    // NULL could be used by the runtime as shared context.
    }

private:
    inline bool IsContextTaken( void *ctxVal )
    {
        return ( this->contextCount != 0 && this->currentContext != ctxVal );
    }
    
public:
    inline void enter( context_t ctxVal )
    {
        while ( true )
        {
            bool couldBeFree = ( IsContextTaken( ctxVal ) == false );

            if ( couldBeFree )
            {
                mutexContextCritical.enter();

                bool isActuallyFree = ( IsContextTaken( ctxVal ) == false );

                if ( isActuallyFree )
                {
                    break;
                }

                mutexContextCritical.leave();
            }

            continue;
        }

        this->contextCount++;
        this->currentContext = ctxVal;

        mutexContextCritical.leave();
    }
    
    inline void leave( void )
    {
        // We can do that safely, as it is an atomic variable.
        this->contextCount--;
    }

private:
    exclusive_lock mutexContextCritical;

    std::atomic <unsigned long> contextCount;
    volatile context_t currentContext;
};

class rwlock_reentrant_bythread
{
    ctxlock_reentrant ctxLock;
public:
    inline void enter_write( void )
    {
        ctxLock.enter( (ctxlock_reentrant::context_t)std::hash <std::thread::id> () ( std::this_thread::get_id() ) );
    }

    inline void leave_write( void )
    {
        ctxLock.leave();
    }

    inline void enter_read( void )
    {
        ctxLock.enter( NULL );
    }

    inline void leave_read( void )
    {
        ctxLock.leave();
    }
};

struct CReentrantReadWriteLockNative : public CReentrantReadWriteLock
{
    inline CReentrantReadWriteLockNative( void )
    {
        return;
    }

    inline ~CReentrantReadWriteLockNative( void )
    {
        return;
    }

    inline void EnterCriticalReadRegionNative( void )
    {
        this->ctxLock.enter_read();
    }

    inline void LeaveCriticalReadRegionNative( void )
    {
        this->ctxLock.leave_read();
    }

    inline void EnterCriticalWriteRegionNative( void )
    {
        this->ctxLock.enter_write();
    }

    inline void LeaveCriticalWriteRegionNative( void )
    {
        this->ctxLock.leave_write();
    }

    rwlock_reentrant_bythread ctxLock;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_READ_WRITE_LOCK_INTERNAL_