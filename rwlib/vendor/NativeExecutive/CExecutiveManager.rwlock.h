/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.h
*  PURPOSE:     Read/Write lock synchronization object
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_READ_WRITE_LOCK_
#define _NATIVE_EXECUTIVE_READ_WRITE_LOCK_

BEGIN_NATIVE_EXECUTIVE

/*
    Synchronization object - the "Read/Write" lock

    Use this sync object if you have a data structure that requires consistency in a multi-threaded environment.
    Just like any other sync object it prevents instruction reordering where it changes the designed functionality.
    But the speciality of this object is that it allows two access modes.

    In typical data structure development, read operations do not change the state of an object. This allows
    multiple threads to run concurrently and still keep the logic of the data intact. This assumption is easily
    warded off if the data structure keeps shadow data for optimization purposes (mutable variables).

    Then there is the writing mode. In this mode threads want exclusive access to a data structure, as concurrent
    modification on a data structure is a daunting task and most often is impossible to solve fast and clean.

    By using this object to mark critical read and write regions in your code, you easily make it thread-safe.
    Thread-safety is the future, as silicon has reached its single-threaded performance peak.

    Please make sure that you use this object in an exception-safe way to prevent dead-locks! This structure does
    not support recursive acquisition, so be careful how you do things!
*/
struct CReadWriteLock abstract
{
    // Shared access to data structures.
    void EnterCriticalReadRegion( void );
    void LeaveCriticalReadRegion( void );

    // Exclusive access to data structures.
    void EnterCriticalWriteRegion( void );
    void LeaveCriticalWriteRegion( void );

    // Attempting to enter the lock while preventing a wait scenario.
    bool TryEnterCriticalReadRegion( void );
    bool TryEnterCriticalWriteRegion( void );
};

/*
    Synchronization object - the reentrant "Read/Write" lock

    This is a variant of CReadWriteLock but it is reentrant. This means that write contexts can be entered multiple times
    on one thread, leaving them the same amount of times to unlock again.

    Due to the reentrance feature this lock is slower than CReadWriteLock.
*/
struct CReentrantReadWriteLock abstract
{
    // Shared access to data structures.
    void EnterCriticalReadRegion( void );
    void LeaveCriticalReadRegion( void );

    // Exclusive access to data structures.
    void EnterCriticalWriteRegion( void );
    void LeaveCriticalWriteRegion( void );
};

// Lock context helpers for exception safe and correct code region marking.
template <typename rwLockType = CReadWriteLock>
struct CReadWriteReadContext
{
    inline CReadWriteReadContext( rwLockType *theLock )
    {
        theLock->EnterCriticalReadRegion();

        this->theLock = theLock;
    }

    inline ~CReadWriteReadContext( void )
    {
        this->theLock->LeaveCriticalReadRegion();
    }

    rwLockType *theLock;
};

template <typename rwLockType = CReadWriteLock>
struct CReadWriteWriteContext
{
    inline CReadWriteWriteContext( rwLockType *theLock )
    {
        theLock->EnterCriticalWriteRegion();

        this->theLock = theLock;
    }

    inline ~CReadWriteWriteContext( void )
    {
        this->theLock->LeaveCriticalWriteRegion();
    }

    rwLockType *theLock;
};

// Variants of locking contexts that accept a NULL pointer.
template <typename rwLockType = CReadWriteLock>
struct CReadWriteReadContextSafe
{
    inline CReadWriteReadContextSafe( rwLockType *theLock )
    {
        if ( theLock )
        {
            theLock->EnterCriticalReadRegion();
        }

        this->theLock = theLock;
    }

    inline ~CReadWriteReadContextSafe( void )
    {
        if ( rwLockType *theLock = this->theLock )
        {
            theLock->LeaveCriticalReadRegion();
        }
    }

    rwLockType *theLock;
};

template <typename rwLockType = CReadWriteLock>
struct CReadWriteWriteContextSafe
{
    inline CReadWriteWriteContextSafe( rwLockType *theLock )
    {
        if ( theLock )
        {
            theLock->EnterCriticalWriteRegion();
        }
        
        this->theLock = theLock;
    }

    inline ~CReadWriteWriteContextSafe( void )
    {
        if ( rwLockType *theLock = this->theLock )
        {
            theLock->LeaveCriticalWriteRegion();
        }
    }

    rwLockType *theLock;
};

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_READ_WRITE_LOCK_