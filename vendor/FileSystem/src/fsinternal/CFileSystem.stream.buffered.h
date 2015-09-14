/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.buffered.h
*  PURPOSE:     Buffered stream utilities for block-based streaming
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_BLOCK_BASED_STREAMING_
#define _FILESYSTEM_BLOCK_BASED_STREAMING_

// Define this macro if you want to debug FileSystem scenarios that should never be reached
// but if they are reached its the implementation's fault.
//#define FILESYSTEM_PERFORM_SANITY_CHECKS

class CBufferedStreamWrap : public CFile
{
public:
                        CBufferedStreamWrap( CFile *toBeWrapped, bool deleteOnQuit );
                        ~CBufferedStreamWrap( void );

    size_t              Read            ( void *buffer, size_t sElement, size_t iNumElements ) override;
    size_t              Write           ( const void *buffer, size_t sElement, size_t iNumElements ) override;
    int                 Seek            ( long iOffset, int iType ) override;
    int                 SeekNative      ( fsOffsetNumber_t iOffset, int iType ) override;
    long                Tell            ( void ) const override;
    fsOffsetNumber_t    TellNative      ( void ) const override;
    bool                IsEOF           ( void ) const override;
    bool                Stat            ( struct stat *stats ) const override;
    void                PushStat        ( const struct stat *stats ) override;
    void                SetSeekEnd      ( void ) override;
    size_t              GetSize         ( void ) const override;
    fsOffsetNumber_t    GetSizeNative   ( void ) const override;
    void                Flush           ( void ) override;
    const filePath&     GetPath         ( void ) const override;
    bool                IsReadable      ( void ) const override;
    bool                IsWriteable     ( void ) const override;

    template <typename unsignedType>
    static inline unsignedType unsignedBarrierSubtract( unsignedType one, unsignedType two, unsignedType barrier )
    {
        if ( ( one + barrier ) < two )
        {
            return barrier;
        }

        return one - two;
    }

    // Type of the number for seeking operations.
    // Use this number sparsely as operations are slow on it.
    typedef fsOffsetNumber_t seekType_t;

    // Slice that should be used on buffered logic.
    typedef sliceOfData <seekType_t> seekSlice_t;

    template <typename bufType, typename numberType>
    class bufferedStorage
    {
        numberType storageSize;
        numberType actualFillCount;
        bufType *storagePtr;
        bool contentChanged;

    public:
        inline bufferedStorage( void )
        {
            this->storageSize = 0;
            this->actualFillCount = 0;
            this->storagePtr = NULL;
            this->contentChanged = false;
        }

        inline ~bufferedStorage( void )
        {
            // Free any used memory.
            DeleteStorage();
        }

        inline void DeleteStorage( void )
        {
            if ( storagePtr )
            {
                delete [] storagePtr;

                storagePtr = NULL;
            }
        }

        inline bool HasChanged( void ) const
        {
            return contentChanged && actualFillCount != 0;
        }

        inline void SetChanged( bool hasChanged )
        {
            contentChanged = hasChanged;
        }

        inline void AllocateStorage( numberType storageSize )
        {
            DeleteStorage();

            storagePtr = new bufType[ (size_t)storageSize ];

            this->storageSize = storageSize;

            // Restore content properties.
            SetFillCount( 0 );
            SetChanged( false );
        }

        // Used when the data is filled manually.
        inline void SetFillCount( numberType val )
        {
            bool hasChanged = ( val != this->actualFillCount );

            this->actualFillCount = val;

            if ( val == 0 )
            {
                SetChanged( false );
            }
            else if ( hasChanged )
            {
                SetChanged( true );
            }
        }

        inline numberType GetFillCount( void ) const
        {
            return this->actualFillCount;
        }

        inline void PutItem( size_t offset, bufType dataItem )
        {
            *( storagePtr + offset ) = dataItem;
        }

        inline numberType GetStorageSize( void ) const
        {
            return this->storageSize;
        }

        inline numberType GetDataOperationSliceCount( numberType sequenceOffset ) const
        {
            return unsignedBarrierSubtract( storageSize, sequenceOffset, (numberType)0 );
        }

        inline void SetDataSequence( numberType sequenceOffset, const bufType dataArray[], numberType writeDataCount, numberType& writtenDataCount )
        {
            // Obtain the count we are allowed to operate on.
            // We can only write positive amounts.
            numberType canWriteCount = std::min( GetDataOperationSliceCount( sequenceOffset ), writeDataCount );

            // Write the actual data.
            if ( canWriteCount > 0 )
            {
                std::copy( dataArray, dataArray + canWriteCount, storagePtr + sequenceOffset );
            }

            // Pass the amount of written data back to the calling runtime.
            writtenDataCount = canWriteCount;
        }

        inline void GetDataSequence( numberType sequenceOffset, bufType dataArray[], numberType readDataCount, numberType& actualReadDataCount ) const
        {
            // Get the amount of data we can read.
            numberType canReadCount = std::min( GetDataOperationSliceCount( sequenceOffset ), readDataCount );

            // Read the data from the buffer.
            if ( canReadCount > 0 )
            {
                const bufType *dataSource = storagePtr + sequenceOffset;

                std::copy( dataSource, dataSource + canReadCount, dataArray );
            }

            // Pass the amount of read data back to the callee.
            actualReadDataCount = canReadCount;
        }

        inline void FillWithFileSector( CFile *srcFile )
        {
            this->actualFillCount = srcFile->Read(
                this->storagePtr,
                1,
                (size_t)this->storageSize
            );

            // As we are directly the file content, it cannot have been changed.
            SetChanged( false );
        }

        inline bool FileSectorCompletion( CFile *srcFile, size_t completeTo, CBufferedStreamWrap& host, bool forceCompletion = true )
        {
            bool hasChanged = false;

            // Makes sure the buffer is filled below the offset "completeTo".
            if ( (numberType)completeTo > this->actualFillCount )
            {
                assert( (numberType)completeTo <= this->storageSize );

                // Store the original file seek.
                seekType_t origSeekPos = host.fileSeek.Tell();

                // Seek to the correct position for filling the buffer.
                host.fileSeek.Seek( host.bufOffset.offsetOfBufferOnFileSpace + this->actualFillCount );
                host.fileSeek.Update();

                unsigned long needToRead = (unsigned long)( completeTo - this->actualFillCount );

                size_t haveReadCount = srcFile->Read(
                    this->storagePtr + this->actualFillCount,
                    sizeof( bufType ),
                    needToRead
                );

                // Since we read some stuff, we need to advance the native seek pointer.
                if ( haveReadCount != 0 )
                {
                    host.fileSeek.PredictNativeAdvance( haveReadCount );
                }

                // Restore file seek.
                host.fileSeek.Seek( origSeekPos );

				if ( forceCompletion )
				{
					// Fill the remaining bytes (that are missing from the stream) with zero.
					if ( needToRead != haveReadCount )
					{
						memset( this->storagePtr + this->actualFillCount + haveReadCount, 0, needToRead - haveReadCount );
					}

					// This routine must complete the buffer to this point.
					// If it failed retrieving bytes from the stream, it is filling them with zero.
					this->actualFillCount = completeTo;

					// We always change.
					hasChanged = true;
				}
				else
				{
					// This routine only has to read what is can.
					// If there is nothing in the file, it could have read zero.
					this->actualFillCount += haveReadCount;

					// We only change it we actually read something.
					hasChanged = ( haveReadCount != 0 );
				}
            }

            return hasChanged;
        }

        inline void PushToFile( CFile *dstFile )
        {
            dstFile->Write(
                this->storagePtr,
                1,
                (size_t)this->actualFillCount
            );
        }
    };

    // Type of the buffer that should be used to cache data for
    // reading and writing operations.
    typedef bufferedStorage <char, seekType_t> streamBuffer_t;

private:
    inline bool IsSeekInsideBufferSpace( seekType_t testOffset, size_t& bufferOffset ) const
    {
        bool success =  ( testOffset >= this->bufOffset.offsetOfBufferOnFileSpace ) &&
                        ( testOffset - this->bufOffset.offsetOfBufferOnFileSpace < this->internalIOBuffer.GetStorageSize() );

        if ( success )
        {
            bufferOffset = (size_t)( testOffset - this->bufOffset.offsetOfBufferOnFileSpace );
        }

        return success;
    }

    inline bool IsSeekInsideBufferSpace_Clamped( seekType_t testOffset, size_t& bufferOffset ) const
    {
        bufferOffset = (size_t)(
            std::max( this->bufOffset.offsetOfBufferOnFileSpace,
                std::min( this->bufOffset.offsetOfBufferOnFileSpace + this->internalIOBuffer.GetStorageSize(),
                    testOffset
                )
            ) - this->bufOffset.offsetOfBufferOnFileSpace
        );

        return true;
    }

    struct SharedSliceSelectorManager
    {
        CBufferedStreamWrap& host;

        AINLINE SharedSliceSelectorManager( CBufferedStreamWrap& host ) : host( host )
        {
            return;
        }

        AINLINE bool AllocateBufferSize( size_t reqSize )
        {
            bool hasChanged = false;

            // Get the actual required size (validated)
            size_t validReqSize = std::min( reqSize, (size_t)host.internalIOBuffer.GetStorageSize() );

            // Set up the internalIOBuffer from the host to the required size.
            // Unused fields must be filled with zero.
            size_t nativeBufferSize = GetBufferSize();

            if ( nativeBufferSize < validReqSize )
            {
                for ( size_t n = nativeBufferSize; n < validReqSize; n++ )
                {
                    host.internalIOBuffer.PutItem( n, 0 );
                }

                host.internalIOBuffer.SetFillCount( validReqSize );

                hasChanged = true;
            }

            return hasChanged;
        }

        AINLINE size_t GetBufferSize( void )
        {
            return (size_t)host.internalIOBuffer.GetFillCount();
        }

        AINLINE void FlushBuffer( void )
        {
            // Has the buffer content been updated at all?
            if ( host.IsWriteable() && host.internalIOBuffer.HasChanged() )
            {
                // Push buffer contents to disk.
                seekType_t currentFileSeek = host.fileSeek.Tell();

                host.fileSeek.Seek( host.bufOffset.offsetOfBufferOnFileSpace );
                host.fileSeek.Update();

                host.internalIOBuffer.PushToFile( host.underlyingStream );

                // We do not know how much we actually wrote to the file.
                // This is why we invalidate the native ptr.
                host.fileSeek.InvalidateNativePtr();

                // Restore the original file seek.
                host.fileSeek.Seek( currentFileSeek );

                // We are not updated anymore.
                host.internalIOBuffer.SetChanged( false );
            }
        }

        AINLINE void UpdateBuffer( void )
        {
            // Reset the buffer.
            // This is done because we dynamically update the buffer using content invokation.
            host.internalIOBuffer.SetFillCount( 0 );
        }

        AINLINE seekType_t GetBufferAlignment( void )
        {
            return host.internalIOBuffer.GetStorageSize();
        }

		AINLINE bool FloatingInvokation( const char *buffer, seekType_t readOffset, seekType_t readCount, seekSlice_t::eIntersectionResult intResult )
		{
			bool canContinue = true;

			if ( intResult == seekSlice_t::INTERSECT_FLOATING_END )
			{
				// If the reading is inside the buffer but it has not been filled to the required count,
				// then we terminate.
				seekType_t awarenessOffset = ( readOffset - host.bufOffset.offsetOfBufferOnFileSpace );

				canContinue = ( awarenessOffset >= host.internalIOBuffer.GetStorageSize() );
			}
            else if ( intResult == seekSlice_t::INTERSECT_UNKNOWN )
            {
                // If we have no buffer size, it means we have read nothing and could establish no content.
                // This means we terminate.
                if ( GetBufferSize() == 0 )
                {
                    canContinue = false;
                }
            }

			return canContinue;
		}
    };

    struct ReadingSliceSelectorManager : public SharedSliceSelectorManager
    {
        size_t actualReadCount;

        AINLINE ReadingSliceSelectorManager( CBufferedStreamWrap& hostClass ) : SharedSliceSelectorManager( hostClass )
        {
            actualReadCount = 0;
        }

        AINLINE void BufferedInvokation( char *targetBuf, size_t bufferOffset, size_t readCount )
        {
            seekType_t actualReadBytes = 0;

            host.internalIOBuffer.GetDataSequence( bufferOffset, targetBuf, readCount, actualReadBytes );

            assert( actualReadBytes == readCount );

            actualReadCount += readCount;
        }

        AINLINE void NativeInvokation( char *targetBuf, size_t readCount, size_t& realReadBytes )
        {
            size_t actualReadBytes = host.underlyingStream->Read( targetBuf, 1, readCount );

            realReadBytes = actualReadBytes;

            actualReadCount += actualReadBytes;
        }

        AINLINE bool ContentInvokation( char *targetBuf, seekType_t readOffset, seekType_t readCount, seekSlice_t::eIntersectionResult intResult )
        {
            // In here we want to profit the reading of the requested file data.
            // The buffer must be aligned properly prior to this call on the underlyingStream.

            bool hasToRepeat = false;

            // todo: what if "readOffset + readCount" overshoots the buffer?
            // theory: still allow it.
            seekType_t allocateTo = readOffset + readCount;

            if ( intResult == seekSlice_t::INTERSECT_BORDER_START ||
                 intResult == seekSlice_t::INTERSECT_FLOATING_END )
            {
                size_t localReadOffset;

                if ( host.IsSeekInsideBufferSpace_Clamped( allocateTo, localReadOffset ) )
                {
                    host.fileSeek.Update();

                    hasToRepeat = host.internalIOBuffer.FileSectorCompletion(
                        host.underlyingStream, localReadOffset, host, false
                    );
                }
            }
            else if ( intResult == seekSlice_t::INTERSECT_UNKNOWN )
            {
                // Handle error cases. These are quite common.

                // Has the buffer no size?
                if ( GetBufferSize() == 0 )
                {
                    // In that case, we must create room for data operations.
                    // First we try to push as much to-be-read data into the buffer.
                    // We assume that the user wants to read as much of the file as possible, so this acts as a cache.
                    if ( !host.bufOffset.HasBeenInitialized() && !host.internalIOBuffer.HasChanged() )
                    {
                        // Purge read the file sector into memory.
                        seekType_t actualFileSeek = host.fileSeek.Tell();

                        host.fileSeek.Seek( host.bufOffset.offsetOfBufferOnFileSpace );
                        host.fileSeek.Update();

                        host.internalIOBuffer.FillWithFileSector(
                            host.underlyingStream
                        );

                        // We have read as many bytes as are in the buffer now.
                        // This is why the underlying stream should have advanced by the content count (fill count).
                        host.fileSeek.PredictNativeAdvance( host.internalIOBuffer.GetFillCount() );

                        host.fileSeek.Seek( actualFileSeek );

                        host.bufOffset.SetInitialized();

                        hasToRepeat = true;
                    }
                    else
                    {
                        // Make sure that we have allocated enough buffer space for our operation.
                        size_t localReadOffset;
                        
                        if ( host.IsSeekInsideBufferSpace_Clamped( allocateTo, localReadOffset ) )
                        {
                            // We should only do work if the buffer is not filled up to "localWriteOffset".
                            // This condition has to be checked by the "FileSectorCompletion" method.
                            hasToRepeat = host.internalIOBuffer.FileSectorCompletion(
                                host.underlyingStream, localReadOffset, host, false
                            );
                        }
                    }
                }
            }

            return hasToRepeat;
        }

        AINLINE size_t GetBytesRead( void )
        {
            return actualReadCount;
        }
    };

    struct WritingSliceSelectorManager : public SharedSliceSelectorManager
    {
        size_t actualWriteCount;

        AINLINE WritingSliceSelectorManager( CBufferedStreamWrap& hostClass ) : SharedSliceSelectorManager( hostClass )
        {
            actualWriteCount = 0;
        }

        AINLINE void BufferedInvokation( const char *sourceBuf, size_t bufferOffset, size_t writeCount )
        {
            seekType_t realWriteCount = 0;

            // Write to the buffer.
            host.internalIOBuffer.SetDataSequence( bufferOffset, sourceBuf, writeCount, realWriteCount );

            assert( realWriteCount == writeCount );

            actualWriteCount += writeCount;
        }

        AINLINE void NativeInvokation( const char *sourceBuf, size_t writeCount, size_t& realWriteCount )
        {
            // Write to the file stream directly.
            size_t actualWriteCount = host.underlyingStream->Write( sourceBuf, 1, writeCount );

            realWriteCount = actualWriteCount;

            actualWriteCount += actualWriteCount;
        }

        AINLINE bool ContentInvokation( const char *sourceBuf, seekType_t writeOffset, seekType_t writeCount, seekSlice_t::eIntersectionResult intResult )
        {
            // If this function is called, we assume the buffer offset has been properly adjusted to the buffer sector.
            // We want to prepare the work on a sector in this routine.
            size_t localWriteOffset = 0;

            bool hasToRepeat = false;

            if ( intResult == seekSlice_t::INTERSECT_BORDER_START ||
                 intResult == seekSlice_t::INTERSECT_ENCLOSING && writeOffset >= host.bufOffset.offsetOfBufferOnFileSpace )
            {
                // To optimize writing operations, we extend the buffer to the maximally allowed size.
                if ( host.IsSeekInsideBufferSpace_Clamped( writeOffset + writeCount, localWriteOffset ) )
                {
                    // Notify the selector method to repeat if we modified the internal I/O buffer.
                    hasToRepeat = AllocateBufferSize( localWriteOffset );
                }
            }
            else if ( intResult == seekSlice_t::INTERSECT_FLOATING_END )
            {
                // We only care about the workload if it is not too far from the buffer.
                if ( host.IsSeekInsideBufferSpace_Clamped( writeOffset, localWriteOffset ) )
                {
                    // The work request is past the end of the buffer filling.
                    // We check that there is a gap between buffer fill end and the write start.
                    // If so, we fill the bytes with file data in that gap.

                    // We should only do work if the buffer is not filled up to "localWriteOffset".
                    // This condition has to be checked by the "FileSectorCompletion" method.
                    hasToRepeat = host.internalIOBuffer.FileSectorCompletion(
                        host.underlyingStream, localWriteOffset, host
                    );

                    // Allocate space to write things into for next pass.
                    bool maybeRepeat = AllocateBufferSize( localWriteOffset + (size_t)writeCount );

                    if ( maybeRepeat )
                    {
                        hasToRepeat = true;
                    }
                }
            }
            else if ( intResult == seekSlice_t::INTERSECT_UNKNOWN )
            {
                // Handle error cases. These are quite common.

                // Has the buffer no size?
                if ( GetBufferSize() == 0 )
                {
                    // In that case, we must create room for data operations.
                    // Make sure that we have allocated enough buffer space for our operation.
                    if ( host.IsSeekInsideBufferSpace_Clamped( writeOffset, localWriteOffset ) )
                    {
                        // We should only do work if the buffer is not filled up to "localWriteOffset".
                        // This condition has to be checked by the "FileSectorCompletion" method.
                        hasToRepeat = host.internalIOBuffer.FileSectorCompletion(
                            host.underlyingStream, localWriteOffset, host
                        );

                        // Allocate space to write things into for next pass.
                        bool maybeRepeat = AllocateBufferSize( localWriteOffset + (size_t)writeCount );

                        if ( maybeRepeat )
                        {
                            hasToRepeat = true;
                        }
                    }
                }
            }

            return hasToRepeat;
        }

        AINLINE size_t GetBufferSize( void )
        {
            return (size_t)host.internalIOBuffer.GetFillCount();
        }

        AINLINE size_t GetBytesWritten( void )
        {
            return actualWriteCount;
        }
    };

    // The idea behind this class is:
    // Cache the logical stream position, so we avoid system calls when unnecessary.
    // TODO: update this class in the future when FileSystem exceptions are designed.
    struct virtualStreamSeekPtr
    {
        seekType_t seekPtr;         // number containing the current seek into the stream.
        seekType_t nativePtr;       // number that has the actual stream seek pointer (as predicted)
        CBufferedStreamWrap *host;  // host class
        bool isNativePtrValid;      // boolean whether the native pointer can be traced

        inline virtualStreamSeekPtr( void )
        {
            // Remember to set the host inside of the constructor.
            host = NULL;

            seekPtr = 0;    // while host == NULL, this is invalid.
            nativePtr = 0;  // same as seekPtr.

            isNativePtrValid = false;
        }

        inline ~virtualStreamSeekPtr( void )
        {
            return;
        }

        inline void SetHost( CBufferedStreamWrap *hostStream )
        {
            this->host = hostStream;
            this->seekPtr = hostStream->underlyingStream->TellNative(); // validate the field.
            this->nativePtr = this->seekPtr;
            this->isNativePtrValid = true;
        }

        inline void Seek( seekType_t totalOffset )
        {
            // Has the seek changed at all?
            if ( totalOffset == seekPtr )
                return;

            // Modify the local seek descriptor.
            seekPtr = totalOffset;

            // the update flag is decided automatically.
        }

        inline bool NeedsUpdating( void ) const
        {
            return ( isNativePtrValid == false ) || ( Tell() != nativePtr );
        }

        inline seekType_t Tell( void ) const
        {
            return seekPtr;
        }

        inline void InvalidateNativePtr( void )
        {
            isNativePtrValid = false;
        }

        inline void PredictNativeAdvance( seekType_t advanceBy )
        {
            // Increase the native seek ptr by some value.
            nativePtr += advanceBy;
        }

        inline void Update( void )
        {
            // Check whether we have to update.
            if ( NeedsUpdating() == false )
                return;

            // Push the local seek pointer into the device handle.
            int iResult = host->underlyingStream->SeekNative( seekPtr, SEEK_SET );

            if ( iResult == 0 )
            {
                // We do not have to update anymore.
                nativePtr = seekPtr;

                isNativePtrValid = true;
            }
            else
            {
                // The native pointer is invalid if the seeking failed.
                isNativePtrValid = false;
            }
        }
    };

public:
    // Pointer to the underlying stream that has to be buffered.
    CFile *underlyingStream;

    // If true, underlyingStream is terminated when this buffered file terminates.
    bool terminateUnderlyingData;

    streamBuffer_t internalIOBuffer;

    // Location of the buffer on the file-space.
    struct bufferSeekPointer_t
    {
        AINLINE bufferSeekPointer_t( void )
        {
            offsetOfBufferOnFileSpace = 0;
            bufferHasBeenInitialized = false;
        }

        AINLINE bool HasBeenInitialized( void )
        {
            return bufferHasBeenInitialized;
        }

        AINLINE void SetOffset( seekType_t newOffset )
        {
            if ( offsetOfBufferOnFileSpace != newOffset )
            {
                offsetOfBufferOnFileSpace = newOffset;

                bufferHasBeenInitialized = false;
            }
        }

        AINLINE void SetInitialized( void )
        {
            bufferHasBeenInitialized = true;
        }

        seekType_t offsetOfBufferOnFileSpace;
        bool bufferHasBeenInitialized;
    };
    bufferSeekPointer_t bufOffset;

private:
    // 64bit number that is the file's seek ptr.
    mutable virtualStreamSeekPtr fileSeek;
};

class CBufferedFile : public CFile
{
public:
                    ~CBufferedFile  ( void );

    size_t          Read            ( void *buffer, size_t sElement, size_t iNumElements ) override;
    size_t          Write           ( const void *buffer, size_t sElement, size_t iNumElements ) override;
    int             Seek            ( long iOffset, int iType ) override;
    long            Tell            ( void ) const override;
    bool            IsEOF           ( void ) const override;
    bool            Stat            ( struct stat *stats ) const override;
    void            PushStat        ( const struct stat *stats ) override;
    void            SetSeekEnd      ( void ) override;
    size_t          GetSize         ( void ) const override;
    void            Flush           ( void ) override;
    const filePath& GetPath         ( void ) const override;
    bool            IsReadable      ( void ) const override;
    bool            IsWriteable     ( void ) const override;

    int             ReadInt();
    short           ReadShort();
    char            ReadByte();

private:
    char*           m_pBuffer;
    size_t          m_iSeek;
    size_t          m_sSize;
    filePath        m_path;
};

#endif //_FILESYSTEM_BLOCK_BASED_STREAMING_