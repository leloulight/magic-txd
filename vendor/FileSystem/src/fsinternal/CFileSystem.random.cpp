/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.random.cpp
*  PURPOSE:     Random number generation
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

#include "CFileSystem.internal.h"

#include <PluginHelpers.h>

#include <random>

#include "CFileSystem.lock.hxx"

namespace fsrandom
{

static fsLockProvider _fileSysRNGLockProvider;

struct fsRandomGeneratorEnv
{
    inline void Initialize( CFileSystemNative *fsys )
    {
        return;
    }

    inline void Shutdown( CFileSystemNative *fsys )
    {
        return;
    }

    inline void operator = ( const fsRandomGeneratorEnv& right )
    {
        // Cannot assign random number generators.
        return;
    }

    // This may not be available on all systems.
    std::random_device _true_random;
};

static PluginDependantStructRegister <fsRandomGeneratorEnv, fileSystemFactory_t> fsRandomGeneratorRegister;

inline fsRandomGeneratorEnv* GetRandomEnv( CFileSystem *fsys )
{
    return fsRandomGeneratorRegister.GetPluginStruct( (CFileSystemNative*)fsys );
}

unsigned long getSystemRandom( CFileSystem *sys )
{
    fsRandomGeneratorEnv *env = GetRandomEnv( sys );

    assert( env != NULL );

    // Not sure whether std::random_device is thread-safe, so be careful here.
    NativeExecutive::CReadWriteWriteContextSafe <> consistency( _fileSysRNGLockProvider.GetReadWriteLock( sys ) );

    return env->_true_random();
}

};

void registerRandomGeneratorExtension( const fs_construction_params& params )
{
    fsrandom::_fileSysRNGLockProvider.RegisterPlugin( params );

    fsrandom::fsRandomGeneratorRegister.RegisterPlugin( _fileSysFactory );
}

void unregisterRandomGeneratorExtension( void )
{
    fsrandom::fsRandomGeneratorRegister.UnregisterPlugin();

    fsrandom::_fileSysRNGLockProvider.UnregisterPlugin();
}