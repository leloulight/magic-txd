/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.h
*  PURPOSE:     Master header of the internal FileSystem modules
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_INTERNAL_LOGIC_
#define _FILESYSTEM_INTERNAL_LOGIC_

#ifndef _WIN32
#define FILE_MODE_CREATE    0x01
#define FILE_MODE_OPEN      0x02

#define FILE_ACCESS_WRITE   0x01
#define FILE_ACCESS_READ    0x02

#define MAX_PATH 260
#else
#include <direct.h>

#define FILE_MODE_CREATE    CREATE_ALWAYS
#define FILE_MODE_OPEN      OPEN_EXISTING

#define FILE_ACCESS_WRITE   GENERIC_WRITE
#define FILE_ACCESS_READ    GENERIC_READ
#endif

#ifdef __linux__
#include <unistd.h>
#include <dirent.h>
#endif //__linux__

// FileSystem is plugin-based. Drivers register a plugin memory to build on.
#include <MemoryUtils.h>

// The native class of the FileSystem library.
struct CFileSystemNative : public CFileSystem
{
public:
    inline CFileSystemNative( void )
    {
        return;
    }

    inline ~CFileSystemNative( void )
    {
        return;
    }

    // All drivers have to add their registration routines here.
    static void RegisterZIPDriver( void );
    static void UnregisterZIPDriver( void );

    static void RegisterIMGDriver( void );
    static void UnregisterIMGDriver( void );

    // Generic things.
    template <typename charType>
    CFileTranslator* GenCreateTranslator( const charType *path );
};

typedef StaticPluginClassFactory <CFileSystemNative> fileSystemFactory_t;

extern fileSystemFactory_t _fileSysFactory;

#include "CFileSystem.internal.common.h"
#include "CFileSystem.random.h"
#include "CFileSystem.stream.raw.h"
#include "CFileSystem.stream.buffered.h"
#include "CFileSystem.translator.pathutil.h"
#include "CFileSystem.translator.system.h"
#include "CFileSystem.translator.widewrap.h"
#include "CFileSystem.internal.repo.h"
#include "CFileSystem.config.h"

/*===================================================
    CSystemCapabilities

    This class determines the system-dependant capabilities
    and exports methods that return them to the runtime.

    It does not depend on a properly initialized CFileSystem
    environment, but CFileSystem depends on it.

    Through-out the application runtime, this class stays
    immutable.
===================================================*/
class CSystemCapabilities
{
private:

#ifdef _WIN32
    struct diskInformation
    {
        // Immutable information of a hardware data storage.
        // Modelled on the basis of a hard disk.
        DWORD sectorsPerCluster;
        DWORD bytesPerSector;
        DWORD totalNumberOfClusters;
    };
#endif //_WIN32

public:
    CSystemCapabilities( void )
    {
        
    }

    inline size_t GetFailSafeSectorSize( void )
    {
        return 2048;
    }

    // Only applicable to raw-files.
    size_t GetSystemLocationSectorSize( char driveLetter )
    {
#ifdef _WIN32
        char systemDriveLocatorBuf[4];

        // systemLocation is assumed to be an absolute path to hardware.
        // Whether the input makes sense ultimatively lies on the shoulders of the callee.
        systemDriveLocatorBuf[0] = driveLetter;
        systemDriveLocatorBuf[1] = ':';
        systemDriveLocatorBuf[2] = '/';
        systemDriveLocatorBuf[3] = 0;

        // Attempt to get disk information.
        diskInformation diskInfo;
        DWORD freeSpace;

        BOOL success = GetDiskFreeSpaceA(
            systemDriveLocatorBuf,
            &diskInfo.sectorsPerCluster,
            &diskInfo.bytesPerSector,
            &freeSpace,
            &diskInfo.totalNumberOfClusters
        );

        // If the retrieval fails, we return a good value for anything.
        if ( success == FALSE )
            return GetFailSafeSectorSize();

        return diskInfo.bytesPerSector;
#elif __linux__
        // 2048 is always a good solution :)
        return GetFailSafeSectorSize();
#endif //PLATFORM DEPENDENT CODE
    }
};

// Export it from the main class file.
extern CSystemCapabilities systemCapabilities;

#endif //_FILESYSTEM_INTERNAL_LOGIC_