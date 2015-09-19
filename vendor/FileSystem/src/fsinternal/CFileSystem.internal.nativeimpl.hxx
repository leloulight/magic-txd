/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.nativeimpl.hxx
*  PURPOSE:     Native implementation utilities to share across files
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_
#define _FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_

#ifdef _WIN32

inline HANDLE _FileWin32_OpenDirectoryHandle( const filePath& absPath )
{
    HANDLE dir = INVALID_HANDLE_VALUE;

    if ( const char *sysPath = absPath.c_str() )
    {
        dir = CreateFileA( sysPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    }
    else if ( const wchar_t *sysPath = absPath.w_str() )
    {
        dir = CreateFileW( sysPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    }

    return dir;
}

#endif

#endif //_FILESYSTEM_NATIVE_SHARED_IMPLEMENTATION_PRIVATE_