/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.Utils.hxx
*  PURPOSE:     FileSystem common utilities
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSUTILS_
#define _FILESYSUTILS_

// Utility function to create a directory tree vector out of a relative path string.
// Ex.: 'my/path/to/desktop/' -> 'my', 'path', 'to', 'desktop'; file == false
//      'my/path/to/file' -> 'my', 'path', 'to', 'file'; file == true
template <typename charType>
static inline bool _File_ParseRelativePath( const charType *path, dirTree& tree, bool& file )
{
    try
    {
        filePath entry;
        entry.reserve( MAX_PATH );

        typedef character_env <charType> char_env;

        for ( char_env::const_iterator iter( path ); !iter.IsEnd(); iter.Increment() )
        {
            char_env::ucp_t curChar = iter.Resolve();

            switch( curChar )
            {
            case '\\':
            case '/':
                if ( entry.empty() )
                    break;

                if ( entry == "." )
                {
                    // We ignore this current path indicator
                    entry.clear();
                    break;
                }
                else if ( entry == ".." )
                {
                    if ( tree.empty() )
                        return false;

                    tree.pop_back();
                    entry.clear();
                    break;
                }

                tree.push_back( entry );
                entry.clear();
                break;
            case ':':
                return false;
    #ifdef _WIN32
            case '<':
            case '>':
            case '"':
            case '|':
            case '?':
            case '*':
                return false;
    #endif
            default:
                entry += curChar;
                break;
            }
        }

        if ( !entry.empty() )
        {
            tree.push_back( entry );

            file = true;
        }
        else
            file = false;

        return true;
    }
    catch( codepoint_exception& )
    {
        return false;
    }
}

// Output a path tree into a filePath output.
// This is the reverse of _File_ParseRelativePath.
static void AINLINE _File_OutputPathTree( const dirTree& tree, bool file, filePath& output )
{
    size_t actualDirEntries = tree.size();

    if ( file )
    {
        if ( tree.empty() )
            return;

        actualDirEntries--;
    }

    for ( size_t n = 0; n < actualDirEntries; n++ )
    {
        output += tree[n];
        output += '/';
    }

    if ( file )
    {
        output += tree.back();
    }
}

// Get relative path tree nodes
template <typename charType>
static inline bool _File_ParseRelativeTree( const charType *path, const dirTree& root, dirTree& output, bool& file )
{
    dirTree tree;

    if (!_File_ParseRelativePath( path, tree, file ))
        return false;

    if ( file )
    {
        if ( tree.size() <= root.size() )
            return false;
    }
    else if ( tree.size() < root.size() )
        return false;

    dirTree::const_iterator rootIter;
    dirTree::iterator treeIter;

    for ( rootIter = root.begin(), treeIter = tree.begin(); rootIter != root.end(); ++rootIter, ++treeIter )
        if ( *rootIter != *treeIter )
            return false;

    output.insert( output.end(), treeIter, tree.end() );
    return true;
}

extern const filePath _dirBack;

static inline bool _File_ParseDeriviation( const dirTree& curTree, dirTree::const_iterator treeIter, dirTree& output, size_t sizeDiff, bool file )
{
    for ( dirTree::const_iterator rootIter( curTree.begin() ); rootIter != curTree.end(); ++rootIter, ++treeIter, sizeDiff-- )
    {
        if ( !sizeDiff )
        {
            // This cannot fail, as we go down to root at maximum
            for ( ; rootIter != curTree.end(); ++rootIter )
                output.push_back( _dirBack );

            if ( file )
                output.push_back( *treeIter );

            return true;
        }

        if ( *rootIter != *treeIter )
        {
            for ( ; rootIter != curTree.end(); ++rootIter )
                output.push_back( _dirBack );

            break;
        }
    }

    return false;
}

static inline bool _File_ParseDeriviateTreeRoot( const dirTree& pathFind, const dirTree& curTree, dirTree& output, bool file )
{
    dirTree::const_iterator treeIter( pathFind.begin() );

    if ( _File_ParseDeriviation( curTree, treeIter, output, file ? pathFind.size() - 1 : pathFind.size(), file ) )
        return true;

    for ( ; treeIter != pathFind.end(); ++treeIter )
        output.push_back( *treeIter );

    return true;
}

template <typename charType>
static inline bool _File_ParseDeriviateTree( const charType *path, const dirTree& curTree, dirTree& output, bool& file )
{
    dirTree tree = curTree;

    if ( !_File_ParseRelativePath( path, tree, file ) )
        return false;

    return _File_ParseDeriviateTreeRoot( tree, curTree, output, file );
}

// Do the same as above, but derive from current directory
template <typename charType>
static inline bool _File_ParseRelativeTreeDeriviate( const charType *path, const dirTree& root, const dirTree& curTree, dirTree& output, bool& file )
{
    dirTree tree;
    size_t sizeDiff;

    if (!_File_ParseRelativePath( path, tree, file ))
        return false;

    if ( file )
    {
        // File should not count as directory
        if ( tree.size() <= root.size() )
            return false;

        sizeDiff = tree.size() - root.size() - 1;
    }
    else
    {
        if ( tree.size() < root.size() )
            return false;

        sizeDiff = tree.size() - root.size();
    }

    dirTree::const_iterator rootIter;
    dirTree::iterator treeIter;

    for ( rootIter = root.begin(), treeIter = tree.begin(); rootIter != root.end(); ++rootIter, ++treeIter )
        if ( *rootIter != *treeIter )
            return false;

    if ( _File_ParseDeriviation( curTree, treeIter, output, sizeDiff, file ) )
        return true;

    output.insert( output.end(), treeIter, tree.end() );
    return true;
}

template <typename charType>
inline bool _File_ParseMode( const CFileTranslator& root, const charType *path, const charType *mode, unsigned int& access, unsigned int& m )
{
    switch( *mode )
    {
    case 'w':
        m = FILE_MODE_CREATE;
        access = FILE_ACCESS_WRITE;
        break;
    case 'r':
        m = FILE_MODE_OPEN;
        access = FILE_ACCESS_READ;
        break;
    case 'a':
        m = root.Exists( path ) ? FILE_MODE_OPEN : FILE_MODE_CREATE;
        access = FILE_ACCESS_WRITE;
        break;
    default:
        return false;
    }

    if ( ( mode[1] == 'b' && mode[2] == '+' ) || mode[1] == '+' )
    {
        access |= FILE_ACCESS_WRITE | FILE_ACCESS_READ;
    }

    return true;
}

// The string must at least have one character length and be zero terminated.
template <typename charType>
inline bool _File_IgnoreDirectoryScanEntry( const charType *name )
{
    return ( ( *name == '.' && ( *(name+1) == '\0' || ( *(name+1) == '.' && *(name+2) == '\0' ) ) ) );
}

template <>
inline bool _File_IgnoreDirectoryScanEntry <char> ( const char *name )
{
    return *(unsigned short*)name == 0x002E || (*(unsigned short*)name == 0x2E2E && *(unsigned char*)(name + 2) == 0x00);
}

/*=================================================
    File Pattern Matching

    Produces filePattern_t small bytecode routines
    to effectively pattern-match many entries.
=================================================*/

template <typename charType>
struct PathPatternEnv
{
    struct filePatternCommand_t
    {
        enum eCommand
        {
            FCMD_STRCMP,
            FCMD_WILDCARD,
            FCMD_REQONE
        };

        eCommand cmd;
    };

private:
    struct filePatternCommandCompare_t : public filePatternCommand_t
    {
        size_t len;
        charType string[1];

        filePatternCommandCompare_t( void )
        {
            cmd = filePatternCommand_t::FCMD_STRCMP;
        }

        void* operator new( size_t size, size_t strlen )
        {
            return new char[(size - 1) + strlen * sizeof(charType)];
        }

        void operator delete( void *ptr, size_t strlen )
        {
            delete (char*)ptr;
        }

        void operator delete( void *ptr )
        {
            delete (char*)ptr;
        }
    };

    struct filePatternCommandWildcard_t : public filePatternCommand_t
    {
        size_t len;
        charType string[1];

        filePatternCommandWildcard_t( void )
        {
            cmd = filePatternCommand_t::FCMD_WILDCARD;
        }

        void* operator new( size_t size, size_t strlen )
        {
            return new char[(size - 1) + strlen * sizeof(charType)];
        }

        void operator delete( void *ptr, size_t strlen )
        {
            delete (char*)ptr;
        }

        void operator delete( void *ptr )
        {
            delete (char*)ptr;
        }
    };

public:
    inline PathPatternEnv( bool caseSensitive )
    {
        this->caseSensitive = caseSensitive;
    }

    struct filePattern_t
    {
        typedef std::vector <filePatternCommand_t*> cmdList_t;
        cmdList_t commands;
    };

    inline filePatternCommand_t* CreatePatternCommand( typename filePatternCommand_t::eCommand cmd, const std::vector <charType>& tokenBuf )
    {
        filePatternCommand_t *outCmd = NULL;
        size_t tokenLen = tokenBuf.size();

        switch( cmd )
        {
        case filePatternCommand_t::FCMD_STRCMP:
            if ( tokenLen != 0 )
            {
                filePatternCommandCompare_t *cmpCmd = new (tokenLen) filePatternCommandCompare_t;
                std::copy( tokenBuf.begin(), tokenBuf.end(), cmpCmd->string );
                cmpCmd->len = tokenLen;

                outCmd = cmpCmd;
            }
            break;
        case filePatternCommand_t::FCMD_WILDCARD:
            {
                filePatternCommandWildcard_t *wildCmd = new (tokenLen) filePatternCommandWildcard_t;

                if ( tokenLen != 0 )
                {
                    std::copy( tokenBuf.begin(), tokenBuf.end(), wildCmd->string );
                }
                wildCmd->len = tokenLen;

                outCmd = wildCmd;
            }
            break;
        case filePatternCommand_t::FCMD_REQONE:
            outCmd = new filePatternCommand_t;
            outCmd->cmd = filePatternCommand_t::FCMD_REQONE;
            break;
        }

        return outCmd;
    }

    inline void AddCommandToPattern( filePattern_t *pattern, typename filePatternCommand_t::eCommand pendCmd, std::vector <charType>& tokenBuf )
    {
        if ( filePatternCommand_t *cmd = CreatePatternCommand( pendCmd, tokenBuf ) )
        {
            pattern->commands.push_back( cmd );
        }

        tokenBuf.clear();
    }

    inline filePattern_t* CreatePattern( const charType *buf )
    {
        std::vector <charType> tokenBuf;
        filePatternCommand_t::eCommand pendCmd = filePatternCommand_t::FCMD_STRCMP; // by default, it is string comparison.
        filePattern_t *pattern = new filePattern_t;

        while ( charType c = *buf )
        {
            buf++;

            if ( c == '*' )
            {
                AddCommandToPattern( pattern, pendCmd, tokenBuf );

                pendCmd = filePatternCommand_t::FCMD_WILDCARD;
            }
            else if ( c == '?' )
            {
                AddCommandToPattern( pattern, pendCmd, tokenBuf );

                // Directly add our command
                AddCommandToPattern( pattern, filePatternCommand_t::FCMD_REQONE, tokenBuf ),

                // default back to string compare.
                pendCmd = filePatternCommand_t::FCMD_STRCMP;
            }
            else
                tokenBuf.push_back( c );
        }

        AddCommandToPattern( pattern, pendCmd, tokenBuf );

        return pattern;
    }

private:
    inline static bool CompareStrings_Count( const charType *primary, const charType *secondary, size_t count, bool case_insensitive )
    {
        return UniversalCompareStrings( primary, count, secondary, count, !case_insensitive );
    }

    inline static const charType* strnstr( const charType *input, size_t input_len, const charType *cookie, size_t cookie_len, size_t& off_find, bool need_case_insensitive )
    {
        if ( input_len < cookie_len )
            return NULL;

        if ( cookie_len == 0 )
        {
            off_find = 0;
            return input;
        }

        size_t scanEnd = input_len - cookie_len;

        size_t n = 0;

        character_env <charType>::const_iterator iter( input );

        while ( n < scanEnd )
        {
            if ( CompareStrings_Count( input + n, cookie, cookie_len, need_case_insensitive ) )
            {
                off_find = n;
                return input + n;
            }

            n += iter.GetIterateCount();

            iter.Increment();
        }

        return NULL;
    }

public:
    inline bool MatchPattern( const charType *input, filePattern_t *pattern ) const
    {
        size_t input_len = std::char_traits <charType>::length( input );

        bool need_case_insensitive = this->caseSensitive;

        for ( filePattern_t::cmdList_t::const_iterator iter = pattern->commands.begin(); iter != pattern->commands.end(); ++iter )
        {
            filePatternCommand_t *genCmd = *iter;

            switch( genCmd->cmd )
            {
            case filePatternCommand_t::FCMD_STRCMP:
                {
                    filePatternCommandCompare_t *cmpCmd = (filePatternCommandCompare_t*)genCmd;
                    size_t len = cmpCmd->len;

                    if ( len > input_len )
                        return false;

                    if ( CompareStrings_Count( input, cmpCmd->string, len, need_case_insensitive ) == false )
                        return false;

                    input_len -= len;
                    input += len;
                }
                break;
            case filePatternCommand_t::FCMD_WILDCARD:
                {
                    filePatternCommandWildcard_t *wildCmd = (filePatternCommandWildcard_t*) genCmd;
                    const charType *nextPos;
                    size_t len = wildCmd->len;
                    size_t off_find;

                    if ( !( nextPos = strnstr( input, input_len, wildCmd->string, len, off_find, need_case_insensitive ) ) )
                        return false;

                    input_len -= ( off_find + len );
                    input = nextPos + len;
                }
                break;
            case filePatternCommand_t::FCMD_REQONE:
                if ( input_len == 0 )
                    return false;

                input_len--;
                input++;
                break;
            }
        }

        return true;
    }

    inline void DestroyPattern( filePattern_t *pattern )
    {
        for ( filePattern_t::cmdList_t::iterator iter = pattern->commands.begin(); iter != pattern->commands.end(); ++iter )
        {
            delete *iter;
        }

        delete pattern;
    }

    bool caseSensitive;
};

typedef PathPatternEnv <char> ANSIPathPatternEnv;
typedef PathPatternEnv <wchar_t> WidePathPatternEnv;

// Function called when any implementation finds a directory.
template <typename charType>
inline void _File_OnDirectoryFound( const PathPatternEnv <charType>& patternEnv, typename PathPatternEnv <charType>::filePattern_t *pattern, const charType *entryName, const filePath& absPath, pathCallback_t dirCallback, void *userdata )
{
    bool allowDir = true;

    if ( !fileSystem->m_includeAllDirsInScan )
    {
        if ( patternEnv.MatchPattern( entryName, pattern ) == false )
        {
            allowDir = false;
        }
    }

    if ( allowDir )
    {
        dirCallback( absPath, userdata );
    }
}

// Function for creating an OS native directory.
inline bool _File_CreateDirectory( const filePath& thePath )
{
#ifdef __linux__
    if ( mkdir( osPath, FILE_ACCESS_FLAG ) == 0 )
        return true;

    switch( errno )
    {
    case EEXIST:
    case 0:
        return true;
    }

    return false;
#elif defined(_WIN32)
    // Make sure a file with that name does not exist.
    DWORD attrib = INVALID_FILE_ATTRIBUTES;

    filePath pathToMaybeFile = thePath;
    pathToMaybeFile.resize( pathToMaybeFile.size() - 1 );

    if ( const char *ansiPath = pathToMaybeFile.c_str() )
    {
        attrib = GetFileAttributesA( ansiPath );
    }
    else if ( const wchar_t *widePath = pathToMaybeFile.w_str() )
    {
        attrib = GetFileAttributesW( widePath );
    }

    if ( attrib != INVALID_FILE_ATTRIBUTES )
    {
        if ( !( attrib & FILE_ATTRIBUTE_DIRECTORY ) )
            return false;
    }

    BOOL dirSuccess = FALSE;

    if ( const char *ansiPath = thePath.c_str() )
    {
        dirSuccess = CreateDirectoryA( ansiPath, NULL );
    }
    else if ( const wchar_t *widePath = thePath.w_str() )
    {
        dirSuccess = CreateDirectoryW( widePath, NULL );
    }

    return ( dirSuccess == TRUE || GetLastError() == ERROR_ALREADY_EXISTS );
#else
    return false;
#endif //OS DEPENDANT CODE
}

#endif //_FILESYSUTILS_
