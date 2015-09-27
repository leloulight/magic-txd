/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.zip.translator.cpp
*  PURPOSE:     ZIP archive filesystem
*
*  Docu: https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>
#include <zlib.h>
#include <sys/stat.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.zip.internal.h"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"
#include "CFileSystem.zip.utils.hxx"

/*=======================================
    CZIPArchiveTranslator::fileDeflate

    ZIP file seeking and handling
=======================================*/

CZIPArchiveTranslator::fileDeflate::fileDeflate( CZIPArchiveTranslator& zip, file& info, CFile& sysFile ) : stream( zip, info, sysFile )
{
}

CZIPArchiveTranslator::fileDeflate::~fileDeflate( void )
{
}

size_t CZIPArchiveTranslator::fileDeflate::Read( void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !m_readable )
        return 0;

    return m_sysFile.Read( buffer, sElement, iNumElements );
}

size_t CZIPArchiveTranslator::fileDeflate::Write( const void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !m_writeable )
        return 0;

    m_info.metaData.UpdateTime();
    return m_sysFile.Write( buffer, sElement, iNumElements );
}

int CZIPArchiveTranslator::fileDeflate::Seek( long iOffset, int iType )
{
    return m_sysFile.Seek( iOffset, iType );
}

long CZIPArchiveTranslator::fileDeflate::Tell( void ) const
{
    return m_sysFile.Tell();
}

bool CZIPArchiveTranslator::fileDeflate::IsEOF( void ) const
{
    return m_sysFile.IsEOF();
}

bool CZIPArchiveTranslator::fileDeflate::Stat( struct stat *stats ) const
{
    tm date;

    m_info.metaData.GetModTime( date );

    date.tm_year -= 1900;

    stats->st_mtime = stats->st_atime = stats->st_ctime = mktime( &date );
    return true;
}

void CZIPArchiveTranslator::fileDeflate::PushStat( const struct stat *stats )
{
    tm *date = gmtime( &stats->st_mtime );

    m_info.metaData.SetModTime( *date );
}

void CZIPArchiveTranslator::fileDeflate::SetSeekEnd( void )
{
    // TODO.
}

size_t CZIPArchiveTranslator::fileDeflate::GetSize( void ) const
{
    return m_sysFile.GetSize();
}

void CZIPArchiveTranslator::fileDeflate::Flush( void )
{
    m_sysFile.Flush();
}

bool CZIPArchiveTranslator::fileDeflate::IsReadable( void ) const
{
    return m_readable;
}

bool CZIPArchiveTranslator::fileDeflate::IsWriteable( void ) const
{
    return m_writeable;
}

/*=======================================
    CZIPArchiveTranslator

    ZIP translation utility
=======================================*/

CZIPArchiveTranslator::CZIPArchiveTranslator( zipExtension& theExtension, CFile& fileStream ) : CSystemPathTranslator( false ), m_zipExtension( theExtension ), m_file( fileStream ), m_virtualFS( true )
{
    // TODO: Get real .zip structure offset
    m_structOffset = 0;

    // Set up the virtual filesystem.
    m_virtualFS.hostTranslator = this;

    m_fileRoot = NULL;
    m_realtimeRoot = NULL;
    m_unpackRoot = NULL;
}

CZIPArchiveTranslator::~CZIPArchiveTranslator( void )
{
    filePath path;

    bool needDeletion = ( m_fileRoot != NULL );

    if ( m_fileRoot )
    {
        m_fileRoot->GetFullPath( "@", false, path );
    }

    // Destroy the locks to our runtime management folders.
    if ( m_unpackRoot )
    {
        delete m_unpackRoot;

        m_unpackRoot = NULL;
    }
    
    if ( m_realtimeRoot )
    {
        delete m_realtimeRoot;

        m_realtimeRoot = NULL;
    }

    if ( m_fileRoot )
    {
        delete m_fileRoot;

        m_fileRoot = NULL;
    }

    if ( needDeletion )
    {
        // Delete all temporary files by deleting our entire folder structure.
        if ( CFileTranslator *sysTmpRoot = m_zipExtension.GetTempRoot() )
        {
            sysTmpRoot->Delete( path );
        }
    }
}

CFileTranslator* CZIPArchiveTranslator::GetFileRoot( void )
{
    if ( !m_fileRoot )
    {
        m_fileRoot = m_zipExtension.repo.GenerateUniqueDirectory();
    }
    return m_fileRoot;
}

CFileTranslator* CZIPArchiveTranslator::GetUnpackRoot( void )
{
    if ( !m_unpackRoot )
    {
        CFileTranslator *fileRoot = GetFileRoot();

        if ( fileRoot )
        {
            m_unpackRoot = CRepository::AcquireDirectoryTranslator( fileRoot, "unpack/" );
        }
    }
    return m_unpackRoot;
}

CFileTranslator* CZIPArchiveTranslator::GetRealtimeRoot( void )
{
    if ( !m_realtimeRoot )
    {
        CFileTranslator *fileRoot = GetFileRoot();

        if ( fileRoot )
        {
            m_realtimeRoot = CRepository::AcquireDirectoryTranslator( fileRoot, "realtime/" );
        }
    }
    return m_realtimeRoot;
}

bool CZIPArchiveTranslator::CreateDir( const char *path )
{
    return m_virtualFS.CreateDir( path );
}

CFile* CZIPArchiveTranslator::OpenNativeFileStream( file *fsObject, unsigned int openMode, unsigned int access )
{
    CFile *dstFile = NULL;

    if ( openMode == FILE_MODE_OPEN )
    {
        const filePath& relPath = fsObject->relPath;

        // Attempt to get a handle to the realtime root.
        CFileTranslator *realtimeRoot = GetRealtimeRoot();

        if ( realtimeRoot )
        {
            if ( fsObject->metaData.archived && !fsObject->metaData.cached )
            {
                dstFile = realtimeRoot->Open( relPath, "wb+" );

                if ( dstFile )
                {
                    Extract( *dstFile, *fsObject );
                    fsObject->metaData.cached = true;
                }
            }
            else
            {
                dstFile = realtimeRoot->Open( relPath, "rb+" );
            }
        }
    }
    else if ( openMode == FILE_MODE_CREATE )
    {
        // Attempt to get a handle to the realtime root.
        CFileTranslator *realtimeRoot = GetRealtimeRoot();

        if ( realtimeRoot )
        {
            const filePath& relPath = fsObject->relPath;

            dstFile = realtimeRoot->Open( relPath, "wb+" );

            fsObject->metaData.cached = true;
        }
    }
    else
    {
        // TODO: implement any unknown cases.
        return NULL;
    }

    CFile *outputFile = NULL;

    if ( dstFile )
    {
        fileDeflate *f = new fileDeflate( *this, *fsObject, *dstFile );

        if ( f )
        {
            f->m_path = fsObject->relPath;
            f->m_writeable = ( access & FILE_ACCESS_WRITE ) != 0;
            f->m_readable = ( access & FILE_ACCESS_READ ) != 0;

            // Update the stat
            {
                struct stat info;
                bool statGetSuccess = f->Stat( &info );
                
                if ( statGetSuccess )
                {
                    dstFile->PushStat( &info );
                }
            }

            outputFile = f;
        }
    }
    return outputFile;
}

CFile* CZIPArchiveTranslator::Open( const char *path, const char *mode )
{
    return m_virtualFS.OpenStream( path, mode );
}

bool CZIPArchiveTranslator::Exists( const char *path ) const
{
    return m_virtualFS.Exists( path );
}

void CZIPArchiveTranslator::fileMetaData::OnFileDelete( void )
{
    // Delete the resources that are associated with this file entry.
    const filePath& ourPath = this->genericInterface->GetRelativePath();

    if ( !this->cached )
    {
        CFileTranslator *unpackRoot = translator->GetUnpackRoot();

        if ( unpackRoot )
        {
            unpackRoot->Delete( ourPath );
        }
    }
    else
    {
        CFileTranslator *realtimeRoot = translator->GetRealtimeRoot();

        if ( realtimeRoot )
        {
            realtimeRoot->Delete( ourPath );
        }
    }
}

bool CZIPArchiveTranslator::Delete( const char *path )
{
    return m_virtualFS.Delete( path );
}

bool CZIPArchiveTranslator::fileMetaData::OnFileCopy( const dirTree& tree, const filePath& newName ) const
{
    // Copy over the data storage to the new location.
    bool copySuccess = false;

    // Construct the destination path.
    filePath dstPath;
    _File_OutputPathTree( tree, false, dstPath );

    dstPath += newName;

    // Get the path to this node.
    const filePath& ourPath = this->genericInterface->GetRelativePath();

    if ( !this->cached )
    {
        // Get the unpack root.
        CFileTranslator *unpackRoot = translator->GetUnpackRoot();

        if ( unpackRoot )
        {
            if ( !this->subParsed )
            {
                _localHeader header;
                translator->seekFile( *this, header );

                CFile *dstFile = unpackRoot->Open( dstPath, "wb" );

                if ( dstFile )
                {
                    FileSystem::StreamCopyCount( translator->m_file, *dstFile, header.sizeCompressed );

                    dstFile->SetSeekEnd();

                    delete dstFile;

                    copySuccess = true;
                }
            }
            else
            {
                copySuccess = unpackRoot->Copy( ourPath, dstPath );
            }
        }
    }
    else
    {
        // Get the realtime root.
        CFileTranslator *realtimeRoot = translator->GetRealtimeRoot();

        if ( realtimeRoot )
        {
            copySuccess = realtimeRoot->Copy( ourPath, dstPath );
        }
    }

    return copySuccess;
}

bool CZIPArchiveTranslator::Copy( const char *src, const char *dst )
{
    return m_virtualFS.Copy( src, dst );
}

bool CZIPArchiveTranslator::fileMetaData::OnFileRename( const dirTree& tree, const filePath& newName )
{
    bool success = false;

    // Construct the destination path.
    filePath dstPath;
    _File_OutputPathTree( tree, false, dstPath );

    dstPath += newName;

    // Get the path to this node.
    const filePath& ourPath = this->genericInterface->GetRelativePath();

    // Move our meta-data.
    if ( !this->cached )
    {
        CFileTranslator *unpackRoot = translator->GetUnpackRoot();

        if ( unpackRoot )
        {
            if ( !this->subParsed )
            {
                _localHeader header;
                translator->seekFile( *this, header );

                CFile *dstFile = unpackRoot->Open( dstPath, "wb" );

                if ( dstFile )
                {
                    FileSystem::StreamCopyCount( translator->m_file, *dstFile, header.sizeCompressed );

                    dstFile->SetSeekEnd();

                    delete dstFile;

                    success = true;
                }
            }
            else
            {
                success = unpackRoot->Rename( ourPath, dstPath );
            }
        }
    }
    else
    {
        CFileTranslator *realtimeRoot = translator->GetRealtimeRoot();

        if ( realtimeRoot )
        {
            success = realtimeRoot->Rename( ourPath, dstPath );
        }
    }

    return success;
}

bool CZIPArchiveTranslator::Rename( const char *src, const char *dst )
{
    return m_virtualFS.Rename( src, dst );
}

size_t CZIPArchiveTranslator::Size( const char *path ) const
{
    return (size_t)m_virtualFS.Size( path );
}

bool CZIPArchiveTranslator::Stat( const char *path, struct stat *stats ) const
{
    return m_virtualFS.Stat( path, stats );
}

bool CZIPArchiveTranslator::OnConfirmDirectoryChange( const dirTree& tree )
{
    return m_virtualFS.ChangeDirectory( tree );
}

static void _scanFindCallback( const filePath& path, std::vector <filePath> *output )
{
    output->push_back( path );
}

void CZIPArchiveTranslator::ScanDirectory( const char *path, const char *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    m_virtualFS.ScanDirectory( path, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CZIPArchiveTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, NULL, &output );
}

void CZIPArchiveTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, NULL, (pathCallback_t)_scanFindCallback, &output );
}

void CZIPArchiveTranslator::ReadFiles( unsigned int count )
{
    char buf[65536];

    while ( count-- )
    {
        _centralFile header;

        if ( !m_file.ReadStruct( header ) || header.signature != ZIP_FILE_SIGNATURE )
            throw;

        // Acquire the path
        m_file.Read( buf, 1, header.nameLen );
        buf[ header.nameLen ] = 0;

        dirTree tree;
        bool isFile;

        if ( !GetRelativePathTreeFromRoot( buf, tree, isFile ) )
            throw;

        union
        {
            genericMetaData *fsObject;
            fileMetaData *fileEntry;
            directoryMetaData *dirEntry;
        };
        fsObject = NULL;

        if ( isFile )
        {
            filePath name = tree[ tree.size() - 1 ];
            tree.pop_back();

            // Deposit in the correct directory
            fileEntry = &m_virtualFS._CreateDirTree( m_virtualFS.GetRootDir(), tree ).AddFile( name ).metaData;
        }
        else
        {
            // Try to create the directory.
            dirEntry = &m_virtualFS._CreateDirTree( m_virtualFS.GetRootDir(), tree ).metaData;
        }

        // Initialize common data.
        if ( fsObject )
        {
            fsObject->version = header.version;
            fsObject->reqVersion = header.reqVersion;
            fsObject->flags = header.flags;
            fsObject->modTime = header.modTime;
            fsObject->modDate = header.modDate;
            fsObject->diskID = header.diskID;
            fsObject->localHeaderOffset = header.localHeaderOffset;

            if ( header.extraLen != 0 )
            {
                m_file.Read( buf, 1, header.extraLen );
                fsObject->extra = std::string( buf, header.extraLen );
            }

            if ( header.commentLen != 0 )
            {
                m_file.Read( buf, 1, header.commentLen );
                fsObject->comment = std::string( buf, header.commentLen );
            }
        }
        else
        {
            m_file.Seek( header.extraLen + header.commentLen, SEEK_CUR );
        }

        if ( fileEntry && isFile )
        {
            // Store file-orriented attributes
            fileEntry->compression = header.compression;
            fileEntry->crc32val = header.crc32val;
            fileEntry->sizeCompressed = header.sizeCompressed;
            fileEntry->sizeReal = header.sizeReal;
            fileEntry->internalAttr = header.internalAttr;
            fileEntry->externalAttr = header.externalAttr;
            fileEntry->archived = true;
        }
    }
}

inline void CZIPArchiveTranslator::seekFile( const fileMetaData& info, _localHeader& header )
{
    m_file.SeekNative( info.localHeaderOffset, SEEK_SET );

    if ( !m_file.ReadStruct( header ) )
        throw;

    if ( header.signature != ZIP_LOCAL_FILE_SIGNATURE )
        throw;

    m_file.Seek( header.nameLen + header.commentLen, SEEK_CUR );
}

struct zip_inflate_decompression
{
    zip_inflate_decompression( void )
    {
        m_stream.zalloc = NULL;
        m_stream.zfree = NULL;
        m_stream.opaque = NULL;

        if ( inflateInit2( &m_stream, -MAX_WBITS ) != Z_OK )
            throw;
    }

    ~zip_inflate_decompression( void )
    {
        inflateEnd( &m_stream );
    }

    inline void prepare( const char *buf, size_t size, bool eof )
    {
        m_stream.avail_in = (uInt)size;
        m_stream.next_in = (Bytef*)buf;
    }

    inline bool parse( char *buf, size_t size, size_t& sout )
    {
        m_stream.avail_out = (uInt)size;
        m_stream.next_out = (Bytef*)buf;

        switch( inflate( &m_stream, Z_NO_FLUSH ) )
        {
        case Z_DATA_ERROR:
        case Z_NEED_DICT:
        case Z_MEM_ERROR:
            throw;
        }

        sout = size - m_stream.avail_out;
        return m_stream.avail_out == 0;
    }

    z_stream m_stream;
};

void CZIPArchiveTranslator::Extract( CFile& dstFile, file& info )
{
    CFile *from = NULL;
    size_t comprSize = info.metaData.sizeCompressed;

    bool fromNeedClosing = false;

    if ( info.metaData.subParsed )
    {
        CFileTranslator *unpackRoot = GetUnpackRoot();

        if ( unpackRoot )
        {
            from = unpackRoot->Open( info.relPath, "rb" );

            if ( from )
            {
                fromNeedClosing = true;
            }
        }
    }
    
    if ( !from )
    {
        _localHeader header;
        seekFile( info.metaData, header );

        from = &m_file;
    }

    if ( info.metaData.compression == 0 )
    {
        FileSystem::StreamCopyCount( *from, dstFile, comprSize );
    }
    else if ( info.metaData.compression == 8 )
    {
        zip_inflate_decompression decompressor;

        FileSystem::StreamParserCount( *from, dstFile, comprSize, decompressor );
    }
    else
    {
        assert( 0 );
    }

    dstFile.SetSeekEnd();

    if ( fromNeedClosing )
    {
        delete from;
    }
}

void CZIPArchiveTranslator::CacheDirectory( const directory& dir )
{
    fileList::const_iterator fileIter = dir.files.begin();

    for ( ; fileIter != dir.files.end(); ++fileIter )
    {
        file *fileEntry = *fileIter;

        if ( fileEntry->metaData.cached || fileEntry->metaData.subParsed )
            continue;

        // Dump the compressed content
        CFileTranslator *unpackRoot = GetUnpackRoot();

        if ( unpackRoot )
        {
            _localHeader header;
            seekFile( fileEntry->metaData, header );

            CFile *dst = unpackRoot->Open( fileEntry->relPath, "wb" );

            FileSystem::StreamCopyCount( m_file, *dst, fileEntry->metaData.sizeCompressed );

            dst->SetSeekEnd();

            delete dst;

            fileEntry->metaData.subParsed = true;
        }
    }

    directory::subDirs::const_iterator iter = dir.children.begin();

    for ( ; iter != dir.children.end(); ++iter )
        CacheDirectory( **iter );
}

struct zip_stream_compression
{
    zip_stream_compression( CZIPArchiveTranslator::_localHeader& header ) : m_header( header )
    {
        m_header.sizeCompressed = 0;
    }

    inline void checksum( const char *buf, size_t size )
    {
        m_header.crc32val = crc32( m_header.crc32val, (Bytef*)buf, (uInt)size );
    }

    inline void prepare( const char *buf, size_t size, bool eof )
    {
        m_rcv = size;
        m_buf = buf;

        checksum( buf, size );
    }

    inline bool parse( char *buf, size_t size, size_t& sout )
    {
        size_t toWrite = std::min( size, m_rcv );
        memcpy( buf, m_buf, toWrite );

        m_header.sizeCompressed += (uInt)toWrite;

        m_buf += toWrite;
        m_rcv -= toWrite;

        sout = size;
        return m_rcv != 0;
    }

    size_t m_rcv;
    const char* m_buf;
    CZIPArchiveTranslator::_localHeader& m_header;
};

struct zip_deflate_compression : public zip_stream_compression
{
    zip_deflate_compression( CZIPArchiveTranslator::_localHeader& header, int level ) : zip_stream_compression( header )
    {
        m_stream.zalloc = NULL;
        m_stream.zfree = NULL;
        m_stream.opaque = NULL;

        if ( deflateInit2( &m_stream, level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY ) != Z_OK )
            throw;
    }

    ~zip_deflate_compression( void )
    {
        deflateEnd( &m_stream );
    }

    inline void prepare( const char *buf, size_t size, bool eof )
    {
        m_flush = eof ? Z_FINISH : Z_NO_FLUSH;

        m_stream.avail_in = (uInt)size;
        m_stream.next_in = (Bytef*)buf;

        zip_stream_compression::checksum( buf, size );
    }

    inline bool parse( char *buf, size_t size, size_t& sout )
    {
        m_stream.avail_out = (uInt)size;
        m_stream.next_out = (Bytef*)buf;

        int ret = deflate( &m_stream, m_flush );

        sout = size - m_stream.avail_out;

        m_header.sizeCompressed += (uInt)sout;

        if ( ret == Z_STREAM_ERROR )
            throw;

        return m_stream.avail_out == 0;
    }

    int m_flush;
    z_stream m_stream;
};

void CZIPArchiveTranslator::SaveDirectory( directory& dir, size_t& size )
{
    if ( dir.metaData.NeedsWriting() )
    {
        _localHeader header = dir.metaData.ConstructLocalHeader();
        
        // Allocate space in the archive.
        dir.metaData.AllocateArchiveSpace( this, header, size );

        header.version = dir.metaData.version;
        header.flags = dir.metaData.flags;
        header.compression = 0;
        header.crc32val = 0;
        header.sizeCompressed = 0;
        header.sizeReal = 0;

        m_file.WriteStruct( header );
        m_file.WriteString( dir.relPath );
        m_file.WriteString( dir.metaData.comment );
    }
    
    directory::subDirs::iterator iter = dir.children.begin();

    for ( ; iter != dir.children.end(); ++iter )
        SaveDirectory( **iter, size );

    fileList::iterator fileIter = dir.files.begin();

    for ( ; fileIter != dir.files.end(); ++fileIter )
    {
        file& info = **fileIter;

        bool canProcess = true;

        union
        {
            CFileTranslator *unpackRoot;
            CFileTranslator *realtimeRoot;
        };

        if ( !info.metaData.cached )
        {
            unpackRoot = GetUnpackRoot();

            if ( !unpackRoot )
            {
                canProcess = false;
            }
        }
        else
        {
            realtimeRoot = GetRealtimeRoot();

            if ( !realtimeRoot )
            {
                canProcess = false;
            }
        }

        if ( !canProcess )
            throw;

        _localHeader header = info.metaData.ConstructLocalHeader();

        // Allocate space in the archive.
        info.metaData.AllocateArchiveSpace( this, header, size );

        if ( !info.metaData.cached )
        {
            header.version          = info.metaData.version;
            header.flags            = info.metaData.flags;
            header.compression      = info.metaData.compression;
            header.crc32val         = info.metaData.crc32val;
            header.sizeCompressed   = (fsUInt_t)info.metaData.sizeCompressed;
            header.sizeReal         = (fsUInt_t)info.metaData.sizeReal;

            size += info.metaData.sizeCompressed;

            m_file.WriteStruct( header );
            m_file.WriteString( info.relPath );
            m_file.WriteString( info.metaData.comment );

            CFile *src = unpackRoot->Open( info.relPath, "rb" );

            FileSystem::StreamCopy( *src, m_file );

            m_file.SetSeekEnd();

            delete src;
        }
        else    // has to be cached
        {
            header.version      = 10;    // WINNT
            header.flags        = info.metaData.flags;
            header.compression  = info.metaData.compression = 8; // deflate
            header.crc32val     = 0;

            m_file.WriteStruct( header );
            m_file.WriteString( info.relPath );
            m_file.WriteString( info.metaData.comment );

            CFile *src = dynamic_cast <CSystemFileTranslator*> ( realtimeRoot )->OpenEx( info.relPath, "rb", FILE_FLAG_WRITESHARE );

            assert( src != NULL );

            size_t actualFileSize = src->GetSize();

            info.metaData.sizeReal = actualFileSize;

            header.sizeReal = (fsUInt_t)actualFileSize;

            if ( header.compression == 0 )
            {
                zip_stream_compression compressor( header );

                FileSystem::StreamParser( *src, m_file, compressor );
            }
            else if ( header.compression == 8 )
            {
                zip_deflate_compression compressor( header, Z_DEFAULT_COMPRESSION );

                FileSystem::StreamParser( *src, m_file, compressor );
            }
            else
            {
                assert( 0 );
            }

            delete src;

            size += info.metaData.sizeCompressed = header.sizeCompressed;
            info.metaData.crc32val = header.crc32val;

            long wayOff = header.sizeCompressed + header.nameLen + header.commentLen;

            m_file.Seek( -wayOff - (long)sizeof( header ), SEEK_CUR );
            m_file.WriteStruct( header );
            m_file.Seek( wayOff, SEEK_CUR );
        }
    }
}

unsigned int CZIPArchiveTranslator::BuildCentralFileHeaders( const directory& dir, size_t& size )
{
    unsigned cnt = 0;

    if ( dir.metaData.NeedsWriting() )
    {
        _centralFile header = dir.metaData.ConstructCentralHeader();

        // Zero out fields which make no sense
        header.compression = 0;
        header.crc32val = 0;
        header.sizeCompressed = 0;
        header.sizeReal = 0;
        header.internalAttr = 0;
#ifdef _WIN32
        header.externalAttr = FILE_ATTRIBUTE_DIRECTORY;
#else
        header.externalAttr = 0;
#endif
        
        m_file.WriteStruct( header );
        m_file.WriteString( dir.relPath );
        m_file.WriteString( dir.metaData.extra );
        m_file.WriteString( dir.metaData.comment );
        
        cnt++;
    }

    directory::subDirs::const_iterator iter = dir.children.begin();

    for ( ; iter != dir.children.end(); ++iter )
        cnt += BuildCentralFileHeaders( **iter, size );

    fileList::const_iterator fileIter = dir.files.begin();

    for ( ; fileIter != dir.files.end(); ++fileIter )
    {
        const file& info = **fileIter;
        _centralFile header = info.metaData.ConstructCentralHeader();

        header.compression      = info.metaData.compression;
        header.crc32val         = info.metaData.crc32val;
        header.sizeCompressed   = (fsUInt_t)info.metaData.sizeCompressed;
        header.sizeReal         = (fsUInt_t)info.metaData.sizeReal;
        header.internalAttr     = info.metaData.internalAttr;
        header.externalAttr     = info.metaData.externalAttr;

        m_file.WriteStruct( header );
        m_file.WriteString( info.relPath );
        m_file.WriteString( info.metaData.extra );
        m_file.WriteString( info.metaData.comment );

        size += sizeof( header ) + header.nameLen + header.extraLen + header.commentLen;
        cnt++;
    }

    return cnt;
}

void CZIPArchiveTranslator::Save( void )
{
    if ( !m_file.IsWriteable() )
        return;

    // Cache the .zip content
    CacheDirectory( m_virtualFS.GetRootDir() );

    // Rewrite the archive
    m_file.SeekNative( m_structOffset, SEEK_SET );

    size_t fileSize = 0;
    SaveDirectory( m_virtualFS.GetRootDir(), fileSize );

    // Create the central directory
    size_t centralSize = 0;
    unsigned int entryCount = BuildCentralFileHeaders( m_virtualFS.GetRootDir(), centralSize );

    // Finishing entry
    m_file.WriteInt( ZIP_SIGNATURE );

    _endDir tail;
    tail.diskID = 0;
    tail.diskAlign = 0;
    tail.entries = entryCount;
    tail.totalEntries = entryCount;
    tail.centralDirectorySize = (fsUInt_t)centralSize;
    tail.centralDirectoryOffset = (fsUInt_t)fileSize; // we might need the offset of the .zip here
    tail.commentLen = (fsUShort_t)m_comment.size();

    m_file.WriteStruct( tail );
    m_file.WriteString( m_comment );

    // Cap the stream
    m_file.SetSeekEnd();
}