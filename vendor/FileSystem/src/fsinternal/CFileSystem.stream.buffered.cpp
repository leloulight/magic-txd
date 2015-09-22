/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.buffered.cpp
*  PURPOSE:     Buffered stream utilities for block-based streaming
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>

// Include internal header.
#include "CFileSystem.internal.h"

/*===================================================
    CBufferedStreamWrap

    An extension of the raw file that uses buffered IO.
    Since, in reality, hardware is sector based, the
    preferred way of writing data to disk is using buffers.

    Always prefer this class instead of CRawFile!
    Only use raw communication if you intend to put your
    own buffering!

    While a file stream is wrapped, the usage of the toBeWrapped
    pointer outside of the wrapper class leads to
    undefined behavior.

    I have not properly documented this buffered system yet.
    Until I have, change of this class is usually not permitted
    other than by me (in fear of breaking anything).
    
    Arguments:
        toBeWrapped - stream pointer that should be buffered
        deleteOnQuit - if true, toBeWrapped is deleted aswell
                       when this class is deleted

    Cool Ideas:
    -   Create more interfaces that wrap FileSystem streams
        so applying attributes to streams is a simple as
        wrapping a virtual class
===================================================*/

CBufferedStreamWrap::CBufferedStreamWrap( CFile *toBeWrapped, bool deleteOnQuit ) : underlyingStream( toBeWrapped )
{
    internalIOBuffer.AllocateStorage(
        systemCapabilities.GetSystemLocationSectorSize( *toBeWrapped->GetPath().c_str() )
    );

    fileSeek.SetHost( this );

    this->terminateUnderlyingData = deleteOnQuit;
}

CBufferedStreamWrap::~CBufferedStreamWrap( void )
{
    // Push any pending buffer operations onto disk space.
    SharedSliceSelectorManager( *this ).FlushBuffer();

    if ( terminateUnderlyingData == true )
    {
        delete underlyingStream;
    }

    underlyingStream = NULL;
}

template <typename callbackType, typename seekGenericType>
AINLINE void UpdateStreamedBufferPosition(
                CBufferedStreamWrap::bufferSeekPointer_t& bufOffset,
                seekGenericType& fileSeek,
                callbackType& cb )
{
    typedef CBufferedStreamWrap::seekType_t seekType_t;

    // Align the buffer so that the file seek is inside the buffer (begin of the reader slice)
    seekType_t newBufferOffset = ALIGN( fileSeek.Tell(), (seekType_t)1, cb.GetBufferAlignment() );

    // If we have to move the slice to another position.
    bool isNewOffset = ( newBufferOffset != bufOffset.offsetOfBufferOnFileSpace );

    if ( isNewOffset )
    {
        // Make sure pending write operations are finished.
        cb.FlushBuffer();

        // Update the buffer offset.
        // This should transfer down to the runtime.
        bufOffset.SetOffset( newBufferOffset );

        cb.UpdateBuffer();
    }
}

template <typename bufferAbstractType, typename callbackType, typename seekGenericType>
AINLINE void SelectBufferedSlice(
                bufferAbstractType *buffer,
                CBufferedStreamWrap::bufferSeekPointer_t& bufOffset,
                seekGenericType& fileSeek, size_t requestedReadCount,
                callbackType& cb )
{
    typedef CBufferedStreamWrap::seekSlice_t seekSlice_t;
    typedef CBufferedStreamWrap::seekType_t seekType_t;

    // If we do not want to read anything, quit right away.
    if ( requestedReadCount == 0 )
        return;

#ifdef FILESYSTEM_PERFORM_SANITY_CHECKS
    // Add simple error checking.
    // It could be fatal to the application to introduce an infinite loop here.
    unsigned int methodRepeatCount = 0;
#endif //FILESYSTEM_PERFORM_SANITY_CHECKS

repeatMethod:
#if FILESYSTEM_PERFORM_SANITY_CHECKS
    methodRepeatCount++;

    if ( methodRepeatCount == 6000000 )
        throw std::exception( "infinite buffered select repetition count" );
#endif //FILESYSTEM_PERFORM_SANITY_CHECKS

    // Do the actual logic.
    seekType_t localFileSeek = fileSeek.Tell();

    size_t bufferSize = cb.GetBufferSize();

    // Create the slices for the seeking operation.
    // We will collide them against each other.
    seekSlice_t readSlice( localFileSeek, requestedReadCount );
    seekSlice_t bufferSlice( bufOffset.offsetOfBufferOnFileSpace, bufferSize );

    seekSlice_t::eIntersectionResult intResult = readSlice.intersectWith( bufferSlice );

    // Make sure the content is prepared for the action.
    bool hasToRepeat = cb.ContentInvokation( buffer, localFileSeek, requestedReadCount, intResult );

    if ( hasToRepeat )
        goto repeatMethod;

    if ( intResult == seekSlice_t::INTERSECT_EQUAL )
    {
        cb.BufferedInvokation( buffer, 0, requestedReadCount );

        fileSeek.Seek( localFileSeek + requestedReadCount );
    }
    else if ( intResult == seekSlice_t::INTERSECT_INSIDE )
    {
        cb.BufferedInvokation( buffer, (size_t)( localFileSeek - bufOffset.offsetOfBufferOnFileSpace ), requestedReadCount );

        fileSeek.Seek( localFileSeek + requestedReadCount );
    }
    else if ( intResult == seekSlice_t::INTERSECT_BORDER_END )
    {
        // Everything read-able has to fit inside client memory.
        // A size_t is assumed to be as big as the client memory allows.
        size_t sliceStartOffset = (size_t)( bufferSlice.GetSliceStartPoint() - localFileSeek );
        
        // First read from the file natively, to reach the buffer border.
        if ( sliceStartOffset > 0 )
        {
            // Make sure the seek pointer is up to date.
            fileSeek.Update();

            size_t actualReadCount = 0;

            cb.NativeInvokation( buffer, sliceStartOffset, actualReadCount );

            // Update the file seek.
            fileSeek.Seek( localFileSeek += actualReadCount );

            // Predict that we advanced by some bytes.
            fileSeek.PredictNativeAdvance( (seekType_t)actualReadCount );
        }

        // Now lets read the remainder from the buffer.
        size_t sliceReadRemainderCount = (size_t)( requestedReadCount - sliceStartOffset );

        if ( sliceReadRemainderCount > 0 )
        {
            cb.BufferedInvokation( buffer + sliceStartOffset, 0, sliceReadRemainderCount );

            fileSeek.Seek( localFileSeek += sliceReadRemainderCount );
        }
    }
    else if ( intResult == seekSlice_t::INTERSECT_BORDER_START )
    {
        // The_GTA: That +1 is very complicated. Just roll with it!
        size_t sliceEndOffset = (size_t)( bufferSlice.GetSliceEndPoint() + 1 - localFileSeek );

        // Read what can be read from the native buffer.
        if ( sliceEndOffset > 0 )
        {
            size_t sliceReadInCount = (size_t)( bufferSize - sliceEndOffset );

            cb.BufferedInvokation( buffer, sliceReadInCount, sliceEndOffset );

            // Update the local file seek.
            fileSeek.Seek( localFileSeek += sliceEndOffset );
        }

        // Increment the buffer location and read the requested content into it.
        size_t sliceReadRemainderCount = (size_t)( requestedReadCount - sliceEndOffset );

        if ( sliceReadRemainderCount > 0 )
        {
            // Update the perform details.
            buffer += sliceEndOffset;
            requestedReadCount = sliceReadRemainderCount;

            goto repeatMethod;
        }
    }
    else if ( intResult == seekSlice_t::INTERSECT_ENCLOSING )
    {
        // Read the beginning segment, that is native file memory.
        size_t sliceStartOffset = (size_t)( bufferSlice.GetSliceStartPoint() - localFileSeek );

        if ( sliceStartOffset > 0 )
        {
            // Make sure the seek pointer is up-to-date.
            fileSeek.Update();

            size_t actualReadCount = 0;

            cb.NativeInvokation( buffer, sliceStartOffset, actualReadCount );

            // Update the seek ptr.
            fileSeek.Seek( localFileSeek += sliceStartOffset );

            // Predict that the real file offset advanced by some bytes.
            fileSeek.PredictNativeAdvance( (seekType_t)actualReadCount );
        }

        // Put the content of the entire internal buffer into the output buffer.
        {
            cb.BufferedInvokation( buffer + sliceStartOffset, 0, bufferSize );

            fileSeek.Seek( localFileSeek += bufferSize );
        }

        // Read the part after the internal buffer slice.
        // This part must be executed on the buffer context.
        size_t sliceEndOffset = (size_t)( readSlice.GetSliceEndPoint() - bufferSlice.GetSliceEndPoint() );

        if ( sliceEndOffset > 0 )
        {
            // Update execution parameters and continue to dispatch.
            buffer += sliceStartOffset + bufferSize;
            requestedReadCount = sliceEndOffset;

            goto repeatMethod;
        }
    }
    else if ( seekSlice_t::isFloatingIntersect( intResult ) || intResult == seekSlice_t::INTERSECT_UNKNOWN )
    {
		// Notify the callback about out-of-bounds content access.
		bool shouldContinue = cb.FloatingInvokation( buffer, localFileSeek, requestedReadCount, intResult );

		if ( shouldContinue )
		{
			// Update buffer contents depending on the stream position.
			UpdateStreamedBufferPosition(
				bufOffset,
				fileSeek,
				cb
			);

			// Attempt to repeat reading.
			goto repeatMethod;
		}
    }
    else
    {
        // We have no hit in any way that we can detect.
        // Throw an exception.
        assert( 0 );
    }
}

size_t CBufferedStreamWrap::Read( void *buffer, size_t sElement, size_t iNumElements )
{
    // If we are not opened for reading rights, this operation should not do anything.
    if ( !IsReadable() )
        return 0;

    ReadingSliceSelectorManager sliceMan( *this );

    // Perform a complex buffered logic.
    SelectBufferedSlice(
        (char*)buffer, this->bufOffset,
        this->fileSeek, sElement * iNumElements,
        sliceMan
    );

    return sliceMan.GetBytesRead();
}

size_t CBufferedStreamWrap::Write( const void *buffer, size_t sElement, size_t iNumElements )
{
    // If we are not opened for writing rights, this operation should not do anything.
    if ( !IsWriteable() )
        return 0;

    WritingSliceSelectorManager sliceMan( *this );

    // Perform the complex buffered logic.
    SelectBufferedSlice(
        (const char*)buffer, this->bufOffset,
        this->fileSeek, sElement * iNumElements,
        sliceMan
    );

    return sliceMan.GetBytesWritten();
}

int CBufferedStreamWrap::Seek( long iOffset, int iType )
{
    // Update the seek with a normal number.
    // This is a faster method than SeekNative.
    fsOffsetNumber_t offsetBase = 0;

    if ( iType == SEEK_CUR )
    {
        offsetBase = fileSeek.Tell();
    }
    else if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_END )
    {
        offsetBase = underlyingStream->GetSize();
    }

    fileSeek.Seek( (long)( offsetBase + iOffset ) );

    // Make sure the buffer is updated depending on a position change.
    // We do not have to do that.
    if ( false )
    {
        SharedSliceSelectorManager selector( *this );

        UpdateStreamedBufferPosition( this->bufOffset, this->fileSeek, selector );
    }
    return 0;
}

int CBufferedStreamWrap::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
    // Update the seek with a bigger number.
    seekType_t offsetBase = 0;

    if ( iType == SEEK_CUR )
    {
        offsetBase = fileSeek.Tell();
    }
    else if ( iType == SEEK_SET )
    {
        offsetBase = 0;
    }
    else if ( iType == SEEK_END )
    {
        offsetBase = (seekType_t)underlyingStream->GetSizeNative();
    }

    fileSeek.Seek( (fsOffsetNumber_t)( offsetBase + iOffset ) );

    // Make sure the buffer is updated depending on a position change.
    // We do not have to do that.
    // TODO: maybe add a property so the user can enable this? (commit of buffer on stream seek)
    if ( false )
    {
        SharedSliceSelectorManager selector( *this );

        UpdateStreamedBufferPosition( this->bufOffset, this->fileSeek, selector );
    }
    return 0;
}

long CBufferedStreamWrap::Tell( void ) const
{
    return (long)fileSeek.Tell();
}

fsOffsetNumber_t CBufferedStreamWrap::TellNative( void ) const
{
    return (fsOffsetNumber_t)fileSeek.Tell();
}

bool CBufferedStreamWrap::IsEOF( void ) const
{
    // Update the underlying stream's seek ptr and see if it finished.
    fileSeek.Update();

    return underlyingStream->IsEOF();
}

bool CBufferedStreamWrap::Stat( struct stat *stats ) const
{
    // Redirect this functionality to the underlying stream.
    // We are not supposed to modify any of these logical attributes.
    return underlyingStream->Stat( stats );
}

void CBufferedStreamWrap::PushStat( const struct stat *stats )
{
    // Attempt to modify the stream's meta data.
    underlyingStream->PushStat( stats );
}

void CBufferedStreamWrap::SetSeekEnd( void )
{
    // Finishes the stream at the given offset.
    fileSeek.Update();

    underlyingStream->SetSeekEnd();
}

size_t CBufferedStreamWrap::GetSize( void ) const
{
    return underlyingStream->GetSize();
}

fsOffsetNumber_t CBufferedStreamWrap::GetSizeNative( void ) const
{
    return underlyingStream->GetSizeNative();
}

void CBufferedStreamWrap::Flush( void )
{
    // Get the contents of our buffer onto disk space (if required).
    SharedSliceSelectorManager( *this ).FlushBuffer();

    // Write the remaining OS buffers.
    underlyingStream->Flush();
}

const filePath& CBufferedStreamWrap::GetPath( void ) const
{
    return underlyingStream->GetPath();
}

bool CBufferedStreamWrap::IsReadable( void ) const
{
    return underlyingStream->IsReadable();
}

bool CBufferedStreamWrap::IsWriteable( void ) const
{
    return underlyingStream->IsWriteable();
}

/*=========================================
    CBufferedFile

    Loads a file at open and keeps it in a managed buffer.

    Info: this is a deprecated class.
=========================================*/

CBufferedFile::~CBufferedFile( void )
{
    return;
}

size_t CBufferedFile::Read( void *pBuffer, size_t sElement, size_t iNumElements )
{
    size_t iReadElements = std::min( ( m_sSize - m_iSeek ) / sElement, iNumElements );
    size_t sRead = iReadElements * sElement;

    if ( iNumElements == 0 )
        return 0;

    memcpy( pBuffer, m_pBuffer + m_iSeek, sRead );
    m_iSeek += sRead;
    return iReadElements;
}

size_t CBufferedFile::Write( const void *pBuffer, size_t sElement, size_t iNumElements )
{
    // TODO: make a proper memory-mapped file.
    return 0;
}

int CBufferedFile::Seek( long iOffset, int iType )
{
    switch( iType )
    {
    case SEEK_SET:
        m_iSeek = 0;
        break;
    case SEEK_END:
        m_iSeek = m_sSize;
        break;
    }

    m_iSeek = std::max( (size_t)0, std::min( (size_t)( m_iSeek + iOffset ), m_sSize ) );
    return 0;
}

long CBufferedFile::Tell( void ) const
{
    return (long)m_iSeek;
}

bool CBufferedFile::IsEOF( void ) const
{
    return ( (size_t)m_iSeek == m_sSize );
}

bool CBufferedFile::Stat( struct stat *stats ) const
{
    stats->st_dev = -1;
    stats->st_ino = -1;
    stats->st_mode = -1;
    stats->st_nlink = -1;
    stats->st_uid = -1;
    stats->st_gid = -1;
    stats->st_rdev = -1;
    stats->st_size = (_off_t)m_sSize;
    stats->st_atime = 0;
    stats->st_ctime = 0;
    stats->st_mtime = 0;
    return 0;
}

void CBufferedFile::PushStat( const struct stat *stats )
{
    // Does not do anything.
    return;
}

size_t CBufferedFile::GetSize( void ) const
{
    return m_sSize;
}

void CBufferedFile::SetSeekEnd( void )
{
    return;
}

void CBufferedFile::Flush( void )
{
    return;
}

const filePath& CBufferedFile::GetPath( void ) const
{
    return m_path;
}

int CBufferedFile::ReadInt( void )
{
    int iResult;

    if ( ( m_sSize - m_iSeek ) < sizeof(int) )
        return 0;

    iResult = *(int*)( m_pBuffer + m_iSeek );
    m_iSeek += sizeof(int);
    return iResult;
}

short CBufferedFile::ReadShort( void )
{
    short iResult;

    if ( (m_sSize - m_iSeek) < sizeof(short) )
        return 0;

    iResult = *(short*)(m_pBuffer + m_iSeek);
    m_iSeek += sizeof(short);
    return iResult;
}

char CBufferedFile::ReadByte( void )
{
    if ( m_sSize == (size_t)m_iSeek )
        return 0;

    return *(m_pBuffer + m_iSeek++);
}

bool CBufferedFile::IsReadable( void ) const
{
    return true;
}

bool CBufferedFile::IsWriteable( void ) const
{
    return false;
}