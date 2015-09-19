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

// Include native utilities for platforms.
#include "CFileSystem.internal.nativeimpl.hxx"

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
    DWORD dwAttributes = GetFileAttributesA(pPath);

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

bool File_IsDirectoryAbsoluteW( const wchar_t *pPath )
{
#ifdef _WIN32
    DWORD dwAttributes = GetFileAttributesW(pPath);

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

template <typename charType>
bool CSystemFileTranslator::GenCreateDir( const charType *path )
{
    dirTree tree;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return false;

    if ( file )
        tree.pop_back();

    return _CreateDirTree( tree );
}

bool CSystemFileTranslator::CreateDir( const char *path )       { return GenCreateDir( path ); }
bool CSystemFileTranslator::CreateDir( const wchar_t *path )    { return GenCreateDir( path ); }

template <typename charType>
CFile* CSystemFileTranslator::GenOpenEx( const charType *path, const charType *mode, unsigned int flags )
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

    HANDLE sysHandle = INVALID_HANDLE_VALUE;

    if ( const char *sysPath = output.c_str() )
    {
        sysHandle = CreateFileA( sysPath, dwAccess, (flags & FILE_FLAG_WRITESHARE) != 0 ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ, NULL, dwCreate, flagAttr, NULL );
    }
    else if ( const wchar_t *sysPath = output.w_str() )
    {
        sysHandle = CreateFileW( sysPath, dwAccess, (flags & FILE_FLAG_WRITESHARE) != 0 ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ, NULL, dwCreate, flagAttr, NULL );
    }

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

CFile* CSystemFileTranslator::OpenEx( const char *path, const char *mode, unsigned int flags )          { return GenOpenEx( path, mode, flags ); }
CFile* CSystemFileTranslator::OpenEx( const wchar_t *path, const wchar_t *mode, unsigned int flags )    { return GenOpenEx( path, mode, flags ); }

template <typename charType>
CFile* CSystemFileTranslator::GenOpen( const charType *path, const charType *mode )
{
    return OpenEx( path, mode, 0 );
}

CFile* CSystemFileTranslator::Open( const char *path, const char *mode )        { return GenOpen( path, mode ); }
CFile* CSystemFileTranslator::Open( const wchar_t *path, const wchar_t *mode )  { return GenOpen( path, mode ); }

inline bool _File_Stat( const filePath& path, struct stat& statOut )
{
    int iStat = -1;

    if ( const char *sysPath = path.c_str() )
    {
        iStat = stat( sysPath, &statOut );
    }
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        struct _stat tmp;

        iStat = _wstat( sysPath, &tmp );

        if ( iStat == 0 )
        {
            // Backwards convert.
            statOut.st_dev = tmp.st_dev;
            statOut.st_ino = tmp.st_ino;
            statOut.st_mode = tmp.st_mode;
            statOut.st_nlink = tmp.st_nlink;
            statOut.st_uid = tmp.st_uid;
            statOut.st_gid = tmp.st_gid;
            statOut.st_rdev = tmp.st_rdev;
            statOut.st_size = tmp.st_size;
            statOut.st_atime = (decltype( statOut.st_atime ))tmp.st_atime;
            statOut.st_mtime = (decltype( statOut.st_mtime ))tmp.st_mtime;
            statOut.st_ctime = (decltype( statOut.st_ctime ))tmp.st_ctime;
        }
    }

    return ( iStat == 0 );
}

template <typename charType>
bool CSystemFileTranslator::GenExists( const charType *path ) const
{
    filePath output;

    if ( !GetFullPath( path, true, output ) )
        return false;

    // The C API cannot cope with trailing slashes
    size_t outSize = output.size();

    if ( outSize && output[--outSize] == '/' )
        output.resize( outSize );
    
    struct stat tmp;

    return _File_Stat( output, tmp );
}

bool CSystemFileTranslator::Exists( const char *path ) const        { return GenExists( path ); }
bool CSystemFileTranslator::Exists( const wchar_t *path ) const     { return GenExists( path ); }

inline bool _deleteFile( const char *path )
{
#ifdef _WIN32
    return DeleteFileA( path ) != FALSE;
#elif defined(__linux__)
    return unlink( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

inline bool _deleteFile( const wchar_t *path )
{
#ifdef _WIN32
    return DeleteFileW( path ) != FALSE;
#elif defined(__linux__)
    return unlink( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

inline bool _deleteFileCallback_gen( const filePath& path )
{
    bool deletionSuccess = false;

    if ( const char *sysPath = path.c_str() )
    {
        deletionSuccess = _deleteFile( sysPath );
    }
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        deletionSuccess = _deleteFile( sysPath );
    }

    return deletionSuccess;
}

static void _deleteFileCallback( const filePath& path, void *ud )
{
    bool deletionSuccess = _deleteFileCallback_gen( path );

    if ( !deletionSuccess )
    {
        __debugbreak();
    }
}

inline bool _deleteDir( const char *path )
{
#ifdef _WIN32
    return RemoveDirectoryA( path ) != FALSE;
#elif defined(__linux__)
    return rmdir( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

inline bool _deleteDir( const wchar_t *path )
{
#ifdef _WIN32
    return RemoveDirectoryW( path ) != FALSE;
#elif defined(__linux__)
    return rmdir( path ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

static void _deleteDirCallback( const filePath& path, void *ud );

inline bool _deleteDirCallback_gen( const filePath& path, CSystemFileTranslator *sysRoot )
{
    bool deletionSuccess = false;

    if ( const char *sysPath = path.c_str() )
    {
        sysRoot->ScanDirectory( sysPath, "*", false, _deleteDirCallback, _deleteFileCallback, sysRoot );

        deletionSuccess = _deleteDir( sysPath );
    }
    else if ( const wchar_t *sysPath = path.w_str() )
    {
        sysRoot->ScanDirectory( sysPath, L"*", false, _deleteDirCallback, _deleteFileCallback, sysRoot );

        deletionSuccess = _deleteDir( sysPath );
    }

    return deletionSuccess;
}

static void _deleteDirCallback( const filePath& path, void *ud )
{
    // Delete all subdirectories too.
    CSystemFileTranslator *sysRoot = (CSystemFileTranslator*)ud;

    bool deletionSuccess = _deleteDirCallback_gen( path, sysRoot );

    if ( !deletionSuccess )
    {
        __debugbreak();
    }
}

template <typename charType>
bool CSystemFileTranslator::GenDelete( const charType *path )
{
    filePath output;

    if ( !GetFullPath( path, true, output ) )
        return false;

    if ( FileSystem::IsPathDirectory( output ) )
    {
        bool isDirectory = false;

        if ( const char *sysPath = output.c_str() )
        {
            isDirectory = File_IsDirectoryAbsolute( sysPath );
        }
        else if ( const wchar_t *sysPath = output.w_str() )
        {
            isDirectory = File_IsDirectoryAbsoluteW( sysPath );
        }

        if ( !isDirectory )
            return false;

        // Remove all files and directories inside
        return _deleteDirCallback_gen( output, this );
    }

    return _deleteFileCallback_gen( output );
}

bool CSystemFileTranslator::Delete( const char *path )      { return GenDelete( path ); }
bool CSystemFileTranslator::Delete( const wchar_t *path )   { return GenDelete( path ); }

inline bool _File_Copy( const char *src, const char *dst )
{
#ifdef _WIN32
    return CopyFileA( src, dst, false ) != FALSE;
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

inline bool _File_Copy( const wchar_t *src, const wchar_t *dst )
{
#ifdef _WIN32
    return CopyFileW( src, dst, false ) != FALSE;
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

template <typename charType>
bool CSystemFileTranslator::GenCopy( const charType *src, const charType *dst )
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
    if ( const wchar_t *sysSrcPath = source.w_str() )
    {
        target.convert_unicode();

        return _File_Copy( sysSrcPath, target.w_str() );
    }
    else if ( const wchar_t *sysDstPath = target.w_str() )
    {
        source.convert_unicode();

        return _File_Copy( source.w_str(), sysDstPath );
    }

    return _File_Copy( source.c_str(), target.c_str() );
}

bool CSystemFileTranslator::Copy( const char *src, const char *dst )        { return GenCopy( src, dst ); }
bool CSystemFileTranslator::Copy( const wchar_t *src, const wchar_t *dst )  { return GenCopy( src, dst ); }

inline bool _File_Rename( const char *src, const char *dst )
{
#ifdef _WIN32
    return MoveFileA( src, dst ) != FALSE;
#elif defined(__linux__)
    return rename( src, dst ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

inline bool _File_Rename( const wchar_t *src, const wchar_t *dst )
{
#ifdef _WIN32
    return MoveFileW( src, dst ) != FALSE;
#elif defined(__linux__)
    return rename( src, dst ) == 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

template <typename charType>
bool CSystemFileTranslator::GenRename( const charType *src, const charType *dst )
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

    if ( const wchar_t *sysSrcPath = source.w_str() )
    {
        target.convert_unicode();

        return _File_Rename( sysSrcPath, target.w_str() );
    }
    else if ( const wchar_t *sysDstPath = target.w_str() )
    {
        source.convert_unicode();

        return _File_Rename( source.w_str(), sysDstPath );
    }

    return _File_Rename( source.c_str(), target.c_str() );
}
    
bool CSystemFileTranslator::Rename( const char *src, const char *dst )          { return GenRename( src, dst ); }
bool CSystemFileTranslator::Rename( const wchar_t *src, const wchar_t *dst )    { return GenRename( src, dst ); }

template <typename charType>
bool CSystemFileTranslator::GenStat( const charType *path, struct stat *stats ) const
{
    filePath output;

    if ( !GetFullPath( path, true, output ) )
        return false;

    return _File_Stat( output, *stats );
}

bool CSystemFileTranslator::Stat( const char *path, struct stat *stats ) const      { return GenStat( path, stats ); }
bool CSystemFileTranslator::Stat( const wchar_t *path, struct stat *stats ) const   { return GenStat( path, stats ); }

template <typename charType>
size_t CSystemFileTranslator::GenSize( const charType *path ) const
{
    struct stat fstats;

    if ( !Stat( path, &fstats ) )
        return 0;

    return fstats.st_size;
}

size_t CSystemFileTranslator::Size( const char *path ) const      { return GenSize( path ); }
size_t CSystemFileTranslator::Size( const wchar_t *path ) const   { return GenSize( path ); }

// Handle absolute paths.

template <typename charType>
bool CSystemFileTranslator::GenGetRelativePathTreeFromRoot( const charType *path, dirTree& tree, bool& file ) const
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

bool CSystemFileTranslator::GetRelativePathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const        { return GenGetRelativePathTreeFromRoot( path, tree, file ); }
bool CSystemFileTranslator::GetRelativePathTreeFromRoot( const wchar_t *path, dirTree& tree, bool& file ) const     { return GenGetRelativePathTreeFromRoot( path, tree, file ); }

template <typename charType>
bool CSystemFileTranslator::GenGetRelativePathTree( const charType *path, dirTree& tree, bool& file ) const
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

bool CSystemFileTranslator::GetRelativePathTree( const char *path, dirTree& tree, bool& file ) const        { return GenGetRelativePathTree( path, tree, file ); }
bool CSystemFileTranslator::GetRelativePathTree( const wchar_t *path, dirTree& tree, bool& file ) const     { return GenGetRelativePathTree( path, tree, file ); }

template <typename charType>
bool CSystemFileTranslator::GenGetFullPathTree( const charType *path, dirTree& tree, bool& file ) const
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

bool CSystemFileTranslator::GetFullPathTree( const char *path, dirTree& tree, bool& file ) const    { return GenGetFullPathTree( path, tree, file ); }
bool CSystemFileTranslator::GetFullPathTree( const wchar_t *path, dirTree& tree, bool& file ) const { return GenGetFullPathTree( path, tree, file ); }

template <typename charType>
bool CSystemFileTranslator::GenGetFullPath( const charType *path, bool allowFile, filePath& output ) const
{
    if ( !CSystemPathTranslator::GetFullPath( path, allowFile, output ) )
        return false;

#ifdef _WIN32
    output.insert( 0, m_root, 3 );
#else
    output.insert( 0, "/", 1 );
#endif //_WIN32
    return true;
}

bool CSystemFileTranslator::GetFullPath( const char *path, bool allowFile, filePath& output ) const     { return GenGetFullPath( path, allowFile, output ); }
bool CSystemFileTranslator::GetFullPath( const wchar_t *path, bool allowFile, filePath& output ) const  { return GenGetFullPath( path, allowFile, output ); }

template <typename charType>
bool CSystemFileTranslator::GenChangeDirectory( const charType *path )
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
    HANDLE dir = _FileWin32_OpenDirectoryHandle( absPath );

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

bool CSystemFileTranslator::ChangeDirectory( const char *path )     { return GenChangeDirectory( path ); }
bool CSystemFileTranslator::ChangeDirectory( const wchar_t *path )  { return GenChangeDirectory( path ); }

template <typename charType>
inline const charType* GetAnyWildcardSelector( void )
{
    static_assert( "invalid character type" );
}

template <>
inline const char* GetAnyWildcardSelector <char> ( void )
{
    return "*";
}

template <>
inline const wchar_t* GetAnyWildcardSelector <wchar_t> ( void )
{
    return L"*";
}

template <typename charType>
inline void copystr( charType *dst, const charType *src, size_t max )
{
    static_assert( false, "invalid string type for copy" );
}

template <>
inline void copystr( char *dst, const char *src, size_t max )
{
    strncpy( dst, src, max );
}

template <>
inline void copystr( wchar_t *dst, const wchar_t *src, size_t max )
{
    wcsncpy( dst, src, max );
}

template <typename charType>
struct FINDDATA_ENV
{
    typedef WIN32_FIND_DATA cont_type;

    inline static HANDLE FindFirst( const filePath& path, cont_type *out )
    {
        return FindFirstFile( path.c_str(), out );
    }

    inline static BOOL FindNext( HANDLE hfind, cont_type *out )
    {
        return FindNextFile( hfind, out );
    }
};

template <>
struct FINDDATA_ENV <char>
{
    typedef WIN32_FIND_DATAA cont_type;

    inline static HANDLE FindFirst( const filePath& path, cont_type *out )
    {
        if ( const char *sysPath = path.c_str() )
        {
            return FindFirstFileA( sysPath, out );
        }

        std::string ansiPath = path.convert_ansi();

        return FindFirstFileA( ansiPath.c_str(), out );
    }

    inline static BOOL FindNext( HANDLE hfind, cont_type *out )
    {
        return FindNextFileA( hfind, out );
    }
};

template <>
struct FINDDATA_ENV <wchar_t>
{
    typedef WIN32_FIND_DATAW cont_type;

    inline static HANDLE FindFirst( const filePath& path, cont_type *out )
    {
        if ( const wchar_t *sysPath = path.w_str() )
        {
            return FindFirstFileW( sysPath, out );
        }

        std::wstring widePath = path.convert_unicode();

        return FindFirstFileW( widePath.c_str(), out );
    }

    inline static BOOL FindNext( HANDLE hfind, cont_type *out )
    {
        return FindNextFileW( hfind, out );
    }
};

template <typename charType>
void CSystemFileTranslator::GenScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                                              pathCallback_t dirCallback,
                                              pathCallback_t fileCallback,
                                              void *userdata ) const
{
    filePath            output;
    charType		    wcard[256];

    if ( !GetFullPath( directory, false, output ) )
        return;

    if ( !wildcard )
    {
        wcard[0] = GetAnyWildcardSelector <charType> ()[ 0 ];
        wcard[1] = 0;
    }
    else
    {
        copystr( wcard, wildcard, 255 );
        wcard[255] = 0;
    }

#ifdef _WIN32
    typedef FINDDATA_ENV <charType> find_prov;

    find_prov::cont_type    finddata;
    HANDLE                  handle;

    PathPatternEnv <charType> patternEnv( true );

    PathPatternEnv <charType>::filePattern_t *pattern = patternEnv.CreatePattern( wildcard );

    try
    {
        // Create the query string to send to Windows.
        filePath query = output;
        query += GetAnyWildcardSelector <charType> ();

        //first search for files only
        if ( fileCallback )
        {
            handle = find_prov::FindFirst( query, &finddata );

            if ( handle != INVALID_HANDLE_VALUE )
            {
                do
                {
                    if ( finddata.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_DIRECTORY) )
                        continue;

                    // Match the pattern ourselves.
                    if ( patternEnv.MatchPattern( finddata.cFileName, pattern ) )
                    {
                        filePath filename = output;
                        filename += finddata.cFileName;

                        fileCallback( filename, userdata );
                    }
                } while ( find_prov::FindNext(handle, &finddata) );

                FindClose( handle );
            }
        }

        if ( dirCallback || recurse )
        {
            //next search for subdirectories only
            handle = find_prov::FindFirst( query, &finddata );

            if ( handle != INVALID_HANDLE_VALUE )
            {
                do
                {
                    if ( finddata.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_TEMPORARY) )
                        continue;

                    if ( !(finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
                        continue;

                    // Optimization :)
                    if ( _File_IgnoreDirectoryScanEntry <charType> ( finddata.cFileName ) )
                        continue;

                    filePath target = output;
                    target += finddata.cFileName;
                    target += '/';

                    if ( dirCallback )
                    {
                        _File_OnDirectoryFound( patternEnv, pattern, finddata.cFileName, target, dirCallback, userdata );
                    }

                    if ( recurse )
                    {
                        filePathLink <charType> scanPath( target );

                        ScanDirectory( scanPath.to_char(), wcard, true, dirCallback, fileCallback, userdata );
                    }

                } while ( find_prov::FindNext(handle, &finddata) );

                FindClose( handle );
            }
        }
    }
    catch( ... )
    {
        // Callbacks may throw exceptions
        patternEnv.DestroyPattern( pattern );

        if ( handle != INVALID_HANDLE_VALUE )
        {
            FindClose( handle );
        }
        throw;
    }

    patternEnv.DestroyPattern( pattern );

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

void CSystemFileTranslator::ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                                           pathCallback_t dirCallback,
                                           pathCallback_t fileCallback,
                                           void *userdata ) const
{
    return GenScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CSystemFileTranslator::ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                           pathCallback_t dirCallback,
                                           pathCallback_t fileCallback,
                                           void *userdata ) const
{
    return GenScanDirectory( directory, wildcard, recurse, dirCallback, fileCallback, userdata );
}

static void _scanFindCallback( const filePath& path, std::vector <filePath> *output )
{
    output->push_back( path );
}

template <typename charType>
void CSystemFileTranslator::GenGetDirectories( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, NULL, &output );
}

void CSystemFileTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    return GenGetDirectories( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetDirectories( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    return GenGetDirectories( path, wildcard, recurse, output );
}

template <typename charType>
void CSystemFileTranslator::GenGetFiles( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, NULL, (pathCallback_t)_scanFindCallback, &output );
}

void CSystemFileTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    return GenGetFiles( path, wildcard, recurse, output );
}
void CSystemFileTranslator::GetFiles( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    return GenGetFiles( path, wildcard, recurse, output );
}