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

namespace fsrandom
{

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

    return env->_true_random();
}

};

void registerRandomGeneratorExtension( void )
{
    fsrandom::fsRandomGeneratorRegister.RegisterPlugin( _fileSysFactory );
}

void unregisterRandomGeneratorExtension( void )
{
    fsrandom::fsRandomGeneratorRegister.UnregisterPlugin();
}