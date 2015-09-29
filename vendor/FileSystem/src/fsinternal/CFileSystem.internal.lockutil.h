/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.lockutil.h
*  PURPOSE:     Locking utilities for proper threading support
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_INTERNAL_LOCKING_UTILS_
#define _FILESYSTEM_INTERNAL_LOCKING_UTILS_

inline NativeExecutive::CReadWriteLock* MakeReadWriteLock( CFileSystem *fsys )
{
    if ( NativeExecutive::CExecutiveManager *nativeMan = fsys->nativeMan )
    {
        return nativeMan->CreateReadWriteLock();
    }

    return NULL;
}

inline void DeleteReadWriteLock( CFileSystem *fsys, NativeExecutive::CReadWriteLock *lock )
{
    if ( !lock )
        return;

    if ( NativeExecutive::CExecutiveManager *nativeMan = fsys->nativeMan )
    {
        nativeMan->CloseReadWriteLock( lock );
    }
}

#endif //_FILESYSTEM_INTERNAL_LOCKING_UTILS_