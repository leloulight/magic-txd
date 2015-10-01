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

// Include native platform utilities.
#include "fsinternal/CFileSystem.internal.nativeimpl.hxx"

// Include threading utilities for CFileSystem class.
#include "CFileSystem.lock.hxx"

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

// Internal plugins.
fsLockProvider _fileSysLockProvider;
static fsLockProvider _fileSysTmpDirLockProvider;

// Sub modules.
extern void registerRandomGeneratorExtension( const fs_construction_params& params );

extern void unregisterRandomGeneratorExtension( void );

AINLINE void InitializeLibrary( const fs_construction_params& params )
{
    // Register addons.
    registerRandomGeneratorExtension( params );
    _fileSysLockProvider.RegisterPlugin( params );
    _fileSysTmpDirLockProvider.RegisterPlugin( params );

    CFileSystemNative::RegisterZIPDriver( params );
    CFileSystemNative::RegisterIMGDriver( params );
}

AINLINE void ShutdownLibrary( void )
{
    // Unregister all addons.
    CFileSystemNative::UnregisterIMGDriver();
    CFileSystemNative::UnregisterZIPDriver();

    _fileSysTmpDirLockProvider.UnregisterPlugin();
    _fileSysLockProvider.UnregisterPlugin();
    unregisterRandomGeneratorExtension();
}

struct fs_constructor
{
    const fs_construction_params& params;

    inline fs_constructor( const fs_construction_params& params ) : params( params )
    {
        return;
    }

    inline CFileSystemNative* Construct( void *mem ) const
    {
        return new (mem) CFileSystemNative( this->params );
    }
};

// Creators of the CFileSystem instance.
// Those are the entry points to this static library.
CFileSystem* CFileSystem::Create( const fs_construction_params& params )
{
    // Make sure that there is no second CFileSystem class alive.
    assert( _hasBeenInitialized == false );

    // Make sure our environment can run CFileSystem in the first place.
    bool isLibraryBootable = _CheckLibraryIntegrity();

    if ( !isLibraryBootable )
    {
        // We failed some critical integrity tests.
        return NULL;
    }

    InitializeLibrary( params );

    // Create our CFileSystem instance!
    fs_constructor constructor( params );

    CFileSystemNative *instance = _fileSysFactory.ConstructTemplate( _memAlloc, constructor );

    if ( instance )
    {
        // Get the application current directory and store it in "fileRoot" global.
        {
            wchar_t cwd[1024];
            _wgetcwd( cwd, 1023 );

            // Make sure it is a correct directory
            filePath cwd_ex( cwd );
            cwd_ex += L'\\';

            // Every application should be able to access itself
            fileRoot = instance->CreateTranslator( cwd_ex.w_str() );
        }

        // We have initialized ourselves.
        _hasBeenInitialized = true;
    }

    if ( !_hasBeenInitialized )
    {
        ShutdownLibrary();
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
        
        ShutdownLibrary();

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

CFileSystem::CFileSystem( const fs_construction_params& params )
{
    // Set up members.
    m_includeAllDirsInScan = false;
#ifdef _WIN32
    m_hasDirectoryAccessPriviledge = false;
#endif //_WIN32

    this->sysTmp = NULL;
    this->nativeMan = params.nativeExecMan;

    // Set the global fileSystem variable.
    fileSystem = this;
}

CFileSystem::~CFileSystem( void )
{
    // Shutdown addon management.
    if ( sysTmp )
    {
        delete sysTmp;

        sysTmp = NULL;
    }

    // Zero the main FileSystem access point.
    fileSystem = NULL;
}

bool CFileSystem::CanLockDirectories( void )
{
    NativeExecutive::CReadWriteWriteContextSafe <> consistency( _fileSysLockProvider.GetReadWriteLock( this ) );

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

template <typename charType>
CFileTranslator* CFileSystemNative::GenCreateTranslator( const charType *path, eDirOpenFlags flags )
{
    // Without access to directory locking, this function can not execute.
    if ( !CanLockDirectories() )
        return NULL;

    // THREAD-SAFE, because this function does not use shared-state variables.

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
        wchar_t pathBuffer[1024];
        _wgetcwd( pathBuffer, NUMELMS(pathBuffer) - 1 );

        pathBuffer[ NUMELMS( pathBuffer ) - 1 ] = 0;

        root += pathBuffer;
        root += "\\";
        root += path;

#ifdef _WIN32
        if (!_File_ParseRelativePath( root.w_str() + 3, tree, bFile ))
            return NULL;

        root.resize( 2 );
        root += "/";
#elif defined(__linux__)
        if (!_File_ParseRelativePath( root.w_str() + 1, tree, bFile ))
            return NULL;

        root = "/";
#endif //OS DEPENDANT CODE
    }

    if ( bFile )
        tree.pop_back();

    _File_OutputPathTree( tree, false, root );

#ifdef _WIN32
    HANDLE dir = _FileWin32_OpenDirectoryHandle( root, flags );

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

CFileTranslator* CFileSystem::CreateTranslator( const char *path, eDirOpenFlags flags )         { return ((CFileSystemNative*)this)->GenCreateTranslator( path, flags ); }
CFileTranslator* CFileSystem::CreateTranslator( const wchar_t *path, eDirOpenFlags flags )      { return ((CFileSystemNative*)this)->GenCreateTranslator( path, flags ); }

CFileTranslator* CFileSystem::GenerateTempRepository( void )
{
    filePath tmpDirBase;

    // Check whether we have a handle to the global temporary system storage.
    // If not, attempt to retrieve it.
    if ( !this->sysTmp )
    {
        NativeExecutive::CReadWriteWriteContext <> consistency( _fileSysTmpDirLockProvider.GetReadWriteLock( this ) );

        if ( !this->sysTmp )
        {
#ifdef _WIN32
            wchar_t buf[2048];

            GetTempPathW( NUMELMS( buf ) - 1, buf );

            buf[ NUMELMS( buf ) - 1 ] = 0;

            // Transform the path into something we can recognize.s
            tmpDirBase.insert( 0, buf, 2 );
            tmpDirBase += L'/';

            dirTree tree;
            bool isFile;
        
            bool parseSuccess = _File_ParseRelativePath( buf + 3, tree, isFile );

            assert( parseSuccess == true && isFile == false );

            _File_OutputPathTree( tree, isFile, tmpDirBase );
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

            this->sysTmp = fileSystem->CreateTranslator( tmpDirBase.w_str() );

            // We failed to get the handle to the temporary storage, hence we cannot deposit temporary files!
            if ( !this->sysTmp )
                return NULL;
        }
    }
    else
    {
        bool success = this->sysTmp->GetFullPath( "@", false, tmpDirBase );

        if ( !success )
            return NULL;
    }

    // Generate a random sub-directory inside of the global OS temp directory.
    // We need to generate until we find a unique directory.
    unsigned int numOfTries = 0;

    filePath tmpDir;

    while ( numOfTries < 50 )
    {
        filePath tmpDir( tmpDirBase );

        tmpDir += "&$!reAr";
        tmpDir += std::to_string( fsrandom::getSystemRandom( this ) );
        tmpDir += "_/";

        if ( this->sysTmp->Exists( tmpDir ) == false )
        {
            // Once we found a not existing directory, we must create and acquire a handle
            // to it. This operation can fail if somebody else happens to delete the directory
            // inbetween or snatched away the handle to the directory before us.
            // Those situations are very unlikely, but we want to make sure anyway, for quality's sake.

            // Make sure the temporary directory exists.
            bool creationSuccessful = _File_CreateDirectory( tmpDir );

            if ( creationSuccessful )
            {
                // Create the temporary root
                CFileTranslator *result = fileSystem->CreateTranslator( tmpDir, DIR_FLAG_EXCLUSIVE );

                if ( result )
                {
                    // Success!
                    return result;
                }
            }
            
            // Well, we failed for some reason, so try again.
        }

        numOfTries++;
    }

    // Nope. Maybe the user wants to try again?
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
    HANDLE file = CreateFileA( path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );

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
