/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.system.h
*  PURPOSE:     FileSystem translator that represents directory links
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_SYSTEM_
#define _FILESYSTEM_TRANSLATOR_SYSTEM_

class CSystemFileTranslator : public CSystemPathTranslator
{
public:
    // We need to distinguish between Windows and other OSes here.
    // Windows uses driveletters (C:/) and Unix + Mac do not.
    // This way, on Windows we can use the '/' or '\\' character
    // to trace paths from the translator root.
    // On Linux and Mac, these characters are for addressing
    // absolute paths.
    CSystemFileTranslator( void ) :
#ifdef _WIN32
        CSystemPathTranslator( false )
#else
        CSystemPathTranslator( true )
#endif //OS DEPENDANT CODE
    { }

                    ~CSystemFileTranslator          ( void );

private:
    template <typename charType>
    bool            GenCreateDir                    ( const charType *path );
    template <typename charType>
    CFile*          GenOpen                         ( const charType *path, const charType *mode );
    template <typename charType>
    CFile*          GenOpenEx                       ( const charType *path, const charType *mode, unsigned int flags );
    template <typename charType>
    bool            GenExists                       ( const charType *path ) const;
    template <typename charType>
    bool            GenDelete                       ( const charType *path );
    template <typename charType>
    bool            GenCopy                         ( const charType *src, const charType *dst );
    template <typename charType>
    bool            GenRename                       ( const charType *src, const charType *dst );
    template <typename charType>
    size_t          GenSize                         ( const charType *path ) const;
    template <typename charType>
    bool            GenStat                         ( const charType *path, struct stat *stats ) const;

public:
    bool            CreateDir                       ( const char *path );
    CFile*          Open                            ( const char *path, const char *mode );
    CFile*          OpenEx                          ( const char *path, const char *mode, unsigned int flags );
    bool            Exists                          ( const char *path ) const;
    bool            Delete                          ( const char *path );
    bool            Copy                            ( const char *src, const char *dst );
    bool            Rename                          ( const char *src, const char *dst );
    size_t          Size                            ( const char *path ) const;
    bool            Stat                            ( const char *path, struct stat *stats ) const;

    bool            CreateDir                       ( const wchar_t *path );
    CFile*          Open                            ( const wchar_t *path, const wchar_t *mode );
    CFile*          OpenEx                          ( const wchar_t *path, const wchar_t *mode, unsigned int flags );
    bool            Exists                          ( const wchar_t *path ) const;
    bool            Delete                          ( const wchar_t *path );
    bool            Copy                            ( const wchar_t *src, const wchar_t *dst );
    bool            Rename                          ( const wchar_t *src, const wchar_t *dst );
    size_t          Size                            ( const wchar_t *path ) const;
    bool            Stat                            ( const wchar_t *path, struct stat *stats ) const;

private:
    template <typename charType>
    bool            GenGetRelativePathTreeFromRoot  ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetRelativePathTree          ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetFullPathTree              ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetFullPath                  ( const charType *path, bool allowFile, filePath& output ) const;

public:
    // Used to handle absolute paths
    bool            GetRelativePathTreeFromRoot     ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetRelativePathTree             ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetFullPathTree                 ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetFullPath                     ( const char *path, bool allowFile, filePath& output ) const;

    bool            GetRelativePathTreeFromRoot     ( const wchar_t *path, dirTree& tree, bool& file ) const;
    bool            GetRelativePathTree             ( const wchar_t *path, dirTree& tree, bool& file ) const;
    bool            GetFullPathTree                 ( const wchar_t *path, dirTree& tree, bool& file ) const;
    bool            GetFullPath                     ( const wchar_t *path, bool allowFile, filePath& output ) const;

private:
    template <typename charType>
    bool            GenChangeDirectory              ( const charType *path );

public:
    bool            ChangeDirectory                 ( const char *path );
    bool            ChangeDirectory                 ( const wchar_t *path );

private:
    template <typename charType>
    void            GenScanDirectory( const charType *directory, const charType *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const;

public:
    void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const;
    void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const;

private:
    template <typename charType>
    void            GenGetDirectories               ( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const;
    template <typename charType>
    void            GenGetFiles                     ( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const;

public:
    void            GetDirectories                  ( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const;
    void            GetFiles                        ( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const;

    void            GetDirectories                  ( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const;
    void            GetFiles                        ( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const;

private:
    friend class CFileSystem;
    friend struct CFileSystemNative;

    bool            _CreateDirTree                  ( const dirTree& tree );

#ifdef _WIN32
    HANDLE          m_rootHandle;
    HANDLE          m_curDirHandle;
#elif defined(__linux__)
    DIR*            m_rootHandle;
    DIR*            m_curDirHandle;
#endif //OS DEPENDANT CODE
};

// Common utilities.
template <typename charType>
inline bool _File_IsAbsolutePath( const charType *path )
{
    charType character = *path;

#ifdef _WIN32
    switch( character )
    {
    // Drive letters
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        return ( path[1] == ':' && path[2] != 0 );
    }
#elif defined(__linux__)
    switch( character )
    {
    case '/':
    case '\\':
        return true;
    }
#endif //OS DEPENDANT CODE

    return false;
}

// Important raw system exports.
bool File_IsDirectoryAbsolute( const char *pPath );

#endif //_FILESYSTEM_TRANSLATOR_SYSTEM_