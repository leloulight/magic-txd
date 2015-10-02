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
    CFile*          GenOpen                         ( const charType *path, const charType *mode, eFileOpenFlags flags );
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
    bool            CreateDir                       ( const char *path ) override;
    CFile*          Open                            ( const char *path, const char *mode, eFileOpenFlags flags ) override;
    bool            Exists                          ( const char *path ) const override;
    bool            Delete                          ( const char *path ) override;
    bool            Copy                            ( const char *src, const char *dst ) override;
    bool            Rename                          ( const char *src, const char *dst ) override;
    size_t          Size                            ( const char *path ) const override;
    bool            Stat                            ( const char *path, struct stat *stats ) const override;

    bool            CreateDir                       ( const wchar_t *path ) override;
    CFile*          Open                            ( const wchar_t *path, const wchar_t *mode, eFileOpenFlags flags ) override;
    bool            Exists                          ( const wchar_t *path ) const override;
    bool            Delete                          ( const wchar_t *path ) override;
    bool            Copy                            ( const wchar_t *src, const wchar_t *dst ) override;
    bool            Rename                          ( const wchar_t *src, const wchar_t *dst ) override;
    size_t          Size                            ( const wchar_t *path ) const override;
    bool            Stat                            ( const wchar_t *path, struct stat *stats ) const override;

private:
    template <typename charType, typename procType>
    bool            GenProcessFullPath              ( const charType *path, dirTree& tree, bool& file, bool& success, procType proc ) const;
    template <typename charType>
    bool            GenOnGetRelativePathTreeFromRoot( const charType *path, dirTree& tree, bool& file, bool& success ) const;
    template <typename charType>
    bool            GenOnGetRelativePathTree        ( const charType *path, dirTree& tree, bool& file, bool& success ) const;
    template <typename charType>
    bool            GenOnGetFullPathTree            ( const charType *path, dirTree& tree, bool& file, bool& success ) const;

protected:
    // Used to handle absolute paths
    bool            OnGetRelativePathTreeFromRoot   ( const char *path, dirTree& tree, bool& file, bool& success ) const override final;
    bool            OnGetRelativePathTree           ( const char *path, dirTree& tree, bool& file, bool& success ) const override final;
    bool            OnGetFullPathTree               ( const char *path, dirTree& tree, bool& file, bool& success ) const override final;

    bool            OnGetRelativePathTreeFromRoot   ( const wchar_t *path, dirTree& tree, bool& file, bool& success ) const override final;
    bool            OnGetRelativePathTree           ( const wchar_t *path, dirTree& tree, bool& file, bool& success ) const override final;
    bool            OnGetFullPathTree               ( const wchar_t *path, dirTree& tree, bool& file, bool& success ) const override final;

    void            OnGetFullPath                   ( filePath& curAbsPath ) const override final;

protected:
    bool            OnConfirmDirectoryChange        ( const dirTree& path ) override final;

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
                        void *userdata ) const override;
    void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const override;

private:
    template <typename charType>
    void            GenGetDirectories               ( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const;
    template <typename charType>
    void            GenGetFiles                     ( const charType *path, const charType *wildcard, bool recurse, std::vector <filePath>& output ) const;

public:
    void            GetDirectories                  ( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const override;
    void            GetFiles                        ( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const override;

    void            GetDirectories                  ( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const override;
    void            GetFiles                        ( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const override;

    enum eRootPathType
    {
        ROOTPATH_UNKNOWN,
        ROOTPATH_UNC,
        ROOTPATH_DISK
    };

private:
    friend class CFileSystem;
    friend struct CFileSystemNative;

    bool            _CreateDirTree                  ( const dirTree& tree );

#ifdef _WIN32
    eRootPathType   m_pathType;
    filePath        m_unc;          // valid if m_pathType == ROOTPATH_UNC

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

#ifdef _WIN32

template <typename charType>
inline bool _File_IsUNCTerminator( charType c )
{
    return ( c == '\\' || c == '/' );
}

template <typename charType>
inline bool _File_IsUNCPath( const charType *path, const charType*& uncEndOut, filePath& uncOut )
{
    typedef character_env <charType> char_env;

    typename char_env::const_iterator iter( path );

    // We must start with two backslashes.
    if ( _File_IsUNCTerminator( iter.ResolveAndIncrement() ) && _File_IsUNCTerminator( iter.ResolveAndIncrement() ) )
    {
        // Then we need a name of some sort. Every character is allowed.
        bool hasName = false;

        const charType *uncStart = iter.GetPointer();

        bool gotUNCTerminator = false;

        while ( !iter.IsEnd() )
        {
            typename char_env::ucp_t curChar = iter.Resolve();

            if ( _File_IsUNCTerminator( curChar ) )
            {
                gotUNCTerminator = true;
                break;
            }
            
            // We found some name character.
            hasName = true;

            // Since we count this character as UNC, we can advance.
            iter.Increment();
        }

        if ( hasName )
        {
            // Verify that it is properly terminated.
            if ( gotUNCTerminator )
            {
                // We got a valid UNC path. Output the UNC name into us.
                const charType *uncEnd = iter.GetPointer();

                uncOut = filePath( uncStart, ( uncEnd - uncStart ) );

                // Tell the runtime where the UNC path ended.
                iter.Increment();

                uncEndOut = iter.GetPointer();

                // Yay!
                return true;
            }
        }
    }

    return false;
}

#endif

// Important raw system exports.
bool File_IsDirectoryAbsolute( const char *pPath );

#endif //_FILESYSTEM_TRANSLATOR_SYSTEM_