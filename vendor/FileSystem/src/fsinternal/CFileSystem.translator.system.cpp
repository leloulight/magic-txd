/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.system.cpp
*  PURPOSE:     FileSystem translator that represents directory links
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>
#include <sys/stat.h>

#ifdef __linux__
#include <utime.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#endif //__linux__

// Include the internal definitions.
#include "CFileSystem.internal.h"

// Include common fs utilitites.
#include "../CFileSystem.utils.hxx"

/*===================================================
    File_IsDirectoryAbsolute

    Arguments:
        pPath - Absolute path pointing to an OS filesystem entry.
    Purpose:
        Checks the given path and returns true if it points
        to a directory, false if a file or no entry was found
        at the path.
===================================================*/
bool File_IsDirectoryAbsolute( const char *pPath )
{
#ifdef _WIN32
    DWORD dwAttributes = GetFileAttributes(pPath);

    if (dwAttributes == INVALID_FILE_ATTRIBUTES)
        return false;

    return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(__linux__)
    struct stat dirInfo;

    if ( stat( pPath, &dirInfo ) != 0 )
        return false;

    return ( dirInfo.st_mode & S_IFDIR ) != 0;
#else
    return false;
#endif
}

/*=======================================
    CSystemFileTranslator

    Default file translator
=======================================*/

CSystemFileTranslator::~CSystemFileTranslator( void )
{
#ifdef _WIN32
    if ( m_curDirHandle )
        CloseHandle( m_curDirHandle );

    CloseHandle( m_rootHandle );
#elif defined(__linux__)
    if ( m_curDirHandle )
        closedir( m_curDirHandle );

    closedir( m_rootHandle );
#endif //OS DEPENDANT CODE
}

bool CSystemFileTranslator::WriteData( const char *path, const char *buffer, size_t size )
{
    filePath output = m_root;
    dirTree tree;
    bool isFile;

    if ( !GetRelativePathTreeFromRoot( path, tree, isFile ) || !isFile )
        return false;

    _File_OutputPathTree( tree, true, output );

    // Make sure directory exists
    tree.pop_back();
    bool dirSuccess = _CreateDirTree( tree );

    if ( !dirSuccess )
        return false;

#ifdef _WIN32
    HANDLE file;

    if ( (file = CreateFile( output.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL )) == INVALID_HANDLE_VALUE )
        return false;

    DWORD numWritten;

    WriteFile( file, buffer, (DWORD)size, &numWritten, NULL );

    CloseHandle( file );
    return numWritten == size;
#elif defined(__linux__)
    int fileToken = open( output.c_str(), O_CREAT | O_WRONLY, FILE_ACCESS_FLAG );

    if ( fileToken == -1 )
        return false;

    ssize_t numWritten = write( fileToken, buffer, size );

    close( fileToken );
    return numWritten == size;
#else
    return 0;
#endif //OS DEPENDANT CODE
}

bool CSystemFileTranslator::_CreateDirTree( const dirTree& tree )
{
    dirTree::const_iterator iter;
    filePath path = m_root;

    for ( iter = tree.begin(); iter != tree.end(); ++iter )
    {
        path += *iter;
        path += '/';

        bool success = _File_CreateDirectory( path );

        if ( !success )
            return false;
    }

    return true;
}

bool CSystemFileTranslator::CreateDir( const char *path )
{
    dirTree tree;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return false;

    if ( file )
        tree.pop_back();

    return _CreateDirTree( tree );
}

CFile* CSystemFileTranslator::OpenEx( const char *path, const char *mode, unsigned int flags )
{
    CFile *outFile = NULL;

    dirTree tree;
    filePath output = m_root;
    CRawFile *pFile;
    unsigned int dwAccess = 0;
    unsigned int dwCreate = 0;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return NULL;

    // We can only open files!
    if ( !file )
        return NULL;

    _File_OutputPathTree( tree, true, output );

    if ( !_File_ParseMode( *this, path, mode, dwAccess, dwCreate ) )
        return NULL;

    // Creation requires the dir tree!
    if ( dwCreate == FILE_MODE_CREATE )
    {
        tree.pop_back();

        bool dirSuccess = _CreateDirTree( tree );

        if ( !dirSuccess )
            return NULL;
    }

#ifdef _WIN32
    DWORD flagAttr = 0;

    if ( flags & FILE_FLAG_TEMPORARY )
        flagAttr |= FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY;

    if ( flags & FILE_FLAG_UNBUFFERED )
        flagAttr |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;

    HANDLE sysHandle = CreateFile( output.c_str(), dwAccess, (flags & FILE_FLAG_WRITESHARE) != 0 ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ, NULL, dwCreate, flagAttr, NULL );

    if ( sysHandle == INVALID_HANDLE_VALUE )
        return NULL;

    pFile = new CRawFile( output );
    pFile->m_file = sysHandle;
#elif defined(__linux__)
    const char *openMode;

    // TODO: support flags parameter.

    if ( dwCreate == FILE_MODE_CREATE )
    {
        if ( dwAccess & FILE_ACCESS_READ )
            openMode = "w+";
        else
            openMode = "w";
    }
    else if ( dwCreate == FILE_MODE_OPEN )
    {
        if ( dwAccess & FILE_ACCESS_WRITE )
            openMode = "r+";
        else
            openMode = "r";
    }
    else
        return NULL;

    FILE *filePtr = fopen( output.c_str(), openMode );

    if ( !filePtr )
        return NULL;

    pFile = new CRawFile( output );
    pFile->m_file = filePtr;
#else
    return NULL;
#endif //OS DEPENDANT CODE

    // Write shared file properties.
    pFile->m_access = dwAccess;

    if ( *mode == 'a' )
        pFile->Seek( 0, SEEK_END );

    outFile = pFile;

    // TODO: improve the buffering implementation, so it does not fail in write-only mode.

    // If required, wrap the file into a buffered stream.
    if ( false ) // todo: add a property that decides this?
    {
        outFile = new CBufferedStreamWrap( pFile, true );
    }

    return outFile;
}

CFile* CSystemFileTranslator::Open( const char *path, const char *mode )
{
    return OpenEx( path, mode, 0 );
}

bool CSystemFileTranslator::Exists( const char *path ) const
{
    filePath output;
    struct stat tmp;

    if ( !GetFullPath( path, true, output ) )
        return false;

    // The C API cannot cope with trailing slashes
    size_t outSize = output.size();

    if ( outSize && output[--outSize] == '/' )
        output.resize( outSize );

    return stat( output.c_str(), &tmp ) == 0;
}

inline bool _deleteFile( const char *path )
{
#ifdef _WIN32
    return DeleteFile( path ) != FALSE;
#elif defined(__linux__)
    return unlink( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

static void _deleteFileCallback( const filePath& path, void *ud )
{
    bool deletionSuccess = _deleteFile( path.c_str() );

    if ( !deletionSuccess )
    {
        __debugbreak();
    }
}

inline bool _deleteDir( const char *path )
{
#ifdef _WIN32
    return RemoveDirectory( path ) != FALSE;
#elif defined(__linux__)
    return rmdir( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

static void _deleteDirCallback( const filePath& path, void *ud )
{
    // Delete all subdirectories too.
    ((CSystemFileTranslator*)ud)->ScanDirectory( path, "*", false, _deleteDirCallback, _deleteFileCallback, ud );

    bool deletionSuccess = _deleteDir( path.c_str() );

    if ( !deletionSuccess )
    {
        __debugbreak();
    }
}

bool CSystemFileTranslator::Delete( const char *path )
{
    filePath output;

    if ( !GetFullPath( path, true, output ) )
        return false;

    if ( FileSystem::IsPathDirectory( output ) )
    {
        if ( !File_IsDirectoryAbsolute( output.c_str() ) )
            return false;

        // Remove all files and directories inside
        ScanDirectory( output.c_str(), "*", false, _deleteDirCallback, _deleteFileCallback, this );
        return _deleteDir( output.c_str() );
    }

    return _deleteFile( output.c_str() );
}

inline bool _File_Copy( const char *src, const char *dst )
{
#ifdef _WIN32
    return CopyFile( src, dst, false ) != FALSE;
#elif defined(__linux__)
    int iReadFile = open( src, O_RDONLY, 0 );

    if ( iReadFile == -1 )
        return false;

    int iWriteFile = open( dst, O_CREAT | O_WRONLY | O_ASYNC, FILE_ACCESS_FLAG );

    if ( iWriteFile == -1 )
        return false;

    struct stat read_info;
    if ( fstat( iReadFile, &read_info ) != 0 )
    {
        close( iReadFile );
        close( iWriteFile );
        return false;
    }

    sendfile( iWriteFile, iReadFile, NULL, read_info.st_size );

    close( iReadFile );
    close( iWriteFile );
    return true;
#else
    return false;
#endif //OS DEPENDANT CODE
}

bool CSystemFileTranslator::Copy( const char *src, const char *dst )
{
    filePath source;
    filePath target;
    dirTree dstTree;
    bool file;

    if ( !GetFullPath( src, true, source ) || !GetRelativePathTreeFromRoot( dst, dstTree, file ) || !file )
        return false;

    // We always start from root
    target = m_root;

    _File_OutputPathTree( dstTree, true, target );

    // Make sure dir exists
    dstTree.pop_back();
    bool dirSuccess = _CreateDirTree( dstTree );

    if ( !dirSuccess )
        return false;

    // Copy data using quick kernel calls.
    return _File_Copy( source.c_str(), target.c_str() );
}

bool CSystemFileTranslator::Rename( const char *src, const char *dst )
{
    filePath source;
    filePath target;
    dirTree dstTree;
    bool file;

    if ( !GetFullPath( src, true, source ) || !GetRelativePathTreeFromRoot( dst, dstTree, file ) || !file )
        return false;

    // We always start from root
    target = m_root;

    _File_OutputPathTree( dstTree, true, target );

    // Make sure dir exists
    dstTree.pop_back();
    bool dirSuccess = _CreateDirTree( dstTree );

    if ( !dirSuccess )
        return false;

#ifdef _WIN32
    return MoveFile( source.c_str(), target.c_str() ) != FALSE;
#elif defined(__linux__)
    return rename( source.c_str(), target.c_str() ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

bool CSystemFileTranslator::Stat( const char *path, struct stat *stats ) const
{
    filePath output;

    if ( !GetFullPath( path, true, output ) )
        return false;

    return stat( output.c_str(), stats ) == 0;
}

size_t CSystemFileTranslator::Size( const char *path ) const
{
    struct stat fstats;

    if ( !Stat( path, &fstats ) )
        return 0;

    return fstats.st_size;
}

bool CSystemFileTranslator::ReadToBuffer( const char *path, std::vector <char>& output ) const
{
    filePath sysPath;

    if ( !GetFullPath( path, true, sysPath ) )
        return false;

    return fileSystem->ReadToBuffer( sysPath.c_str(), output );
}

// Handle absolute paths.

bool CSystemFileTranslator::GetRelativePathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const
{
    if ( _File_IsAbsolutePath( path ) )
    {
#ifdef _WIN32
        if ( m_root.compareCharAt( path[0], 0 ) == false )
            return false;   // drive mismatch

        return _File_ParseRelativeTree( path + 3, m_rootTree, tree, file );
#else
        return _File_ParseRelativeTree( path + 1, m_rootTree, tree, file );
#endif //OS DEPENDANT CODE
    }

    return CSystemPathTranslator::GetRelativePathTreeFromRoot( path, tree, file );
}

bool CSystemFileTranslator::GetRelativePathTree( const char *path, dirTree& tree, bool& file ) const
{
    if ( _File_IsAbsolutePath( path ) )
    {
#ifdef _WIN32
        if ( m_root.compareCharAt( path[0], 0 ) == false )
            return false;   // drive mismatch

        return _File_ParseRelativeTreeDeriviate( path + 3, m_rootTree, m_curDirTree, tree, file );
#else
        return _File_ParseRelativeTreeDeriviate( path + 1, m_rootTree, m_curDirTree, tree, file );
#endif //OS DEPENDANT CODE
    }

    return CSystemPathTranslator::GetRelativePathTree( path, tree, file );
}

bool CSystemFileTranslator::GetFullPathTree( const char *path, dirTree& tree, bool& file ) const
{
    if ( _File_IsAbsolutePath( path ) )
    {
#ifdef _WIN32
        if ( m_root.compareCharAt( path[0], 0 ) == false )
            return false;   // drive mismatch

        tree = m_rootTree;
        return _File_ParseRelativeTree( path + 3, m_rootTree, tree, file );
#else
        tree = m_rootTree;
        return _File_ParseRelativeTree( path + 1, m_rootTree, tree, file );
#endif //OS DEPENDANT CODE
    }

    return CSystemPathTranslator::GetFullPathTree( path, tree, file );
}

bool CSystemFileTranslator::GetFullPath( const char *path, bool allowFile, filePath& output ) const
{
    if ( !CSystemPathTranslator::GetFullPath( path, allowFile, output ) )
        return false;

#ifdef _WIN32
    output.insert( 0, m_root.c_str(), 3 );
#else
    output.insert( 0, "/", 1 );
#endif //_WIN32
    return true;
}

bool CSystemFileTranslator::ChangeDirectory( const char *path )
{
    dirTree tree;
    filePath absPath;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return false;

    if ( file )
        tree.pop_back();

    absPath = m_root;
    _File_OutputPathTree( tree, false, absPath );

#ifdef _WIN32
    HANDLE dir = CreateFile( absPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );

    if ( dir == INVALID_HANDLE_VALUE )
        return false;

    if ( m_curDirHandle )
        CloseHandle( m_curDirHandle );

    m_curDirHandle = dir;
#elif defined(__linux__)
    DIR *dir = opendir( absPath.c_str() );

    if ( dir == NULL )
        return false;

    if ( m_curDirHandle )
        closedir( m_curDirHandle );

    m_curDirHandle = dir;
#else
    if ( !File_IsDirectoryAbsolute( absPath.c_str() ) )
        return false;
#endif //OS DEPENDANT CODE

    m_currentDir.clear();
    _File_OutputPathTree( tree, false, m_currentDir );

    m_curDirTree = tree;
    return true;
}

void CSystemFileTranslator::ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                                            pathCallback_t dirCallback,
                                            pathCallback_t fileCallback,
                                            void *userdata ) const
{
    filePath            output;
    char				wcard[256];

    if ( !GetFullPath( directory, false, output ) )
        return;

    if ( !wildcard )
        strcpy(wcard, "*");
    else
        strncpy(wcard, wildcard, 255);

#ifdef _WIN32
    WIN32_FIND_DATA		finddata;
    HANDLE				handle;

    filePattern_t *pattern = _File_CreatePattern( wildcard );

    try
    {
        // Create the query string to send to Windows.
        std::string query = std::string( output.c_str(), output.size() );
        query += "*";

        //first search for files only
        if ( fileCallback )
        {
            handle = FindFirstFile( query.c_str(), &finddata );

            if ( handle != INVALID_HANDLE_VALUE )
            {
                do
                {
                    if ( finddata.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_DIRECTORY) )
                        continue;

                    // Match the pattern ourselves.
                    if ( _File_MatchPattern( finddata.cFileName, pattern ) )
                    {
                        filePath filename = output;
                        filename += finddata.cFileName;

                        fileCallback( filename, userdata );
                    }
                } while ( FindNextFile(handle, &finddata) );

                FindClose( handle );
            }
        }

        if ( dirCallback || recurse )
        {
            //next search for subdirectories only
            handle = FindFirstFile( query.c_str(), &finddata );

            if ( handle != INVALID_HANDLE_VALUE )
            {
                do
                {
                    if ( finddata.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_TEMPORARY) )
                        continue;

                    if ( !(finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
                        continue;

                    // Optimization :)
                    if ( _File_IgnoreDirectoryScanEntry( finddata.cFileName ) )
                        continue;

                    filePath target = output;
                    target += finddata.cFileName;
                    target += '/';

                    if ( dirCallback )
                    {
                        _File_OnDirectoryFound( pattern, finddata.cFileName, target, dirCallback, userdata );
                    }

                    if ( recurse )
                        ScanDirectory( target.c_str(), wcard, true, dirCallback, fileCallback, userdata );

                } while ( FindNextFile(handle, &finddata) );

                FindClose( handle );
            }
        }
    }
    catch( ... )
    {
        // Callbacks may throw exceptions
        _File_DestroyPattern( pattern );

        if ( handle != INVALID_HANDLE_VALUE )
        {
            FindClose( handle );
        }
        throw;
    }

    _File_DestroyPattern( pattern );

#elif defined(__linux__)
    DIR *findDir = opendir( output.c_str() );

    if ( !findDir )
        return;

    filePattern_t *pattern = _File_CreatePattern( wildcard );

    try
    {
        //first search for files only
        if ( fileCallback )
        {
            while ( struct dirent *entry = readdir( findDir ) )
            {
                filePath path = output;
                path += entry->d_name;

                struct stat entry_stats;

                if ( stat( path.c_str(), &entry_stats ) == 0 )
                {
                    if ( !( S_ISDIR( entry_stats.st_mode ) ) && _File_MatchPattern( entry->d_name, pattern ) )
                    {
                        fileCallback( path.c_str(), userdata );
                    }
                }
            }
        }

        rewinddir( findDir );

        if ( dirCallback || recurse )
        {
            //next search for subdirectories only
            while ( struct dirent *entry = readdir( findDir ) )
            {
                const char *name = entry->d_name;

                if ( _File_IgnoreDirectoryScanEntry( name ) )
                    continue;

                filePath path = output;
                path += name;
                path += '/';

                struct stat entry_info;

                if ( stat( path.c_str(), &entry_info ) == 0 && S_ISDIR( entry_info.st_mode ) )
                {
                    if ( dirCallback )
                    {
                        _File_OnDirectoryFound( pattern, entry->d_name, path, dirCallback, userdata );
                    }

                    // TODO: this can be optimized by reusing the pattern structure.
                    if ( recurse )
                        ScanDirectory( path.c_str(), wcard, recurse, dirCallback, fileCallback, userdata );
                }
            }
        }
    }
    catch( ... )
    {
        // Callbacks may throw exceptions
        _File_DestroyPattern( pattern );

        closedir( findDir );
        throw;
    }

    _File_DestroyPattern( pattern );

    closedir( findDir );
#endif //OS DEPENDANT CODE
}

static void _scanFindCallback( const filePath& path, std::vector <filePath> *output )
{
    output->push_back( path );
}

void CSystemFileTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, NULL, &output );
}

void CSystemFileTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, NULL, (pathCallback_t)_scanFindCallback, &output );
}