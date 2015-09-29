/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.lock.hxx
*  PURPOSE:     Threading support definitions for CFileSystem
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_MAIN_THREADING_SUPPORT_
#define _FILESYSTEM_MAIN_THREADING_SUPPORT_

struct fsLockProvider
{
    inline fsLockProvider( void )
    {
        this->pluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
    }

    inline ~fsLockProvider( void )
    {
        assert( this->pluginOffset == fileSystemFactory_t::INVALID_PLUGIN_OFFSET );
    }

    struct lock_item
    {
        inline void Initialize( CFileSystemNative *fsys )
        {
            fsys->nativeMan->CreatePlacedReadWriteLock( this );
        }

        inline void Shutdown( CFileSystemNative *fsys )
        {
            fsys->nativeMan->ClosePlacedReadWriteLock( (NativeExecutive::CReadWriteLock*)this );
        }
    };

    inline NativeExecutive::CReadWriteLock* GetReadWriteLock( const CFileSystem *fsys )
    {
        return (NativeExecutive::CReadWriteLock*)fileSystemFactory_t::RESOLVE_STRUCT <NativeExecutive::CReadWriteLock> ( (const CFileSystemNative*)fsys, this->pluginOffset );
    }
    
    inline void RegisterPlugin( const fs_construction_params& params )
    {
        // We only want to register the lock plugin if the FileSystem has threading support.
        if ( NativeExecutive::CExecutiveManager *nativeMan = params.nativeExecMan )
        {
            size_t lock_size = nativeMan->GetReadWriteLockStructSize();

            this->pluginOffset =
                _fileSysFactory.RegisterDependantStructPlugin <lock_item> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID, lock_size );
        }
    }

    inline void UnregisterPlugin( void )
    {
        if ( fileSystemFactory_t::IsOffsetValid( this->pluginOffset ) )
        {
            _fileSysFactory.UnregisterPlugin( this->pluginOffset );

            this->pluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
        }
    }

    fileSystemFactory_t::pluginOffset_t pluginOffset;
};

#endif //_FILESYSTEM_MAIN_THREADING_SUPPORT_