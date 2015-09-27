/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.random.h
*  PURPOSE:     Random number generation
*  DEVELOPERS:  Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_RANDOM_NUMBER_GENERATOR_
#define _FILESYSTEM_RANDOM_NUMBER_GENERATOR_

namespace fsrandom
{

unsigned long getSystemRandom( CFileSystem *sys );

};

#endif //_FILESYSTEM_RANDOM_NUMBER_GENERATOR_