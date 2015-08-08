/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.rwlock.cpp
*  PURPOSE:     Read/Write lock synchronization object
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

BEGIN_NATIVE_EXECUTIVE

void CReadWriteLock::EnterCriticalReadRegion( void )
{
    ((CReadWriteLockNative*)this)->EnterCriticalReadRegionNative();
}

void CReadWriteLock::LeaveCriticalReadRegion( void )
{
    ((CReadWriteLockNative*)this)->LeaveCriticalReadRegionNative();
}

void CReadWriteLock::EnterCriticalWriteRegion( void )
{
    ((CReadWriteLockNative*)this)->EnterCriticalWriteRegionNative();
}

void CReadWriteLock::LeaveCriticalWriteRegion( void )
{
    ((CReadWriteLockNative*)this)->LeaveCriticalWriteRegionNative();
}

bool CReadWriteLock::TryEnterCriticalReadRegion( void )
{
    return ((CReadWriteLockNative*)this)->TryEnterCriticalReadRegionNative();
}

bool CReadWriteLock::TryEnterCriticalWriteRegion( void )
{
    return ((CReadWriteLockNative*)this)->TryEnterCriticalWriteRegionNative();
}

// Executive manager API.
CReadWriteLock* CExecutiveManager::CreateReadWriteLock( void )
{
    return new CReadWriteLockNative();
}

void CExecutiveManager::CloseReadWriteLock( CReadWriteLock *theLock )
{
    CReadWriteLockNative *lockImpl = (CReadWriteLockNative*)theLock;

    delete lockImpl;
}

size_t CExecutiveManager::GetReadWriteLockStructSize( void )
{
    return sizeof( CReadWriteLockNative );
}

CReadWriteLock* CExecutiveManager::CreatePlacedReadWriteLock( void *mem )
{
    return new (mem) CReadWriteLockNative();
}

void CExecutiveManager::ClosePlacedReadWriteLock( CReadWriteLock *placedLock )
{
    CReadWriteLockNative *lockImpl = (CReadWriteLockNative*)placedLock;

    lockImpl->~CReadWriteLockNative();
}

// Reentrant RW lock implementation.
void CReentrantReadWriteLock::EnterCriticalReadRegion( void )
{
    ((CReentrantReadWriteLockNative*)this)->EnterCriticalReadRegionNative();
}

void CReentrantReadWriteLock::LeaveCriticalReadRegion( void )
{
    ((CReentrantReadWriteLockNative*)this)->LeaveCriticalReadRegionNative();
}

void CReentrantReadWriteLock::EnterCriticalWriteRegion( void )
{
    ((CReentrantReadWriteLockNative*)this)->EnterCriticalWriteRegionNative();
}

void CReentrantReadWriteLock::LeaveCriticalWriteRegion( void )
{
    ((CReentrantReadWriteLockNative*)this)->LeaveCriticalWriteRegionNative();
}

// Executive manager API.
CReentrantReadWriteLock* CExecutiveManager::CreateReentrantReadWriteLock( void )
{
    return new CReentrantReadWriteLockNative();
}

void CExecutiveManager::CloseReentrantReadWriteLock( CReentrantReadWriteLock *theLock )
{
    delete theLock;
}

size_t CExecutiveManager::GetReentrantReadWriteLockStructSize( void )
{
    return sizeof( CReentrantReadWriteLockNative );
}

CReentrantReadWriteLock* CExecutiveManager::CreatePlacedReentrantReadWriteLock( void *mem )
{
    return new (mem) CReentrantReadWriteLockNative();
}

void CExecutiveManager::ClosePlacedReentrantReadWriteLock( CReentrantReadWriteLock *theLock )
{
    ((CReentrantReadWriteLockNative*)theLock)->~CReentrantReadWriteLockNative();
}                                                                                                                                                                          

END_NATIVE_EXECUTIVE