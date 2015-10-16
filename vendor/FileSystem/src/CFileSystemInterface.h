/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystemInterface.h
*  PURPOSE:     File management
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _CFileSystemInterface_
#define _CFileSystemInterface_

#include "CFileSystem.common.h"

#define _FILESYSTEM_WIDEPATH_SUPPORT

// File open flags.
enum eFileOpenFlags : unsigned int
{
    FILE_FLAG_NONE =            0x00000000,
    FILE_FLAG_TEMPORARY =       0x00000001,
    FILE_FLAG_UNBUFFERED =      0x00000002,
    FILE_FLAG_GRIPLOCK =        0x00000004,
    FILE_FLAG_WRITESHARE =      0x00000008
};

enum eDirOpenFlags : unsigned int
{
    DIR_FLAG_NONE =             0x00000000,
    DIR_FLAG_EXCLUSIVE =        0x00000001,
    DIR_FLAG_WRITABLE =         0x00000002
};

/*===================================================
    CFile (stream class)

    This is the access interface to files/streams. You can read,
    write to and obtain information from this. Once destroyed, the
    connection is unlinked. During class life-time, the file may bes locked
    for deletion. Locks depend on the nature of the stream and of
    the OS/environment.
===================================================*/
class CFile abstract
{
public:
    virtual                 ~CFile( void )
    {
    }

    /*===================================================
        CFile::Read

        Arguments:
            buffer - memory location to write data to
            sElement - size of data chunks to be read
            iNumElements - number of data chunks
        Purpose:
            Requests data from the file/stream.
    ===================================================*/
    virtual	size_t          Read( void *buffer, size_t sElement, size_t iNumElements ) = 0;

    /*===================================================
        CFile::Write

        Arguments:
            buffer - memory location to read data from
            sElement - size of data chunks to be written
            iNumElements - number of data chunks
        Purpose:
            Reads data chunks from buffer per sElement stride and
            forwards it to the file/stream.
    ===================================================*/
    virtual	size_t          Write( const void *buffer, size_t sElement, size_t iNumElements ) = 0;

    /*===================================================
        CFile::Seek

        Arguments:
            iOffset - positive or negative value to offset the stream by
            iType - SET_* ANSI enum to specify the procedure
        Purpose:
            Relocates the position of the file/stream. If successful,
            zero is returned. Otherwise, any other value than zero
            is returned.
    ===================================================*/
    virtual	int             Seek( long iOffset, int iType ) = 0;

    /*===================================================
        CFile::SeekNative

        Arguments:
            iOffset - positive or negative value to offset the stream by
            iType - SET_* ANSI enum to specify the procedure
        Purpose:
            Relocates the position of the file/stream. This function
            uses the native number type for maximum file addressing.
            If successful, zero is returned. Otherwise, any other value
            than zero is returned.
    ===================================================*/
    virtual int             SeekNative( fsOffsetNumber_t iOffset, int iType )
    {
        // Overwrite this function to offer actual native functionality.
        // Implementations do not have to support broader access.
        return Seek( (long)iOffset, iType );
    }

    /*===================================================
        CFile::Tell

        Purpose:
            Returns the absolute file/stream location.
    ===================================================*/
    virtual	long            Tell( void ) const = 0;

    /*===================================================
        CFile::TellNative

        Purpose:
            Returns the absolute file/stream location. The return
            value is a native number, so it has maximum file addressing
            range.
    ===================================================*/
    virtual fsOffsetNumber_t    TellNative( void ) const
    {
        // Overwrite this method to offset actual native functionality.
        // Implementations do not have to do that.
        return (fsOffsetNumber_t)Tell();
    }

    /*===================================================
        CFile::IsEOF

        Purpose:
            Returns whether the file/stream has reached it's end. Other
            than the ANSI feof, this is not triggered by reaching over
            the file/stream boundary.
    ===================================================*/
    virtual	bool            IsEOF( void ) const = 0;

    /*===================================================
        CFile::Stat

        Arguments:
            stats - ANSI file information struct
        Purpose:
            Returns true whether it could request information
            about the file/stream. If successful, stats has been
            filled with useful information.
    ===================================================*/
    virtual	bool            Stat( struct stat *stats ) const = 0;

    /*===================================================
        CFile::PushStat

        Arguments:
            stats - ANSI file information struct
        Purpose:
            Updates the file/stream meta information.
    ===================================================*/
    virtual void            PushStat( const struct stat *stats ) = 0;

    /*===================================================
        CFile::SetSeekEnd

        Purpose:
            Sets the file/stream end at the current seek location.
            It effectively cuts off bytes beyond that.
    ===================================================*/
    virtual void            SetSeekEnd( void ) = 0;

    /*===================================================
        CFile::GetSize

        Purpose:
            Returns the total file/stream size if available.
            Otherwise it should return 0.
    ===================================================*/
    virtual	size_t          GetSize( void ) const = 0;

    /*===================================================
        CFile::GetSizeNative

        Purpose:
            Returns the total file/stream size if available.
            Otherwise it should return 0. This function returns
            the size in a native number.
    ===================================================*/
    virtual fsOffsetNumber_t    GetSizeNative( void ) const
    {
        // Overwrite this function to enable actual native support.
        return (fsOffsetNumber_t)GetSize();
    }

    /*===================================================
        CFile::Flush

        Purpose:
            Writes pending file/stream buffers to disk, thus having
            an updated representation in the filesystem to be read
            by different applications.
    ===================================================*/
    virtual	void            Flush( void ) = 0;

    /*===================================================
        CFile::GetPath

        Purpose:
            Returns the unique filesystem location descriptor of this
            file/stream.
    ===================================================*/
    virtual const filePath& GetPath( void ) const = 0;

    /*===================================================
        CFile::IsReadable

        Purpose:
            Returns whether read operations are possible on this
            file/stream. If not, all attempts to request data
            from this are going to fail.
    ===================================================*/
    virtual bool            IsReadable( void ) const = 0;

    /*===================================================
        CFile::IsWriteable

        Purpose:
            Returns whether write operations are possible on this
            file/stream. If not, all attempts to push data into
            this are going to fail.
    ===================================================*/
    virtual bool            IsWriteable( void ) const = 0;

    // Utility definitions, mostly self-explanatory
    // These functions should be used if you want to preserve binary compatibility between systems.
    virtual	bool            ReadInt     ( fsInt_t& out_i )          { return ReadStruct( out_i ); }
    virtual bool            ReadUInt    ( fsUInt_t& out_ui )        { return ReadStruct( out_ui ); }
    virtual	bool            ReadShort   ( fsShort_t& out_s )        { return ReadStruct( out_s ); }
    virtual bool            ReadUShort  ( fsUShort_t& out_us )      { return ReadStruct( out_us ); }
    virtual	bool            ReadByte    ( fsChar_t& out_b )         { return ReadStruct( out_b ); }
    virtual bool            ReadByte    ( fsUChar_t& out_b )        { return ReadStruct( out_b ); }
    virtual bool            ReadWideInt ( fsWideInt_t out_wi )      { return ReadStruct( out_wi ); }
    virtual bool            ReadWideUInt( fsUWideInt_t out_uwi )    { return ReadStruct( out_uwi ); }
    virtual	bool            ReadFloat   ( fsFloat_t& out_f )        { return ReadStruct( out_f ); }
    virtual bool            ReadDouble  ( fsDouble_t& out_d )       { return ReadStruct( out_d ); }
    virtual bool            ReadBool    ( fsBool_t& out_b )         { return ReadStruct( out_b ); }

    virtual	size_t          WriteInt        ( fsInt_t iInt )            { return WriteStruct( iInt ); }
    virtual size_t          WriteUInt       ( fsUInt_t uiInt )          { return WriteStruct( uiInt ); }
    virtual size_t          WriteShort      ( fsShort_t iShort )        { return WriteStruct( iShort ); }
    virtual size_t          WriteUShort     ( fsUShort_t uShort )       { return WriteStruct( uShort ); }
    virtual size_t          WriteByte       ( fsChar_t cByte )          { return WriteStruct( cByte ); }
    virtual size_t          WriteByte       ( fsUChar_t ucByte )        { return WriteStruct( ucByte ); }
    virtual size_t          WriteWideInt    ( fsWideInt_t wInt )        { return WriteStruct( wInt ); }
    virtual size_t          WriteUWideInt   ( fsUWideInt_t uwInt )      { return WriteStruct( uwInt ); }
    virtual size_t          WriteFloat      ( fsFloat_t fFloat )        { return WriteStruct( fFloat ); }
    virtual size_t          WriteDouble     ( fsDouble_t dDouble )      { return WriteStruct( dDouble ); }
    virtual size_t          WriteBool       ( fsBool_t bBool )          { return WriteStruct( bBool ); }

    /*===================================================
        CFile::Printf

        Arguments:
            pFormat - ANSI string implementation pattern.
            ... - VARG of necessary string implementation components.
        Purpose:
            ANSI C style interface which can be used to write
            formatted strings into this file/stream. Returns the
            amount of bytes written.
    ===================================================*/
    virtual	size_t          Printf( const char *pFormat, ... )
    {
        va_list args;
        char cBuffer[1024];

        va_start(args, pFormat);
        _vsnprintf(cBuffer, 1023, pFormat, args);
        va_end(args);

        return Write(cBuffer, 1, strlen(cBuffer));
    }

    size_t                  WriteString( std::string input )
    {
        return Write( input.c_str(), 1, input.size() );
    }

    /*===================================================
        CFile::GetString

        Arguments:
            output - std::string type to write the string to
        Purpose:
            Reads a line from this file/stream. Lines are seperated
            by \n. Returns whether anything could be read.
    ===================================================*/
    bool                    GetString( std::string& output )
    {
        if ( IsEOF() )
            return false;

        do
        {
            char c;

            bool successful = ReadByte( c );

            if ( !successful || !c || c == '\n' )
                break;

            if ( c == '\r' )
            {
                char next_c;

                ReadByte( next_c );

                if ( next_c == '\n' )
                {
                    break;
                }
                else
                {
                    this->Seek( -1, SEEK_CUR );
                }
            }

            output += c;
        }
        while ( !IsEOF() );

        return true;
    }

    /*===================================================
        CFile::GetString

        Arguments:
            buf - memory location to write a C string to
            max - has to be >1; maximum valid range of the
                  memory area pointed to by buf.
        Purpose:
            Same as above, but C-style interface. Automatically
            terminates buf contents by \n if successful.
    ===================================================*/
    bool                    GetString( char *buf, const size_t max )
    {
        size_t n = 0;

        if ( max < 2 || IsEOF() )
            return false;

        do
        {
            char c;

            bool successful = ReadByte( c );

            if ( !successful || !c || c == '\n' )
                goto finish;

            buf[n++] = c;

            if ( n == max - 1 )
                goto finish;
        }
        while ( !IsEOF() );

finish:
        buf[n] = '\0';
        return true;
    }

    template <class type>
    bool    ReadStruct( type& buf )
    {
        return Read( &buf, 1, sizeof( type ) ) == sizeof( type );
    }

    template <class type>
    bool    WriteStruct( type& buf )
    {
        return Write( &buf, 1, sizeof( type ) ) == sizeof( type );
    }
};

typedef void (*pathCallback_t)( const filePath& path, void *userdata );

/*===================================================
    CFileTranslator (directory class)

    A file translator is an access point to filesystems on the local
    filesystem, the network or archives. Before destroying this, all files
    created by this have to be previously destroyed.

    It resides in a root directory and can change it's current directory.
    All these directories are locked for deletion for security reasons.
===================================================*/
class CFileTranslator abstract
{
public:
    virtual                 ~CFileTranslator( void )
    {}

    /*===================================================
        CFileTranslator::CreateDir

        Arguments:
            path - target path for directory creation
        Purpose:
            Attempts to create the directory tree pointed at by
            path. Returns whether the operation was successful.
            It creates all directory along the way, so if path
            is valid, the operation will mos-tlikely succeed.
    ===================================================*/
    virtual bool            CreateDir( const char *path ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            CreateDir( const wchar_t *path ) = 0;
#endif
    AINLINE bool            CreateDir( const filePath& path )
    {
        return filePath_dispatch( path, [&] ( auto path ) { return CreateDir( path ); } );
    }

    /*===================================================
        CFileTranslator::Open

        Arguments:
            path - target path to attempt access to
            mode - ANSI C mode descriptor ("w", "rb+", "a", ...)
        Purpose:
            Attempt to access resources located at path. The access type
            is described by mode. If the operation fails, NULL is returned.
            Failure is either caused due to locks set by the filesystem
            or by an invalid path or invalid mode descriptor.
    ===================================================*/
    virtual CFile*          Open( const char *path, const char *modes, eFileOpenFlags flags = FILE_FLAG_NONE ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CFile*          Open( const wchar_t *path, const wchar_t *mode, eFileOpenFlags flags = FILE_FLAG_NONE ) = 0;
#endif
    AINLINE CFile*          Open( const filePath& path, const filePath& mode, eFileOpenFlags flags = FILE_FLAG_NONE )
    {
        return filePath_dispatch( path,
            [&] ( auto path )
            {
                typedef resolve_type <decltype(path)>::type charType;

                filePathLink <charType> modeLink( mode );

                return Open( path, modeLink.to_char() );
            }
        );
    }

    /*===================================================
        CFileTranslator::Exists

        Arguments:
            path - target path
        Purpose:
            Returns whether the resource at path exists.
    ===================================================*/
    virtual bool            Exists( const char *path ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            Exists( const wchar_t *path ) const = 0;
#endif
    virtual bool            Exists( const filePath& path ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Exists( path ); } );
    }

    /*===================================================
        CFileTranslator::Delete

        Arguments:
            path - target path
        Purpose:
            Attempts to delete the resources located at path. If it is a single
            resource, it is deleted. If it is a directory, all contents are
            recursively deleted and finally the diretory entry itself. If any
            resource fails to be deleted, false is returned.
    ===================================================*/
    virtual bool            Delete( const char *path ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            Delete( const wchar_t *path ) = 0;
#endif
    AINLINE bool            Delete( const filePath& path )
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Delete( path ); } );
    }

    /*===================================================
        CFileTranslator::Copy

        Arguments:
            src - location of the source resource
            dst - location to copy the resource to
        Purpose:
            Creates another copy of the resource pointed at by src
            at the dst location. Returns whether the operation
            was successful.
    ===================================================*/
    virtual bool            Copy( const char *src, const char *dst ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            Copy( const wchar_t *src, const wchar_t *dst ) = 0;
#endif
    AINLINE bool            Copy( const filePath& src, const filePath& dst )
    {
        return filePath_dispatch( src,
            [&] ( auto src )
            {
                typedef typename resolve_type <decltype(src)>::type charType;

                filePathLink <charType> dstLink( dst );

                return Copy( src, dstLink.to_char() );
            }
        );
    }

    /*===================================================
        CFileTranslator::Rename

        Arguments:
            src - location of the source resource
            dst - location to move the resource to
        Purpose:
            Moves the resource pointed to by src to dst location.
            Returns whether the operation was successful.
    ===================================================*/
    virtual bool            Rename( const char *src, const char *dst ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            Rename( const wchar_t *src, const wchar_t *dst ) = 0;
#endif
    AINLINE bool            Rename( const filePath& src, const filePath& dst )
    {
        return filePath_dispatch( src,
            [&] ( auto src )
            {
                typedef typename resolve_type <decltype(src)>::type charType;

                filePathLink <charType> dstLink( dst );

                return Rename( src, dstLink.to_char() );
            }
        );
    }

    /*===================================================
        CFileTranslator::Size

        Arguments:
            path - path of the query resource
        Purpose:
            Returns the size of the resource at path. The result
            is zero if an error occurred.
    ===================================================*/
    virtual size_t          Size( const char *path ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual size_t          Size( const wchar_t *path ) const = 0;
#endif
    AINLINE size_t          Size( const filePath& path ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Size( path ); } );
    }

    /*===================================================
        CFileTranslator::Stat

        Arguments:
            path - path of the query resource
            stats - ANSI C structure for file information
        Purpose:
            Attempts to receive resource meta information at path.
            Returns false if operation failed; then stats remains
            unchanged.
    ===================================================*/
    virtual bool            Stat( const char *path, struct stat *stats ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            Stat( const wchar_t *path, struct stat *stats ) const = 0;
#endif
    AINLINE bool            Stat( const filePath& path, struct stat *stats ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return Stat( path, stats ); } );
    }

    /*==============================================================
        Path Translation functions

        Any path provided to these functions is valid, if it does not
        uproot. It may not leave the translator's root directory.
    ==============================================================*/

    /*===================================================
        CFileTranslator::GetFullPathTreeFromRoot

        Arguments:
            path - target path
            tree - list of path items
            file - whether path points to a file or not
        Purpose:
            Attempts to parse a provided path into a unique absolute path.
            In a Windows OS, the path starts from the root of the drive.
            The path is not constructed, but rather seperated into the list
            pointed to at variable tree. Paths are based against the root
            of this translator.
    ===================================================*/
    virtual bool            GetFullPathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetFullPathTreeFromRoot( const wchar_t *path, dirTree& tree, bool& file ) const = 0;
#endif
    AINLINE bool            GetFullPathTreeFromRoot( const filePath& path, dirTree& tree, bool& file ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathTreeFromRoot( path, tree, file ); } );
    }

    /*===================================================
        CFileTranslator::GetFullPathTree

        Arguments:
            path - target path
            tree - list of path items
            file - whether path points to a file or not
        Purpose:
            Does the same as GetFullPathTreeFromRoot, but the path is based
            against the current directory of the translator.
    ===================================================*/
    virtual bool            GetFullPathTree( const char *path, dirTree& tree, bool& file ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetFullPathTree( const wchar_t *path, dirTree& tree, bool& file ) const = 0;
#endif
    AINLINE bool            GetFullPathTree( const filePath& path, dirTree& tree, bool& file ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathTree( path, tree, file ); } );
    }

    /*===================================================
        CFileTranslator::GetRelativePathTreeFromRoot

        Arguments:
            path - target path
            tree - list of path items
            file - whether path points to a file or not
        Purpose:
            Attempts to parse the provided path to a unique representation
            based on the root directory of the translator. The resulting path
            can be considered a unique representation for this translator.
            The resulting path is split into it's components at the tree list.
    ===================================================*/
    virtual bool            GetRelativePathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetRelativePathTreeFromRoot( const wchar_t *path, dirTree& tree, bool& file ) const = 0;
#endif
    AINLINE bool            GetRelativePathTreeFromRoot( const filePath& path, dirTree& tree, bool& file ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetRelativePathTreeFromRoot( path, tree, file ); } );
    }

    /*===================================================
        CFileTranslator::GetRelativePathTree

        Arguments:
            path - target path
            tree - list of path items
            file - whether path points to a file or not
        Purpose:
            Does the same as GetRelativePathTreeFromRoot, but bases the
            resulting path on the translator's current directory.
    ===================================================*/
    virtual bool            GetRelativePathTree( const char *path, dirTree& tree, bool& file ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetRelativePathTree( const wchar_t *path, dirTree& tree, bool& file ) const = 0;
#endif
    AINLINE bool            GetRelativePathTree( const filePath& path, dirTree& tree, bool& file) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetRelativePathTree( path, tree, file ); } );
    }

    /*===================================================
        CFileTranslator::GetFullPathFromRoot

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetFullPathTreeFromRoot and parses it's output
            into a full (system) path. That is to convert path into a
            full (system) path based on the translator's root.
    ===================================================*/
    virtual bool            GetFullPathFromRoot( const char *path, bool allowFile, filePath& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetFullPathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
#endif
    virtual bool            GetFullPathFromRoot( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&] ( auto path ) { return GetFullPathFromRoot( path, allowFile, output ); } );
    }

    /*===================================================
        CFileTranslator::GetFullPath

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetFullPathTree and parses it's output into a full
            (system) path. This translation is based on the translator's
            current directory.
    ===================================================*/
    virtual bool            GetFullPath( const char *path, bool allowFile, filePath& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetFullPath( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
#endif
    AINLINE bool            GetFullPath( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetFullPath( path, allowFile, output ); } );
    }

    /*===================================================
        CFileTranslator::GetRelativePathFromRoot

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetRelativePathTreeFromRoot and parses it's output
            into a path relative to the translator's root directory.
    ===================================================*/
    virtual bool            GetRelativePathFromRoot( const char *path, bool allowFile, filePath& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetRelativePathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
#endif
    AINLINE bool            GetRelativePathFromRoot( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetRelativePathFromRoot( path, allowFile, output ); } );
    }

    /*===================================================
        CFileTranslator::GetRelativePath

        Arguments:
            path - target path
            allowFile - if false only directory paths are returned
            output - write location for output path
        Purpose:
            Executes GetRelativePathTree and parses it's output
            into a path relative to the translator's current directory.
    ===================================================*/
    virtual bool            GetRelativePath( const char *path, bool allowFile, filePath& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            GetRelativePath( const wchar_t *path, bool allowFile, filePath& output ) const = 0;
#endif
    AINLINE bool            GetRelativePath( const filePath& path, bool allowFile, filePath& output ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetRelativePath( path, allowFile, output ); } );
    }

    /*===================================================
        CFileTranslator::ChangeDirectory

        Arguments:
            path - target path
        Purpose:
            Attempts to change the current directory of the translator.
            Returns whether the operation succeeded.
    ===================================================*/
    virtual bool            ChangeDirectory( const char *path ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool            ChangeDirectory( const wchar_t *path ) = 0;
#endif
    AINLINE bool            ChangeDirectory( const filePath& path )
    {
        return filePath_dispatch( path, [&]( auto path ) { return ChangeDirectory( path ); } );
    }

    /*===================================================
        CFileTranslator::GetDirectory

        Arguments:
            output - structure to save path at
        Purpose:
            Writes the current directory of the translator into output.
    ===================================================*/
    virtual void            GetDirectory( filePath& output ) const = 0;

    /*===================================================
        CFileTranslator::ScanDirectory

        Arguments:
            directory - location of the scan
            wildcard - pattern to check filenames onto
            recurse - if true then sub directories are scanned, too
            dirCallback - executed for every sub directory found
            fileCallback - executed for every file found
            userdata - passed to every callback
        Purpose:
            Scans the designated directory for files and directories.
            The callback is passed the full path of the found resource
            and the userdata.
    ===================================================*/
    virtual void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const = 0;
#endif
    AINLINE void            ScanDirectory( const filePath& directory, const filePath& wildcard, bool recurse,
                                pathCallback_t dirCallback,
                                pathCallback_t fileCallback,
                                void *userdata ) const
    {
        filePath_dispatch( directory,
            [&] ( auto directory )
            {
                typedef typename resolve_type <decltype(directory)>::type charType;

                filePathLink <charType> wildcardLink( wildcard );

                ScanDirectory( directory, wildcardLink.to_char(), recurse, dirCallback, fileCallback, userdata );
            }
        );
    }

    // These functions are easy helpers for ScanDirectory.
    virtual void            GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual void            GetDirectories( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const = 0;
#endif
    AINLINE void            GetDirectories( const filePath& path, const filePath& wildcard, bool recurse, std::vector <filePath>& output ) const
    {
        filePath_dispatch( path,
            [&] ( auto path )
            {
                typedef typename resolve_type <decltype(path)>::type charType;

                filePathLink <charType> wildcardLink( wildcard );

                GetDirectories( path, wildcardLink.to_char(), recurse, output );
            }
        );
    }

    virtual void            GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual void            GetFiles( const wchar_t *path, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const = 0;
#endif
    AINLINE void            GetFiles( const filePath& path, const filePath& wildcard, bool recurse, std::vector <filePath>& output ) const
    {
        filePath_dispatch( path,
            [&] ( auto path )
            {
                typedef typename resolve_type <decltype(path)>::type charType;

                filePathLink <charType> wildcardLink( wildcard );

                GetDirectories( path, wildcardLink.to_char(), recurse, output );
            }
        );
    }
};

#include "CFileSystem.common.stl.h"

/*===================================================
    CArchiveTranslator (archive root class)

    This is a special form of CFileTranslator that is
    an archive root. It manages content to-and-from
    the underlying archive.
===================================================*/
class CArchiveTranslator abstract : public virtual CFileTranslator
{
public:
    virtual void            Save( void ) = 0;
};

// Include public extension headers.
#include "fsinternal/CFileSystem.zip.public.h"
#include "fsinternal/CFileSystem.img.public.h"

class CFileSystemInterface
{
public:
    virtual bool                GetSystemRootDescriptor ( const char *path, filePath& descOut ) const = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual bool                GetSystemRootDescriptor ( const wchar_t *path, filePath& descOut ) const = 0;
#endif
    AINLINE bool                GetSystemRootDescriptor ( const filePath& path, filePath& descOut ) const
    {
        return filePath_dispatch( path, [&]( auto path ) { return GetSystemRootDescriptor( path, descOut ); } );
    }

    virtual CFileTranslator*    CreateTranslator        ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CFileTranslator*    CreateTranslator        ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
#endif
    AINLINE CFileTranslator*    CreateTranslator        ( const filePath& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return filePath_dispatch( path, [&]( auto path ) { return CreateTranslator( path, flags ); } );
    }

    virtual CFileTranslator*    CreateSystemMinimumAccessPoint  ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CFileTranslator*    CreateSystemMinimumAccessPoint  ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) = 0;
#endif
    AINLINE CFileTranslator*    CreateSystemMinimumAccessPoint  ( const filePath& path, eDirOpenFlags flags = DIR_FLAG_NONE )
    {
        return filePath_dispatch( path, [&]( auto path ) { return CreateSystemMinimumAccessPoint( path, flags ); } );
    }

    virtual CArchiveTranslator* OpenArchive         ( CFile& file ) = 0;

    virtual CArchiveTranslator* OpenZIPArchive      ( CFile& file ) = 0;
    virtual CArchiveTranslator* CreateZIPArchive    ( CFile& file ) = 0;

    // Standard IMG archive functions that should be used.
    virtual CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const char *srcPath ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CIMGArchiveTranslatorHandle* OpenIMGArchive         ( CFileTranslator *srcRoot, const wchar_t *srcPath ) = 0;
#endif
    AINLINE CIMGArchiveTranslatorHandle* OpenIMGArchive( CFileTranslator *srcRoot, const filePath& srcPath )
    {
        return filePath_dispatch( srcPath, [&] ( auto srcPath ) { return OpenIMGArchive( srcRoot, srcPath ); } );
    }

    virtual CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CIMGArchiveTranslatorHandle* CreateIMGArchive       ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version ) = 0;
#endif
    AINLINE CIMGArchiveTranslatorHandle* CreateIMGArchive( CFileTranslator *srcRoot, const filePath& srcPath, eIMGArchiveVersion version )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return CreateIMGArchive( srcRoot, srcPath, version ); } );
    }

    // Special functions for IMG archives that should support compression.
    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const char *srcPath ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive        ( CFileTranslator *srcRoot, const wchar_t *srcPath ) = 0;
#endif
    AINLINE CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive( CFileTranslator *srcRoot, const filePath& srcPath )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return OpenCompressedIMGArchive( srcRoot, srcPath ); } );
    }

    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version ) = 0;
#ifdef _FILESYSTEM_WIDEPATH_SUPPORT
    virtual CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive      ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version ) = 0;
#endif
    AINLINE CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive( CFileTranslator *srcRoot, const filePath& srcPath, eIMGArchiveVersion version )
    {
        return filePath_dispatch( srcPath, [&]( auto srcPath ) { return CreateCompressedIMGArchive( srcRoot, srcPath, version ); } );
    }

    // Insecure, use with caution!
    virtual bool                IsDirectory         ( const char *path ) = 0;
    virtual bool                Exists              ( const char *path ) = 0;
    virtual size_t              Size                ( const char *path ) = 0;
    virtual bool                ReadToBuffer        ( const char *path, std::vector <char>& output ) = 0;

    // Settings.
    virtual void                SetIncludeAllDirectoriesInScan  ( bool enable ) = 0;
    virtual bool                GetIncludeAllDirectoriesInScan  ( void ) const = 0;
};

namespace FileSystem
{
    // These functions are not for noobs.

    // Reads the file and gives possible patterns to a callback interface.
    // The interface may break the scan through the file and specify the location
    // where the seek should reside at. This function is used by the .zip extension
    // to find where the .zip stream starts at.
    template <class t, typename F>
    inline bool MappedReaderReverse( CFile& file, F& f )
    {
        t buf;
        long off;

        file.Seek( -(long)sizeof( buf ), SEEK_END );

        do
        {
            size_t readCount = file.Read( &buf, 1, sizeof( buf ) );

            if ( f.Perform( buf, readCount, off ) )
            {
                file.Seek( -(long)readCount + off, SEEK_CUR );
                return true;
            }

        } while ( file.Seek( -(long)sizeof( buf ) * 2 + (long)sizeof( buf ) / 2, SEEK_CUR ) == 0 );

        return false;
    }

    // Memory friendly file copy function.
    inline void StreamCopy( CFile& src, CFile& dst )
    {
        char buf[8096];

#ifdef FILESYSTEM_STREAM_PARANOIA
        // Check for nasty implementation bugs.
        size_t actualFileSize = src.GetSize() - src.Tell();
        size_t addedFileSize = 0;
#endif //FILESYSTEM_STREAM_PARANOIA

        while ( !src.IsEOF() )
        {
            size_t rb = src.Read( buf, 1, sizeof( buf ) );
            size_t writtenBytes = dst.Write( buf, 1, rb );

#ifdef FILESYSTEM_STREAM_PARANOIA
            assert( rb == writtenBytes );

            addedFileSize += rb;
#endif //FILESYSTEM_STREAM_PARANOIA
        }

#ifdef FILESYSTEM_STREAM_PARANOIA
        if ( actualFileSize != addedFileSize )
        {
            __asm int 3
        }

        assert( actualFileSize == addedFileSize );
#endif //FILESYSTEM_STREAM_PARANOIA
    }

    // Memory friendly file copy function which only copies 'cnt' bytes
    // from src to dst.
    inline void StreamCopyCount( CFile& src, CFile& dst, size_t cnt )
    {
        size_t toRead;
        char buf[8096];

        while ( ( toRead = std::min( sizeof( buf ), cnt ) ) != 0 )
        {
            size_t rb = src.Read( buf, 1, toRead );

            cnt -= rb;

            dst.Write( buf, 1, rb );
        }
    }

    // Function which is used to parse a source stream into
    // an appropriate dst representation. It reads the src stream
    // into a temporary buffer and the callback structure may modify it.
    template <class cb>
    inline void StreamParser( CFile& src, CFile& dst, cb& f )
    {
        char buf[8096];
        char outBuf[16192];
        size_t outSize;

        for (;;)
        {
            size_t rb = src.Read( buf, 1, sizeof( buf ) );

            bool eof = src.IsEOF();
            f.prepare( buf, rb, eof );

            for (;;)
            {
                bool cnt = f.parse( outBuf, sizeof( outBuf ), outSize );
                dst.Write( outBuf, 1, outSize );

                if ( !cnt )
                    break;
            }

            if ( eof )
                break;
        }

        dst.SetSeekEnd();
    }

    // Parses the stream same as StreamParser, but limited to 'cnt' bytes of the
    // source stream.
    template <typename cb>
    inline void StreamParserCount( CFile& src, CFile& dst, size_t cnt, cb& f )
    {
        char buf[8096];
        char outBuf[16192];
        size_t outSize;
        size_t toRead;

        for (;;)
        {
            bool eof;

            if ( sizeof( buf ) >= cnt )
            {
                eof = true;

                toRead = cnt;
            }
            else
            {
                eof = false;

                cnt -= toRead = sizeof( buf );
            }

            size_t rb = src.Read( buf, 1, toRead );

            f.prepare( buf, rb, eof );

            for (;;)
            {
                bool continu = f.parse( outBuf, sizeof( outBuf ), outSize );
                dst.Write( outBuf, 1, outSize );

                if ( !continu )
                    break;
            }

            if ( eof )
                break;
        }

        dst.SetSeekEnd();
    }

    // Copies from one translator to another using optimizations.
    inline bool FileCopy( CFileTranslator *srcTranslator, const char *srcPath, CFileTranslator *dstTranslator, const char *dstPath )
    {
        if ( srcTranslator == dstTranslator )
        {
            return srcTranslator->Copy( srcPath, dstPath );
        }

        CFile *srcStream = srcTranslator->Open( srcPath, "rb" );

        bool successful = false;

        if ( srcStream )
        {
            CFile *dstStream = dstTranslator->Open( dstPath, "wb" );

            if ( dstStream )
            {
                StreamCopy( *srcStream, *dstStream );

                successful = true;

                delete dstStream;
            }

            delete srcStream;
        }

        return successful;
    }

    template <typename charType>
    AINLINE const charType* GetFileNameItemStart( const charType *name )
    {
        typedef character_env <charType> char_env;

        const charType *origName = name;

        char_env::const_iterator nameIter( name );

        const charType *fileStartFrom = NULL;

        while ( true )
        {
            char_env::ucp_t ichr = nameIter.Resolve();

            if ( ichr == '\0' )
            {
                if ( !fileStartFrom )
                {
                    fileStartFrom = origName;
                }

                break;
            }

            nameIter.Increment();

            if ( ichr == '\\' || ichr == '/' )
            {
                fileStartFrom = nameIter.GetPointer();
            }
        }

        return fileStartFrom;
    }

    template <typename charType>
    AINLINE const charType* GetFileNameItemEndWithExtension( const charType *name, const charType*& extOut )
    {
        typedef character_env <charType> char_env;
        
        char_env::const_iterator nameIter( name );

        const charType *strEnd = NULL;
        const charType *extStart = NULL;

        while ( true )
        {
            char_env::ucp_t ichr = nameIter.Resolve();

            if ( ichr == '\0' )
            {
                strEnd = nameIter.GetPointer();
                break;
            }

            nameIter.Increment();

            if ( ichr == '.' )
            {
                extStart = nameIter.GetPointer();
            }
        }

        extOut = extStart;
        return strEnd;
    }

    // Useful utility to get the file name out of a path.
    template <typename charType>
    inline filePath GetFileNameItem( const charType *name, bool includeExtension = false, filePath *outDirectory = NULL, filePath *outExtention = NULL )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = NULL;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        // Dispatch the results.
        const charType *fileEnd = NULL;

        if ( !includeExtension && extStart != NULL )
        {
            fileEnd = extStart - 1;
        }
        else
        {
            fileEnd = strEnd;
        }

        // Grab the extension if required.
        if ( outExtention && extStart != NULL )
        {
            *outExtention = filePath( extStart, strEnd - extStart );
        }

        if ( outDirectory )
        {
            // Only create directory path if it is applicable.
            if ( name != fileStartFrom )
            {
                *outDirectory = filePath( name, fileStartFrom - name );
            }
            else
            {
                outDirectory->clear();
            }
        }

        return filePath( fileStartFrom, fileEnd - fileStartFrom );
    }

    AINLINE filePath GetFileNameItem( const filePath& name, bool includeExtension = false, filePath *outDirectory = NULL, filePath *outExtention = NULL )
    {
        return filePath_dispatch( name, [&]( auto name ) { return GetFileNameItem( name, includeExtension, outDirectory, outExtention ); } );
    }

    // Useful function to get just the extension of a filename.
    template <typename charType>
    inline const charType* FindFileNameExtension( const charType *name )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = NULL;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        return extStart;
    }

    // Useful function to get just the directory of a filename, if available.
    template <typename charType>
    inline bool GetFileNameDirectory( const charType *name, filePath& dirOut )
    {
        const charType *fileStartFrom = GetFileNameItemStart( name );

        const charType *extStart = NULL;
        const charType *strEnd = GetFileNameItemEndWithExtension( fileStartFrom, extStart );

        // Check if we have no directory.
        if ( name == fileStartFrom )
            return false;

        dirOut = filePath( name, fileStartFrom - name );
        return true;
    }

    AINLINE bool GetFileNameDirectory( const filePath& name, filePath& dirOut )
    {
        return filePath_dispatch( name, [&]( auto name ) { return GetFileNameDirectory( name, dirOut ); } );
    }

    // Returns whether a path is a directory.
    inline bool IsPathDirectory( const filePath& thePath )
    {
        size_t pathSize = thePath.size();

        if ( pathSize == 0 )
            return true;

        if ( thePath.compareCharAt( '/', pathSize - 1 ) ||
             thePath.compareCharAt( '\\', pathSize - 1 ) )
        {
            return true;
        }

        return false;
    }
}

#endif //_CFileSystemInterface_
