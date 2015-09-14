/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.cpp
*  PURPOSE:     File management
*  DEVELOPERS:  S2 Games <http://savage.s2games.com> (historical entry)
*               Martin Turski <quiret@gmx.de>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>
#include <sstream>
#include <sys/stat.h>

// Include internal header.
#include "fsinternal/CFileSystem.internal.h"

// This variable is exported across the whole FileSystem library.
// It should be used by CRawFile classes that are created.
std::list <CFile*> *openFiles;

CFileSystem *fileSystem = NULL;
CFileTranslator *fileRoot = NULL;

#include "CFileSystem.Utils.hxx"

// Create the class at runtime initialization.
CSystemCapabilities systemCapabilities;

// Constructor of the CFileSystem instance.
// Every driver should register itself in this.
fileSystemFactory_t _fileSysFactory;

// Allocator of plugin meta-data.
struct _fileSystemAllocator
{
    inline void* Allocate( size_t memSize )
    {
        return new char[ memSize ];
    }

    inline void Free( void *memPtr, size_t memSize )
    {
        delete [] (char*)memPtr;
    }
};
static _fileSystemAllocator _memAlloc;

// Global string for the double-dot.
const filePath _dirBack( ".." );

/*=======================================
    CFileSystem

    Management class with root-access functions.
    These methods are root-access. Exposing them
    to a security-critical user-space context is
    not viable.
=======================================*/
static bool _hasBeenInitialized = false;

// Integrity check function.
// If this fails, then CFileSystem cannot boot.
inline bool _CheckLibraryIntegrity( void )
{
    // Check all data types.
    bool isValid = true;

    if ( sizeof(fsBool_t) != 1 ||
         sizeof(fsChar_t) != 1 || sizeof(fsUChar_t) != 1 ||
         sizeof(fsShort_t) != 2 || sizeof(fsUShort_t) != 2 ||
         sizeof(fsInt_t) != 4 || sizeof(fsUInt_t) != 4 ||
         sizeof(fsWideInt_t) != 8 || sizeof(fsUWideInt_t) != 8 ||
         sizeof(fsFloat_t) != 4 ||
         sizeof(fsDouble_t) != 8 )
    {
        isValid = false;
    }

    if ( !isValid )
    {
        // Notify the developer.
        __debugbreak();
    }

    return isValid;
}

// Creators of the CFileSystem instance.
// Those are the entry points to this static library.
CFileSystem* CFileSystem::Create( void )
{
    // Make sure that there is no second CFileSystem class alive.
    assert( _hasBeenInitialized == false );

    // Make sure our environment can run CFileSystem in the first place.
    // I need to sort out some compatibility problems before allowing a x64 build.
    bool isLibraryBootable = _CheckLibraryIntegrity();

    if ( !isLibraryBootable )
    {
        // We failed some critical integrity tests.
        return NULL;
    }

    // Register addons.
    CFileSystemNative::RegisterZIPDriver();
    CFileSystemNative::RegisterIMGDriver();

    // Create our CFileSystem instance!
    CFileSystemNative *instance = _fileSysFactory.Construct( _memAlloc );

    if ( instance )
    {
        // Get the application current directory and store it in "fileRoot" global.
        {
            char cwd[1024];
            getcwd( cwd, 1023 );

            // Make sure it is a correct directory
            filePath cwd_ex( cwd );
            cwd_ex += '\\';

            // Every application should be able to access itself
            fileRoot = instance->CreateTranslator( cwd_ex );
        }

        // Set the global fileSystem variable.
        fileSystem = instance;

        openFiles = new std::list<CFile*>;

        // We have initialized ourselves.
        _hasBeenInitialized = true;
    }

    return instance;
}

void CFileSystem::Destroy( CFileSystem *lib )
{
    CFileSystemNative *nativeLib = (CFileSystemNative*)lib;

    assert( nativeLib != NULL );

    if ( nativeLib )
    {
        // Delete the main fileRoot access point.
        if ( CFileTranslator *rootAppDir = fileRoot )
        {
            delete rootAppDir;

            fileRoot = NULL;
        }

        _fileSysFactory.Destroy( _memAlloc, nativeLib );

        // Unregister all addons.
        CFileSystemNative::UnregisterIMGDriver();
        CFileSystemNative::UnregisterZIPDriver();

        delete openFiles;

        // Zero the main FileSystem access point.
        fileSystem = NULL;

        // We have successfully destroyed FileSystem activity.
        _hasBeenInitialized = false;
    }
}

#ifdef _WIN32

struct MySecurityAttributes
{
    DWORD count;
    LUID_AND_ATTRIBUTES attr[2];
};

#endif //_WIN32

CFileSystem::CFileSystem( void )
{
    // Set up members.
    m_includeAllDirsInScan = false;
#ifdef _WIN32
    m_hasDirectoryAccessPriviledge = false;
#endif //_WIN32

    this->sysTmp = NULL;
}

CFileSystem::~CFileSystem( void )
{
    // Shutdown addon management.
    if ( sysTmp )
    {
        delete sysTmp;

        sysTmp = NULL;
    }
}

bool CFileSystem::CanLockDirectories( void )
{
    // We should set special priviledges for the application if
    // running under Win32.
#ifdef _WIN32
    // We assume getting the priviledge once is enough.
    if ( !m_hasDirectoryAccessPriviledge )
    {
        HANDLE token;

        // We need SE_BACKUP_NAME to gain directory access on Windows
        if ( OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token ) == TRUE )
        {
            MySecurityAttributes priv;

            priv.count = 2; // we want to request two priviledges.

            BOOL backupRequest = LookupPrivilegeValue( NULL, SE_BACKUP_NAME, &priv.attr[0].Luid );

            priv.attr[0].Attributes = SE_PRIVILEGE_ENABLED;

            BOOL restoreRequest = LookupPrivilegeValue( NULL, SE_RESTORE_NAME, &priv.attr[1].Luid );

            priv.attr[1].Attributes = SE_PRIVILEGE_ENABLED;

            if ( backupRequest == TRUE && restoreRequest == TRUE )
            {
                BOOL success = AdjustTokenPrivileges( token, false, (TOKEN_PRIVILEGES*)&priv, sizeof( priv ), NULL, NULL );

                if ( success == TRUE )
                {
                    m_hasDirectoryAccessPriviledge = true;
                }
            }

            CloseHandle( token );
        }
    }
    return m_hasDirectoryAccessPriviledge;
#elif defined(__linux__)
    // We assume that we can always lock directories under Unix.
    // This is actually a lie, because it does not exist.
    return true;
#else
    // No idea about directory locking on other environments.
    return false;
#endif //OS DEPENDENT CODE
}

CFileTranslator* CFileSystem::CreateTranslator( const char *path )
{
    // Without access to directory locking, this function can not execute.
    if ( !CanLockDirectories() )
        return NULL;

    CSystemFileTranslator *pTranslator;
    filePath root;
    dirTree tree;
    bool bFile;

#ifdef _WIN32
    // We have to handle absolute path, too
    if ( _File_IsAbsolutePath( path ) )
    {
        if (!_File_ParseRelativePath( path + 3, tree, bFile ))
            return NULL;

        root += path[0];
        root += ":/";
    }
#elif defined(__linux__)
    if ( *path == '/' || *path == '\\' )
    {
        if (!_File_ParseRelativePath( path + 1, tree, bFile ))
            return NULL;

        root = "/";
    }
#endif //OS DEPENDANT CODE
    else
    {
        char pathBuffer[1024];
        getcwd( pathBuffer, sizeof(pathBuffer) );

        root += pathBuffer;
        root += "\\";
        root += path;

#ifdef _WIN32
        if (!_File_ParseRelativePath( root.c_str() + 3, tree, bFile ))
            return NULL;

        root.resize( 2 );
        root += "/";
#elif defined(__linux__)
        if (!_File_ParseRelativePath( root.c_str() + 1, tree, bFile ))
            return NULL;

        root = "/";
#endif //OS DEPENDANT CODE
    }

    if ( bFile )
        tree.pop_back();

    _File_OutputPathTree( tree, false, root );

#ifdef _WIN32
    HANDLE dir = CreateFile( root.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );

    if ( dir == INVALID_HANDLE_VALUE )
        return NULL;
#elif defined(__linux__)
    DIR *dir = opendir( root.c_str() );

    if ( dir == NULL )
        return NULL;
#else
    if ( !IsDirectory( root.c_str() ) )
        return NULL;
#endif //OS DEPENDANT CODE

    pTranslator = new CSystemFileTranslator();
    pTranslator->m_root = root;
    pTranslator->m_rootTree = tree;

#ifdef _WIN32
    pTranslator->m_rootHandle = dir;
    pTranslator->m_curDirHandle = NULL;
#elif defined(__linux__)
    pTranslator->m_rootHandle = dir;
    pTranslator->m_curDirHandle = NULL;
#endif //OS DEPENDANT CODE

    return pTranslator;
}

CFileTranslator* CFileSystem::GenerateTempRepository( void )
{
    filePath tmpDir;

    // Check whether we have a handle to the global temporary system storage.
    // If not, attempt to retrieve it.
    if ( !this->sysTmp )
    {
#ifdef _WIN32
        char buf[1024];

        GetTempPath( sizeof( buf ), buf );

        // Transform the path into something we can recognize.
        tmpDir.insert( 0, buf, 2 );
        tmpDir += '/';

        dirTree tree;
        bool isFile;
        
        bool parseSuccess = _File_ParseRelativePath( buf + 3, tree, isFile );

        assert( parseSuccess == true && isFile == false );

        _File_OutputPathTree( tree, isFile, tmpDir );
#elif defined(__linux__)
        const char *dir = getenv("TEMPDIR");

        if ( !dir )
            tmpDir = "/tmp";
        else
            tmpDir = dir;

        tmpDir += '/';

        // On linux we cannot be sure that our directory exists.
        if ( !_File_CreateDirectory( tmpDir ) )
            exit( 7098 );
#endif //OS DEPENDANT CODE

        this->sysTmp = fileSystem->CreateTranslator( tmpDir.c_str() );

        // We failed to get the handle to the temporary storage, hence we cannot deposit temporary files!
        if ( !this->sysTmp )
            return NULL;
    }
    else
    {
        bool success = this->sysTmp->GetFullPath( "@", false, tmpDir );

        if ( !success )
            return NULL;
    }

    // Generate a random sub-directory inside of the global OS temp directory.
    {
        std::stringstream stream;

        stream.precision( 0 );
        stream << ( rand() % 647251833 );

        tmpDir += "&$!reAr";
        tmpDir += stream.str();
        tmpDir += "_/";
    }

    // Make sure the temporary directory exists.
    bool creationSuccessful = _File_CreateDirectory( tmpDir );

    if ( creationSuccessful )
    {
        // Create the .zip temporary root
        CFileTranslator *result = fileSystem->CreateTranslator( tmpDir.c_str() );

        if ( result )
            return result;
    }

    return NULL;
}

bool CFileSystem::IsDirectory( const char *path )
{
    return File_IsDirectoryAbsolute( path );
}

#ifdef _WIN32

// By definition, crash dumps are OS dependant.
// Currently we only support crash dumps on Windows OS.

bool CFileSystem::WriteMiniDump( const char *path, _EXCEPTION_POINTERS *except )
{
#if 0
    CRawFile *file = (CRawFile*)fileRoot->Open( path, "wb" );
    MINIDUMP_EXCEPTION_INFORMATION info;

    if ( !file )
        return false;

    // Create an exception information struct
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = except;
    info.ClientPointers = false;

    // Write the dump
    MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file->m_file, MiniDumpNormal, &info, NULL, NULL );

    delete file;
#endif

    return true;
}
#endif //_WIN32

bool CFileSystem::Exists( const char *path )
{
    struct stat tmp;

    return stat( path, &tmp ) == 0;
}

size_t CFileSystem::Size( const char *path )
{
    struct stat stats;

    if ( stat( path, &stats ) != 0 )
        return 0;

    return stats.st_size;
}

// Utility to quickly load data from files on the local filesystem.
// Do not export it into user-space since this function has no security restrictions.
bool CFileSystem::ReadToBuffer( const char *path, std::vector <char>& output )
{
#ifdef _WIN32
    HANDLE file = CreateFile( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );

    if ( file == INVALID_HANDLE_VALUE )
        return false;

    size_t size = GetFileSize( file, NULL );

    if ( size != 0 )
    {
        output.resize( size );
        output.reserve( size );

        DWORD _pf;

        ReadFile( file, &output[0], (DWORD)size, &_pf, NULL );
    }
    else
        output.clear();

    CloseHandle( file );
    return true;
#elif defined(__linux__)
    int iReadFile = open( path, O_RDONLY, 0 );

    if ( iReadFile == -1 )
        return false;

    struct stat read_info;

    if ( fstat( iReadFile, &read_info ) != 0 )
        return false;

    if ( read_info.st_size != 0 )
    {
        output.resize( read_info.st_size );
        output.reserve( read_info.st_size );

        ssize_t numRead = read( iReadFile, &output[0], read_info.st_size );

        if ( numRead == 0 )
        {
            close( iReadFile );
            return false;
        }

        if ( numRead != read_info.st_size )
            output.resize( numRead );
    }
    else
        output.clear();

    close( iReadFile );
    return true;
#else
    return false;
#endif //OS DEPENDANT CODE
}
