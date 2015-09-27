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

#include <random>

namespace fsrandom
{

static std::random_device _true_random;

unsigned long getSystemRandom( CFileSystem *sys )
{
    return _true_random();
}

};