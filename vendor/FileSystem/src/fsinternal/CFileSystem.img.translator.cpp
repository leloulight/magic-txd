/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.translator.cpp
*  PURPOSE:     IMG R* Games archive management
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>
#include <sys/stat.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.img.internal.h"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"

/*=======================================
    CIMGArchiveTranslator::dataCachedStream

    IMG archive read-only data stream
=======================================*/
inline CIMGArchiveTranslator::dataCachedStream::dataCachedStream( CIMGArchiveTranslator *translator, file *theFile, filePath thePath, unsigned int accessMode, CFile *rawStream ) : streamBase( translator, theFile, thePath, accessMode )
{
    this->m_rawStream = rawStream;
}

inline CIMGArchiveTranslator::dataCachedStream::~dataCachedStream( void )
{
    delete m_rawStream;
}

size_t CIMGArchiveTranslator::dataCachedStream::Read( void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !IsReadable() )
        return 0;

    return m_rawStream->Read( buffer, sElement, iNumElements );
}

size_t CIMGArchiveTranslator::dataCachedStream::Write( const void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !IsWriteable() )
        return 0;

    return m_rawStream->Write( buffer, sElement, iNumElements );
}

int CIMGArchiveTranslator::dataCachedStream::Seek( long iOffset, int iType )
{
    return m_rawStream->Seek( iOffset, iType );
}

int CIMGArchiveTranslator::dataCachedStream::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
    return m_rawStream->SeekNative( iOffset, iType );
}

long CIMGArchiveTranslator::dataCachedStream::Tell( void ) const
{
    return m_rawStream->Tell();
}

fsOffsetNumber_t CIMGArchiveTranslator::dataCachedStream::TellNative( void ) const
{
    return m_rawStream->TellNative();
}

bool CIMGArchiveTranslator::dataCachedStream::IsEOF( void ) const
{
    return m_rawStream->IsEOF();
}

bool CIMGArchiveTranslator::dataCachedStream::Stat( struct stat *stats ) const
{
    return m_rawStream->Stat( stats );
}

void CIMGArchiveTranslator::dataCachedStream::PushStat( const struct stat *stats )
{
    if ( !IsWriteable() )
        return;
    
    m_rawStream->PushStat( stats );
}

void CIMGArchiveTranslator::dataCachedStream::SetSeekEnd( void )
{
    if ( !IsWriteable() )
        return;

    m_rawStream->SetSeekEnd();
}

size_t CIMGArchiveTranslator::dataCachedStream::GetSize( void ) const
{
    return m_rawStream->GetSize();
}

fsOffsetNumber_t CIMGArchiveTranslator::dataCachedStream::GetSizeNative( void ) const
{
    return m_rawStream->GetSizeNative();
}

void CIMGArchiveTranslator::dataCachedStream::Flush( void )
{
    if ( !IsWriteable() )
        return;

    m_rawStream->Flush();
}

/*=======================================
    CIMGArchiveTranslator::dataSectorStream

    IMG archive read-only data stream
=======================================*/
inline CIMGArchiveTranslator::dataSectorStream::dataSectorStream( CIMGArchiveTranslator *translator, file *theFile, filePath thePath ) : streamBase( translator, theFile, thePath, FILE_ACCESS_READ )
{
    this->m_currentSeek = 0;
}

inline CIMGArchiveTranslator::dataSectorStream::~dataSectorStream( void )
{
    return;
}

inline fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::_getoffset( void ) const
{
    return ( this->m_info->metaData.blockOffset * IMG_BLOCK_SIZE );
}

inline fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::_getsize( void ) const
{
    return ( this->m_info->metaData.resourceSize * IMG_BLOCK_SIZE );
}

inline void CIMGArchiveTranslator::dataSectorStream::TargetSourceFile( void )
{
    // Calculate the offset inside of the file we should seek to.
    fsOffsetNumber_t imgArchiveRealSeek =
        ( _getoffset() + this->m_currentSeek );

    this->m_translator->m_contentFile->SeekNative( imgArchiveRealSeek, SEEK_SET );
}

inline size_t CIMGArchiveTranslator::dataSectorStream::GetBytesLeftToRead( void ) const
{
    size_t realResourceSize = ( this->m_info->metaData.resourceSize * IMG_BLOCK_SIZE );

    // Get the amount of bytes we may actually read from the file.
    size_t bytesLeftToRead = ( realResourceSize - (size_t)this->m_currentSeek );

    return bytesLeftToRead;
}

size_t CIMGArchiveTranslator::dataSectorStream::Read( void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !IsReadable() )
        return 0;
    
    size_t bytesLeftToRead = GetBytesLeftToRead();

    // Make sure we do not try to read past the buffer.
    size_t realReadSize = std::min( bytesLeftToRead, (size_t)( sElement * iNumElements ) );

    if ( realReadSize == 0 )
        return 0;

    TargetSourceFile();

    // Attempt to read.
    size_t actuallyRead = this->m_translator->m_contentFile->Read( buffer, 1, realReadSize );

    // Advance the seek by the bytes that have been read.
    this->m_currentSeek += actuallyRead;

    return ( actuallyRead / sElement );
}

size_t CIMGArchiveTranslator::dataSectorStream::Write( const void *buffer, size_t sElement, size_t iNumElements )
{
    if ( !IsWriteable() )
        return 0;

    // TODO: add support for this shit.
    TargetSourceFile();

    return 0;
}

int CIMGArchiveTranslator::dataSectorStream::Seek( long iOffset, int iType )
{
    // Update the seek pointer depending on operation.
    long offsetBase = 0;

    if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        offsetBase = this->Tell();
    }
    else if ( iType == SEEK_END )
    {
        offsetBase = (long)this->GetSize();
    }

    fsOffsetNumber_t newOffset = (fsOffsetNumber_t)( offsetBase + iOffset );

    // Verify the offset.
    if ( newOffset < 0 )
        return -1;

    // Alright, set the new offset.
    this->m_currentSeek = newOffset;

    return 0;
}

int CIMGArchiveTranslator::dataSectorStream::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
    // Do the same as seek, but do so with higher accuracy.
    fsOffsetNumber_t offsetBase;

    if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_CUR )
    {
        offsetBase = this->TellNative();
    }
    else if ( iType == SEEK_END )
    {
        offsetBase = this->GetSizeNative();
    }

    fsOffsetNumber_t newOffset = ( offsetBase + (fsOffsetNumber_t)iOffset );

    // Verify the offset.
    if ( newOffset < 0 )
        return -1;

    // Update our offset.
    this->m_currentSeek = newOffset;

    return 0;
}

long CIMGArchiveTranslator::dataSectorStream::Tell( void ) const
{
    return (long)this->m_currentSeek;
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::TellNative( void ) const
{
    return this->m_currentSeek;
}

bool CIMGArchiveTranslator::dataSectorStream::IsEOF( void ) const
{
    return ( GetBytesLeftToRead() == 0 );
}

bool CIMGArchiveTranslator::dataSectorStream::Stat( struct stat *stats ) const
{
    m_translator->m_virtualFS.StatObject( m_info, stats );
    return true;
}

void CIMGArchiveTranslator::dataSectorStream::PushStat( const struct stat *stats )
{
    if ( !IsWriteable() )
        return;
}

void CIMGArchiveTranslator::dataSectorStream::SetSeekEnd( void )
{
    if ( !IsWriteable() )
        return;
}

size_t CIMGArchiveTranslator::dataSectorStream::GetSize( void ) const
{
    return (size_t)_getsize();
}

fsOffsetNumber_t CIMGArchiveTranslator::dataSectorStream::GetSizeNative( void ) const
{
    return _getsize();
}

void CIMGArchiveTranslator::dataSectorStream::Flush( void )
{
    // TODO
}

/*=======================================
    CIMGArchiveTranslator

    R* Games IMG archive management
=======================================*/

// Header of a file entry in the IMG archive.
struct resourceFileHeader_ver1
{
    fsUInt_t            offset;                         // 0
    fsUInt_t            fileDataSize;                   // 4
    char                name[24];                       // 8
};

struct resourceFileHeader_ver2
{
    fsUInt_t            offset;                         // 0
    fsUShort_t          fileDataSize;                   // 4
    fsUShort_t          expandedSize;                   // 6
    char                name[24];                       // 8
};


CIMGArchiveTranslator::CIMGArchiveTranslator( imgExtension& imgExt, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion theVersion )
    : CSystemPathTranslator( false ), m_imgExtension( imgExt ), m_contentFile( contentFile ), m_registryFile( registryFile )
{
    // Set up the virtual file system by giving the
    // translator pointer to it.
    m_virtualFS.hostTranslator = this;

    // Fill out default fields.
    this->m_version = theVersion;

    // NULL the file root translator.
    this->m_fileRoot = NULL;
    this->m_unpackRoot = NULL;
    this->m_compressRoot = NULL;

    // We have no compression handler by default.
    this->m_compressionHandler = NULL;
}

CIMGArchiveTranslator::~CIMGArchiveTranslator( void )
{
    filePath path;

    // Unlock the archive file.
    delete this->m_contentFile;

    if ( this->m_version == IMG_VERSION_1 )
    {
        delete this->m_registryFile;
    }

    bool needDeletion = ( m_fileRoot != NULL );

    if ( m_fileRoot )
    {
        m_fileRoot->GetFullPath( "@", false, path );
    }

    // Destroy the locks to our runtime management folders.
    if ( m_compressRoot )
    {
        delete m_compressRoot;

        m_compressRoot = NULL;
    }

    if ( m_unpackRoot )
    {
        delete m_unpackRoot;

        m_unpackRoot = NULL;
    }

    if ( m_fileRoot )
    {
        delete m_fileRoot;

        m_fileRoot = NULL;
    }

    if ( needDeletion )
    {
        // Delete all temporary files by deleting our entire folder structure.
        if ( CFileTranslator *sysTmpRoot = m_imgExtension.GetTempRoot() )
        {
            sysTmpRoot->Delete( path.c_str() );
        }
    }
}

CFileTranslator* CIMGArchiveTranslator::GetFileRoot( void )
{
    if ( !m_fileRoot )
    {
        m_fileRoot = m_imgExtension.repo.GenerateUniqueDirectory();
    }
    return m_fileRoot;
}

CFileTranslator* CIMGArchiveTranslator::GetUnpackRoot( void )
{
    CFileTranslator *unpackRoot = this->m_unpackRoot;

    if ( unpackRoot == NULL )
    {
        CFileTranslator *fileRoot = this->GetFileRoot();

        if ( fileRoot )
        {
            unpackRoot = CRepository::AcquireDirectoryTranslator( fileRoot, "unpack/" );

            if ( unpackRoot )
            {
                this->m_unpackRoot = unpackRoot;
            }
        }
    }
    return unpackRoot;
}

CFileTranslator* CIMGArchiveTranslator::GetCompressRoot( void )
{
    CFileTranslator *compressRoot = this->m_compressRoot;

    if ( compressRoot == NULL )
    {
        CFileTranslator *fileRoot = this->GetFileRoot();

        if ( fileRoot )
        {
            compressRoot = CRepository::AcquireDirectoryTranslator( fileRoot, "compress/" );

            if ( compressRoot )
            {
                this->m_compressRoot = compressRoot;
            }
        }
    }
    return compressRoot;
}

bool CIMGArchiveTranslator::WriteData( const char *path, const char *buffer, size_t size )
{
    // TODO
    return false;
}

bool CIMGArchiveTranslator::CreateDir( const char *path )
{
    return m_virtualFS.CreateDir( path );
}

CFile* CIMGArchiveTranslator::OpenNativeFileStream( file *fsObject, unsigned int openMode, unsigned int access )
{
    CFile *outputStream = NULL;

    const filePath& relPath = fsObject->relPath;

    bool needsCachedWrap = false;

    if ( openMode == FILE_MODE_OPEN )
    {
        // If the data has been extracted already from the archive, we need to open a disk stream.
        if ( fsObject->metaData.isExtracted )
        {
            CFile *diskFile = fileRoot->Open( relPath.c_str(), "rb+" );

            if ( diskFile )
            {
                dataCachedStream *diskStream = new dataCachedStream( this, fsObject, relPath, access, diskFile );

                if ( diskStream )
                {
                    //todo: maybe set special properties.

                    outputStream = diskStream;
                }
            }

            if ( outputStream == NULL )
            {
                delete diskFile;
            }
        }
        else
        {
            // Determine whether our stream needs extraction.
            bool needsExtraction = false;

            if ( !needsExtraction )
            {
                // If we want write access, we definately have to extract.
                if ( ( access & FILE_ACCESS_WRITE ) != 0 )
                {
                    needsExtraction = true;
                }
            }

            // Create our stream.
            dataSectorStream *dataStream = new dataSectorStream( this, fsObject, relPath );

            if ( !needsExtraction )
            {
                // If we are not writing, then we may be compressed.
                // Compressed files have to be extracted.
                bool runtimeExtractionRequest = this->RequiresExtraction( dataStream );

                if ( runtimeExtractionRequest )
                {
                    needsExtraction = true;
                }
            }

            if ( dataStream )
            {
                CFile *intermediateStream = NULL;

                // If we have to extract, do that and return a handle to the on-disk file.
                if ( needsExtraction )
                {
                    // Extract the stream.
                    CFileTranslator *fileRoot = this->GetUnpackRoot();
                    
                    if ( fileRoot )
                    {
                        CFile *extractedStream = fileRoot->Open( relPath.c_str(), "wb+" );

                        if ( extractedStream )
                        {
                            // Perform the extraction.
                            bool extractionSuccess = this->ExtractStream( dataStream, extractedStream, fsObject );

                            if ( extractionSuccess )
                            {
                                // Return our extracted stream.
                                intermediateStream = extractedStream;

                                // We need to wrap this stream in a special class.
                                needsCachedWrap = true;
                            }

                            // Clean up if not required anymore.
                            if ( intermediateStream != extractedStream )
                            {
                                delete extractedStream;
                            }
                        }
                    }
                }
                else
                {
                    // Otherwise we can return the optimized in-archive file.
                    intermediateStream = dataStream;
                }

                // Clean up the in-archive handle if we don't need it anymore.
                if ( intermediateStream != dataStream )
                {
                    delete dataStream;
                }

                outputStream = intermediateStream;
            }
        }
    }
    else if ( openMode == FILE_MODE_CREATE )
    {
        CFileTranslator *fileRoot = this->GetUnpackRoot();

        if ( fileRoot )
        {
            CFile *diskFile = fileRoot->Open( relPath.c_str(), "wb+" );

            if ( diskFile )
            {
                if ( fsObject->metaData.isExtracted == false )
                {
                    // Notify the file registry that a cached file has replaced the
                    // original archive entry.
                    fsObject->metaData.isExtracted = true;
                }

                // Return the handle to the on-disk file.
                outputStream = diskFile;

                // Remember to wrap it.
                needsCachedWrap = true;
            }

            // Fix grave errors.
            if ( outputStream == NULL )
            {
                delete diskFile;

                fileRoot->Delete( relPath.c_str() );
            }
        }

        if ( outputStream == NULL )
        {
            // TODO: figure out how to deal with this error!
            assert( 0 );
        }
    }

    if ( outputStream )
    {
        if ( needsCachedWrap )
        {
            dataCachedStream *repoStream = new dataCachedStream( this, fsObject, relPath, access, outputStream );

            if ( repoStream )
            {
                //todo: maybe set special properties.

                outputStream = repoStream;
            }
            else
            {
                // We kinda failed.
                delete outputStream;

                outputStream = NULL;
            }
        }
    }

    return outputStream;
}

bool CIMGArchiveTranslator::RequiresExtraction( CFile *stream )
{
    bool needsExtraction = false;

    // If we have a compression provider, check it.
    if ( CIMGArchiveCompressionHandler *compressHandler = this->m_compressionHandler )
    {
        // Save the stream seek.
        fsOffsetNumber_t savedOffset = stream->TellNative();
        
        // Query the compression provider.
        bool isCompressed = compressHandler->IsStreamCompressed( stream );

        if ( isCompressed )
        {
            // If we are compressed, we must decompress.
            // This is done at extraction time.
            needsExtraction = true;
        }

        // Reset the stream.
        stream->SeekNative( savedOffset, SEEK_SET );
    }

    return needsExtraction;
}

bool CIMGArchiveTranslator::ExtractStream( CFile *input, CFile *output, file *theFile )
{
    // If we have a compressed provider and it recognizes that the stream is compressed,
    // then let it process the stream.
    bool hasProcessed = false;

    if ( !hasProcessed )
    {
        if ( CIMGArchiveCompressionHandler *compressHandler = this->m_compressionHandler )
        {
            fsOffsetNumber_t savedOffset = input->TellNative();

            bool isCompressed = compressHandler->IsStreamCompressed( input );

            input->SeekNative( savedOffset, SEEK_SET );

            if ( isCompressed )
            {
                bool decompressSuccess = compressHandler->Decompress( input, output );

                if ( decompressSuccess )
                {
                    hasProcessed = true;
                }
                else
                {
                    // We have to reset the stream and try a file-copy instead.
                    input->SeekNative( 0, SEEK_SET );
                    output->SeekNative( 0, SEEK_SET );
                }
            }
        }
    }

    // If we have not proceesed it through the compression provider, simply copy the stream.
    if ( !hasProcessed )
    {
        // TODO.
        FileSystem::StreamCopy( *input, *output );
    }

    // We successfully copied, so mark as extracted.
    theFile->metaData.isExtracted = true;

    return true;
}

CFile* CIMGArchiveTranslator::Open( const char *path, const char *mode )
{
    return m_virtualFS.OpenStream( path, mode );
}

bool CIMGArchiveTranslator::Exists( const char *path ) const
{
    return m_virtualFS.Exists( path );
}

void CIMGArchiveTranslator::fileMetaData::OnFileDelete( void )
{
    // Delete all left-overs.
    if ( this->isExtracted )
    {
        CFileTranslator *fileRoot = translator->GetUnpackRoot();

        if ( fileRoot )
        {
            const filePath& ourPath = this->fileNode->GetRelativePath();

            fileRoot->Delete( ourPath.c_str() );
        }
    }
}

bool CIMGArchiveTranslator::Delete( const char *path )
{
    return m_virtualFS.Delete( path );
}

bool CIMGArchiveTranslator::fileMetaData::OnFileCopy( const dirTree& tree, const filePath& newName ) const
{
    bool copySuccess = false;

    if ( this->isExtracted )
    {
        // Copy over the extracted disk content to another location.
        CFileTranslator *fileRoot = translator->GetUnpackRoot();

        if ( fileRoot )
        {
            const filePath& ourPath = this->fileNode->GetRelativePath();

            filePath dstPath;
            _File_OutputPathTree( tree, false, dstPath );

            dstPath += newName;

            copySuccess = fileRoot->Copy( ourPath.c_str(), dstPath.c_str() );
        }
    }
    else
    {
        // We reuse the data that is loacted inside of the archive.
        // This means that there is no meta-data to copy over.
        copySuccess = true;
    }

    return copySuccess;
}

bool CIMGArchiveTranslator::Copy( const char *src, const char *dst )
{
    return m_virtualFS.Copy( src, dst );
}

bool CIMGArchiveTranslator::fileMetaData::OnFileRename( const dirTree& tree, const filePath& newName )
{
    bool renameSuccess = false;

    if ( this->isExtracted )
    {
        CFileTranslator *fileRoot = translator->GetUnpackRoot();
        
        if ( fileRoot )
        {
            // Move the extracted file.
            const filePath& ourPath = this->fileNode->GetRelativePath();

            filePath dstPath;
            _File_OutputPathTree( tree, false, dstPath );

            dstPath += newName;

            renameSuccess = fileRoot->Rename( ourPath.c_str(), dstPath.c_str() );
        }
    }
    else
    {
        // Nothing to do if we are not extracted.
        // Success by default.
        renameSuccess = true;
    }

    return renameSuccess;
}

bool CIMGArchiveTranslator::Rename( const char *src, const char *dst )
{
    return m_virtualFS.Rename( src, dst );
}

size_t CIMGArchiveTranslator::Size( const char *path ) const
{
    return (size_t)m_virtualFS.Size( path );
}

bool CIMGArchiveTranslator::Stat( const char *path, struct stat *stats ) const
{
    return m_virtualFS.Stat( path, stats );
}

bool CIMGArchiveTranslator::ReadToBuffer( const char *path, std::vector <char>& output ) const
{   
    // TODO, not that important.
    return false;
}

bool CIMGArchiveTranslator::ChangeDirectory( const char *path )
{
    dirTree tree;
    bool isFile;

    if ( !GetRelativePathTreeFromRoot( path, tree, isFile ) )
        return false;

    if ( isFile )
        tree.pop_back();

    bool contextChangeSuccess = m_virtualFS.ChangeDirectory( tree );

    if ( !contextChangeSuccess )
        return false;

    // Update the inherited values from the path translator base class.
    m_curDirTree = tree;

    m_currentDir.clear();
    _File_OutputPathTree( tree, false, m_currentDir );
    return true;
}

static void _scanFindCallback( const filePath& path, std::vector <filePath> *output )
{
    output->push_back( path );
}

void CIMGArchiveTranslator::ScanDirectory( const char *path, const char *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    m_virtualFS.ScanDirectory( path, wildcard, recurse, dirCallback, fileCallback, userdata );
}

void CIMGArchiveTranslator::GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, (pathCallback_t)_scanFindCallback, NULL, &output );
}

void CIMGArchiveTranslator::GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    ScanDirectory( path, wildcard, recurse, NULL, (pathCallback_t)_scanFindCallback, &output );
}

void CIMGArchiveTranslator::GenerateFileHeaderStructure( directory& baseDir, headerGenPresence& genOut )
{
    // Generate for all sub directories first.
    for ( directory::subDirs::const_iterator iter = baseDir.children.begin(); iter != baseDir.children.end(); iter++ )
    {
        directory *childDir = *iter;

        GenerateFileHeaderStructure( *childDir, genOut );
    }

    // Now perform operation on us.
    // Just add together all the file counts.
    genOut.numOfFiles += baseDir.files.size();
}

void CIMGArchiveTranslator::GenerateArchiveStructure( directory& baseDir, archiveGenPresence& genOut )
{
    // Get the file count for all sub directories.
    for ( directory::subDirs::const_iterator iter = baseDir.children.begin(); iter != baseDir.children.end(); iter++ )
    {
        directory *childDir = *iter;

        GenerateArchiveStructure( *childDir, genOut );
    }

    // Loop through all our files and generate their presence.
    CIMGArchiveCompressionHandler *compressHandler = this->m_compressionHandler;

    for ( fileList::iterator iter = baseDir.files.begin(); iter != baseDir.files.end(); iter++ )
    {
        file *theFile = *iter;

        // Position the file onto the archive.
        size_t newBlockOffset = genOut.currentBlockCount;

        // Get the block count of this file entry.
        unsigned long blockCount = 0;

        const char *relativePath = theFile->relPath.c_str();

        // Determine whether we need to compress this file.
        bool requiresCompression = false;

        if ( compressHandler != NULL )
        {
            // If we have a compression handler, we generarily compress everything.
            requiresCompression = true;
        }

        bool hasCompressed = false;

        // Grab the destination handle.
        // We will calculate the blocksize depending on it.
        CFile *destinationHandle = NULL;

        CFile *srcHandle = NULL;

        bool checkWhetherAlreadyCompressed = true;

        bool isExtraction = false;

        if ( theFile->metaData.isExtracted )
        {
            CFileTranslator *fileRoot = this->GetUnpackRoot();

            if ( fileRoot )
            {
                CFile *diskFile = fileRoot->Open( relativePath, "rb" );

                CFile *compressToHandle = NULL;

                // If we need to compress the file, then compress it and query the block size from the compressed file.
                if ( requiresCompression )
                {
                    CFileTranslator *compressRoot = this->GetCompressRoot();

                    if ( compressRoot )
                    {
                        CFile *compressedOut = compressRoot->Open( relativePath, "wb+" );

                        if ( compressedOut )
                        {
                            // Return the handle to the compressed file.
                            compressToHandle = compressedOut;

                            // We are located in the compress root.
                            hasCompressed = true;
                        }
                    }
                }

                if ( compressToHandle == NULL )
                {
                    // If the compression has failed or we dont have to compress, then just query the disk file.
                    compressToHandle = diskFile;
                }

                if ( compressToHandle )
                {
                    srcHandle = diskFile;

                    destinationHandle = compressToHandle;
                }

                // Since we are extracted, we cannot be compressed.
                // We can optimize away the compression check.
                checkWhetherAlreadyCompressed = false;
            }
        }
        else
        {
            // We need to dump the file to disk.
            CFile *srcStream = new dataSectorStream( this, theFile, theFile->relPath );

            if ( srcStream )
            {
                // If we need to compress, we need to dump a compressed copy.
                if ( destinationHandle == NULL && requiresCompression )
                {
                    CFileTranslator *compressRoot = this->GetCompressRoot();

                    if ( compressRoot )
                    {
                        CFile *dstStream = compressRoot->Open( relativePath, "wb" );

                        if ( dstStream )
                        {
                            // The file we want is now located in the compress root.
                            // Even if it may not be compressed already.
                            hasCompressed = true;

                            // Give the destination stream to the runtime.
                            destinationHandle = dstStream;
                        }
                    }

                    // If we have not successfully compressed, we need to put it into the unpack root at least.
                }
                
                // If anything else failed, we just put it into the unpack root.
                if ( destinationHandle == NULL )
                {
                    CFileTranslator *fileRoot = this->GetUnpackRoot();

                    if ( fileRoot )
                    {
                        // Create the file in the target directory and put data into it.
                        CFile *dstStream = fileRoot->Open( relativePath, "wb" );

                        if ( dstStream )
                        {
                            // We should extract the stream instead.
                            isExtraction = true;

                            // Give the destination handle to the runtime.
                            destinationHandle = dstStream;
                        }
                        else
                        {
                            assert( 0 );
                        }
                    }
                }

                // Make sure we pass the source handle aswell.
                srcHandle = srcStream;
            }
            else
            {
                assert( 0 );
            }
        }

        // Perform a parsing (if required).
        if ( srcHandle && destinationHandle && destinationHandle != srcHandle )
        {
            bool processedParse = false;

            if ( !processedParse && requiresCompression )
            {
                bool shouldCompress = true;

                if ( checkWhetherAlreadyCompressed )
                {
                    bool isAlreadyCompressed = compressHandler->IsStreamCompressed( srcHandle );

                    srcHandle->SeekNative( 0, SEEK_SET );

                    if ( isAlreadyCompressed )
                    {
                        // If we are already compressed, there is no point in compressing.
                        // We can just take the source stream and use it for processing.
                        shouldCompress = false;
                    }
                }

                if ( shouldCompress )
                {
                    bool hasSuccessfullyCompressed = compressHandler->Compress( srcHandle, destinationHandle );

                    if ( hasSuccessfullyCompressed )
                    {
                        processedParse = true;
                    }
                    
                    if ( !processedParse )
                    {
                        srcHandle->SeekNative( 0, SEEK_SET );
                        destinationHandle->SeekNative( 0, SEEK_SET );
                    }
                }
            }

            if ( !processedParse && isExtraction )
            {
                // Do the extraction now.
                bool extractSuccess = this->ExtractStream( srcHandle, destinationHandle, theFile );

                if ( extractSuccess )
                {
                    processedParse = true;
                }
            }

            if ( !processedParse )
            {
                // Just copy over the file.
                FileSystem::StreamCopy( *srcHandle, *destinationHandle );

                // Do not forget to trim it off at the end.
                destinationHandle->SetSeekEnd();
            }
        }

        // If we have a source handle, just close it.
        if ( srcHandle && srcHandle != destinationHandle )
        {
            delete srcHandle;

            srcHandle = NULL;
        }

        if ( !destinationHandle )
        {
            // If there could not even be a destination handle, we have got a problem.
            assert( 0 );
        }
        else
        {
            // Query its size and calculate the block could depending on it.
            fsOffsetNumber_t realFileSize = destinationHandle->GetSizeNative();

            blockCount =
                (unsigned long)ALIGN_SIZE( realFileSize, (fsOffsetNumber_t)IMG_BLOCK_SIZE ) / IMG_BLOCK_SIZE;

            // Close it.
            delete destinationHandle;
        }

        // Update the resource properties.
        theFile->metaData.resourceSize = blockCount;
        theFile->metaData.blockOffset = newBlockOffset;
        theFile->metaData.hasCompressed = hasCompressed;

        // Update the generated block count.
        genOut.currentBlockCount += blockCount;
    }
}

void CIMGArchiveTranslator::WriteFileHeaders( CFile *targetStream, directory& baseDir )
{
    // Write files of all sub directories first.
    for ( directory::subDirs::iterator iter = baseDir.children.begin(); iter != baseDir.children.end(); iter++ )
    {
        directory *subDir = *iter;

        WriteFileHeaders( targetStream, *subDir );
    }

    // Now write our files.
    eIMGArchiveVersion theVersion = this->m_version;

    for ( fileList::iterator iter = baseDir.files.begin(); iter != baseDir.files.end(); iter++ )
    {
        file *theFile = *iter;

        union
        {
            resourceFileHeader_ver1 _ver1Header;
            resourceFileHeader_ver2 _ver2Header;
        };

        void *headerPointer = NULL;
        size_t headerSize = 0;

        char *namePointer = NULL;
        size_t maxName = 0;

        if ( theVersion == IMG_VERSION_1 )
        {
            headerPointer = &_ver1Header;
            headerSize = sizeof( _ver1Header );

            _ver1Header.offset = (fsUInt_t)theFile->metaData.blockOffset;
            _ver1Header.fileDataSize = (fsUInt_t)theFile->metaData.resourceSize;

            namePointer = _ver1Header.name;
            maxName = sizeof( _ver1Header.name );
        }
        else
        {
            headerPointer = &_ver2Header;
            headerSize = sizeof( _ver2Header );

            _ver2Header.offset = (fsUInt_t)theFile->metaData.blockOffset;
            _ver2Header.fileDataSize = (fsUShort_t)theFile->metaData.resourceSize;
            _ver2Header.expandedSize = 0;   // always zero.

            namePointer = _ver2Header.name;
            maxName = sizeof( _ver2Header.name );
        }
       
        if ( namePointer )
        {
            // Create a (trimmed) filename.
            std::string ansiName = theFile->relPath.convert_ansi();

            size_t maxAllowedFilename = maxName;
            size_t currentNumFilename = ansiName.size();

            size_t actualCopyCount = std::min( maxAllowedFilename - 1, currentNumFilename );

            memcpy( namePointer, ansiName.c_str(), actualCopyCount );

            // Zero terminate the name.
            namePointer[ actualCopyCount ] = '\0';
        }

        if ( headerPointer )
        {
            // Write the header to the stream.
            targetStream->Write( headerPointer, 1, headerSize );
        }
    }
}

void CIMGArchiveTranslator::WriteFiles( CFile *targetStream, directory& baseDir )
{
    // Write files of all sub directories first.
    for ( directory::subDirs::iterator iter = baseDir.children.begin(); iter != baseDir.children.end(); iter++ )
    {
        directory *subDir = *iter;

        WriteFiles( targetStream, *subDir );
    }

    // Now write our files.
    for ( fileList::iterator iter = baseDir.files.begin(); iter != baseDir.files.end(); iter++ )
    {
        file *theFile = *iter;

        // Seek to the required position.
        {
            fsOffsetNumber_t requiredArchivePos = ( theFile->metaData.blockOffset * IMG_BLOCK_SIZE );

            targetStream->SeekNative( requiredArchivePos, SEEK_SET );
        }

        // Write the data.
        CFile *srcStream = NULL;

        if ( theFile->metaData.hasCompressed )
        {
            CFileTranslator *fileRoot = this->GetCompressRoot();

            if ( fileRoot )
            {
                srcStream = fileRoot->Open( theFile->relPath.c_str(), "rb" );
            }
        }
        else if ( theFile->metaData.isExtracted )
        {
            CFileTranslator *fileRoot = this->GetUnpackRoot();

            if ( fileRoot )
            {
                srcStream = fileRoot->Open( theFile->relPath.c_str(), "rb" );
            }
        }

        // It should never be NULL, but it can be, if something goes horribly wrong.
        assert( srcStream != NULL );

        if ( srcStream )
        {
            FileSystem::StreamCopy( *srcStream, *targetStream );

            delete srcStream;
        }
    }
}

struct generalHeader
{
    fsUInt_t checksum;
    fsUInt_t numberOfEntries;
};

inline size_t getDataBlockCount( size_t dataSize )
{
    return ALIGN_SIZE <size_t> ( dataSize, IMG_BLOCK_SIZE ) / IMG_BLOCK_SIZE;
}

void CIMGArchiveTranslator::Save( void )
{
    // We can only work if the underlying stream is writeable.
    if ( !m_contentFile->IsWriteable() || !m_registryFile->IsWriteable() )
        return;

    CFile *targetStream = this->m_contentFile;
    CFile *registryStream = this->m_registryFile;

    eIMGArchiveVersion imgVersion = this->m_version;

    // Write things depending on version.
    {
        // Generate header meta information.
        headerGenPresence headerGenMetaData;
        headerGenMetaData.numOfFiles = 0;

        GenerateFileHeaderStructure( m_virtualFS.GetRootDir(), headerGenMetaData );

        // Generate the archive structure for files in this archive.
        archiveGenPresence genMetaData;
        genMetaData.currentBlockCount = 0;

        // If we are version two, then we prepend the file headers before the content blocks.
        // Take that into account.
        if ( targetStream == registryStream )   // this is a pretty weak check tbh. but it works for the most part.
        {
            size_t headerSize = 0;

            if (imgVersion == IMG_VERSION_2)
            {
                // First, there is a general header.
                headerSize += sizeof( generalHeader );
            }

            // Now come the file entries.
            size_t resourceFileHeaderSize = 0;

            if (imgVersion == IMG_VERSION_1)
            {
                resourceFileHeaderSize = sizeof(resourceFileHeader_ver1);
            }
            else if (imgVersion == IMG_VERSION_2)
            {
                resourceFileHeaderSize = sizeof(resourceFileHeader_ver2);
            }

            headerSize += resourceFileHeaderSize * headerGenMetaData.numOfFiles;

            // Add this block count to the allocation.
            genMetaData.currentBlockCount += getDataBlockCount( headerSize );
        }
        
        GenerateArchiveStructure( m_virtualFS.GetRootDir(), genMetaData );

        // Preallocate required file space.
        {
            fsOffsetNumber_t realFileSize = ( (fsOffsetNumber_t)genMetaData.currentBlockCount * IMG_BLOCK_SIZE );

            targetStream->SeekNative( realFileSize, SEEK_SET );
            targetStream->SetSeekEnd();

            targetStream->SeekNative( 0, SEEK_SET );
        }

        // We only write a header in version two archives.
        if ( imgVersion == IMG_VERSION_2 )
        {
            // Write the main header of the archive.
            generalHeader mainHeader;
            mainHeader.checksum = '2REV';
            mainHeader.numberOfEntries = (fsUInt_t)headerGenMetaData.numOfFiles;

            targetStream->WriteStruct( mainHeader );
        }

        // Write all file headers.
        WriteFileHeaders( registryStream, m_virtualFS.GetRootDir() );

        // Now write all the files.
        WriteFiles( targetStream, m_virtualFS.GetRootDir() );
    }

    // Clean up the compressed files, since we do not need them anymore
    // from here on.
    if ( CFileTranslator *compressRoot = this->m_compressRoot )
    {
        filePath compressRootPath;

        bool hasRootPath = compressRoot->GetFullPath( "@", false, compressRootPath );

        // Delete the handle to the compress root and NULL it.
        {
            delete compressRoot;

            this->m_compressRoot = NULL;
        }

        // Delete the disk contents.
        if ( hasRootPath )
        {
            CFileTranslator *fileRoot = this->GetFileRoot();

            if ( fileRoot )
            {
                fileRoot->Delete( compressRootPath.c_str() );
            }
        }
    }
}

void CIMGArchiveTranslator::SetCompressionHandler( CIMGArchiveCompressionHandler *handler )
{
    this->m_compressionHandler = handler;
}

bool CIMGArchiveTranslator::ReadArchive( void )
{
    // Load archive.
    bool hasFileCount = false;
    unsigned int fileCount = 0;

    if ( this->m_version == IMG_VERSION_2 )
    {
        bool hasReadFileCount = this->m_registryFile->ReadUInt( fileCount );

        if ( !hasReadFileCount )
        {
            return false;
        }

        hasFileCount = true;
    }

    bool successLoadingFiles = true;

    // Load every file block registration into memory.
    unsigned int n = 0;
    
    eIMGArchiveVersion theVersion = this->m_version;

    while ( true )
    {
        // Halting condition.
        if ( hasFileCount && n == fileCount )
        {
            break;
        }

        // Read the file header.
        union
        {
            resourceFileHeader_ver1 _ver1Header;
            resourceFileHeader_ver2 _ver2Header;
        };

        bool hasReadEntryHeader = false;

        if ( theVersion == IMG_VERSION_1 )
        {
            hasReadEntryHeader = this->m_registryFile->ReadStruct( _ver1Header );
        }
        else if ( theVersion == IMG_VERSION_2 )
        {
            hasReadEntryHeader = this->m_registryFile->ReadStruct( _ver2Header );
        }

        if ( !hasReadEntryHeader )
        {
            if ( hasFileCount )
            {
                successLoadingFiles = false;
            }

            // If we are version 1, we will probably halt here.
            break;
        }

        // Read from the header.
        unsigned long resourceSize = 0;
        unsigned long resourceOffset = 0;
        
        const char *namePointer = NULL;
        size_t maxName = 0;

        if ( theVersion == IMG_VERSION_1 )
        {
            resourceOffset = _ver1Header.offset;
            resourceSize = _ver1Header.fileDataSize;

            namePointer = _ver1Header.name;
            maxName = sizeof( _ver1Header.name );
        }
        else if ( theVersion == IMG_VERSION_2 )
        {
            resourceOffset = _ver2Header.offset;
            resourceSize =
                ( _ver2Header.expandedSize != 0 ) ? ( _ver2Header.expandedSize ) : ( _ver2Header.fileDataSize );

            namePointer = _ver2Header.name;
            maxName = sizeof( _ver2Header.name );
        }

        // Register this into our archive struct.
        dirTree directories;
        bool isFile;

        char newPath[ sizeof( _ver1Header.name ) + 1 ];

        if ( namePointer )
        {
            memcpy( newPath, namePointer, maxName );

            newPath[ sizeof( newPath ) - 1 ] = '\0';
        }
        else
        {
            newPath[ 0 ] = '\0';
        }

        bool validPath = this->GetRelativePathTree( newPath, directories, isFile );

        if ( validPath && isFile )
        {
            filePath justFileName = directories.back();
            directories.pop_back();

            // We do not care about file name conflicts, for now.
            // Otherwise this routine would be terribly slow.
            file *fileEntry = m_virtualFS.ConstructFile( directories, justFileName );

            // Try to create this file.
            // This operation will overwrite files if they are found double.
            if ( fileEntry )
            {
                fileEntry->metaData.blockOffset = resourceOffset;
                fileEntry->metaData.resourceSize = resourceSize;

                size_t maxFinalName = sizeof( fileEntry->metaData.resourceName );

                assert( maxFinalName == sizeof( newPath ) );

                memcpy( fileEntry->metaData.resourceName, newPath, maxFinalName );
                fileEntry->metaData.resourceName[ maxFinalName - 1 ] = '\0';
            }
        }

        // Increment current iter.
        n++;
    }

    if ( successLoadingFiles )
    {
        return true;
    }
    return false;
}