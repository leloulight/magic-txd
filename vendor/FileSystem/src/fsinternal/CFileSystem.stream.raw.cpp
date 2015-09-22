/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.raw.cpp
*  PURPOSE:     Raw OS filesystem file link
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>

// Include internal header.
#include "CFileSystem.internal.h"

enum eNumberConversion
{
    NUMBER_LITTLE_ENDIAN,
    NUMBER_BIG_ENDIAN
};

// Utilities for number splitting.
static inline unsigned int CalculateNumberSplitCount( size_t toBeSplitNumberSize, size_t nativeNumberSize )
{
    return (unsigned int)( toBeSplitNumberSize / nativeNumberSize );
}

template <typename nativeNumberType, typename splitNumberType>
static inline nativeNumberType* GetNumberSector(
            splitNumberType& numberToBeSplit,
            unsigned int index,
            eNumberConversion splitConversion, eNumberConversion nativeConversion )
{
    // Extract the sector out of numberToBeSplit.
    nativeNumberType& nativeNumberPartial = *( (nativeNumberType*)&numberToBeSplit + index );

    return &nativeNumberPartial;
}

template <typename splitNumberType, typename nativeNumberType>
AINLINE void SplitIntoNativeNumbers(
            splitNumberType numberToBeSplit,
            nativeNumberType *nativeNumbers,
            unsigned int maxNativeNumbers, unsigned int& nativeCount,
            eNumberConversion toBeSplitConversion, eNumberConversion nativeConversion )
{
    // todo: add endian-ness support.
    assert( toBeSplitConversion == nativeConversion );

    // We assume a lot of things here.
    // - a binary number system
    // - endian integer system
    // else this routine will produce garbage.

    // On Visual Studio 2008, this routine optimizes down completely into compiler intrinsics.

    const size_t nativeNumberSize = sizeof( nativeNumberType );
    const size_t toBeSplitNumberSize = sizeof( splitNumberType );

    // Calculate the amount of numbers we can fit into the array.
    unsigned int splitNumberCount = std::min( CalculateNumberSplitCount( toBeSplitNumberSize, nativeNumberSize ), maxNativeNumbers );
    unsigned int actualWriteCount = 0;

    for ( unsigned int n = 0; n < splitNumberCount; n++ )
    {
        // Write it into the array.
        nativeNumbers[ actualWriteCount++ ] = *GetNumberSector <nativeNumberType> ( numberToBeSplit, n, toBeSplitConversion, nativeConversion );
    }

    // Notify the runtime about how many numbers we have successfully written.
    nativeCount = actualWriteCount;
}

template <typename splitNumberType, typename nativeNumberType>
AINLINE void ConvertToWideNumber(
            splitNumberType& wideNumberOut,
            nativeNumberType *nativeNumbers, unsigned int numNativeNumbers,
            eNumberConversion wideConversion, eNumberConversion nativeConversion )
{
    // todo: add endian-ness support.
    assert( wideConversion == nativeConversion );

    // we assume the same deals as SplitIntoNativeNumbers.
    // else this routine is garbage.

    // On Visual Studio 2008, this routine optimizes down completely into compiler intrinsics.

    const size_t nativeNumberSize = sizeof( nativeNumberType );
    const size_t toBeSplitNumberSize = sizeof( splitNumberType );

    // Calculate the amount of numbers we need to put together.
    unsigned int splitNumberCount = CalculateNumberSplitCount( toBeSplitNumberSize, nativeNumberSize );

    for ( unsigned int n = 0; n < splitNumberCount; n++ )
    {
        // Write it into the number.
        nativeNumberType numberToWrite = (nativeNumberType)0;

        if ( n < numNativeNumbers )
        {
            numberToWrite = nativeNumbers[ n ];
        }

        *GetNumberSector <nativeNumberType> ( wideNumberOut, n, wideConversion, nativeConversion ) = numberToWrite;
    }
}

/*===================================================
    CRawFile

    This class represents a file on the system.
    As long as it is present, the file is opened.

    WARNING: using this class directly is discouraged,
        as it uses direct methods of writing into
        hardware. wrap it with CBufferedStreamWrap instead!

    fixme: Port to mac
===================================================*/

CRawFile::CRawFile( const filePath& absFilePath )
{
    m_path = absFilePath;
}

CRawFile::~CRawFile( void )
{
#ifdef _WIN32
    CloseHandle( m_file );
#elif defined(__linux__)
    fclose( m_file );
#endif //OS DEPENDANT CODE
}

inline void _64bit_security_check( size_t sElement, size_t iNumElements )
{
    if ( sizeof( size_t ) >= 4 )
    {
        assert( sElement * iNumElements < 0x100000000 );
    }
}

size_t CRawFile::Read( void *pBuffer, size_t sElement, size_t iNumElements )
{
    _64bit_security_check( sElement, iNumElements );

#ifdef _WIN32
    DWORD dwBytesRead;

    if (sElement == 0 || iNumElements == 0)
        return 0;

    BOOL readComplete = ReadFile(m_file, pBuffer, (DWORD)( sElement * iNumElements ), &dwBytesRead, NULL);

    if ( readComplete == FALSE )
        return 0;

    return dwBytesRead / sElement;
#elif defined(__linux__)
    return fread( pBuffer, sElement, iNumElements, m_file );
#else
    return 0;
#endif //OS DEPENDANT CODE
}

size_t CRawFile::Write( const void *pBuffer, size_t sElement, size_t iNumElements )
{
    _64bit_security_check( sElement, iNumElements );

#ifdef _WIN32
    DWORD dwBytesWritten;

    if (sElement == 0 || iNumElements == 0)
        return 0;

    BOOL writeComplete = WriteFile(m_file, pBuffer, (DWORD)( sElement * iNumElements ), &dwBytesWritten, NULL);

    if ( writeComplete == FALSE )
        return 0;

    return dwBytesWritten / sElement;
#elif defined(__linux__)
    return fwrite( pBuffer, sElement, iNumElements, m_file );
#else
    return 0;
#endif //OS DEPENDANT CODE
}

int CRawFile::Seek( long iOffset, int iType )
{
#ifdef _WIN32
    if (SetFilePointer(m_file, iOffset, NULL, iType) == INVALID_SET_FILE_POINTER)
        return -1;
    return 0;
#elif defined(__linux__)
    return fseek( m_file, iOffset, iType );
#else
    return -1;
#endif //OS DEPENDANT CODE
}

int CRawFile::SeekNative( fsOffsetNumber_t iOffset, int iType )
{
#ifdef _WIN32
    // Split our offset into two DWORDs.
    LONG numberParts[ 2 ];
    unsigned int splitCount = 0;

    SplitIntoNativeNumbers( iOffset, numberParts, NUMELMS(numberParts), splitCount, NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    // Tell the OS.
    // Using the preferred method.
    DWORD resultVal = INVALID_SET_FILE_POINTER;

    if ( splitCount == 1 )
    {
        resultVal = SetFilePointer( this->m_file, numberParts[0], NULL, iType );
    }
    else if ( splitCount >= 2 )
    {
        resultVal = SetFilePointer( this->m_file, numberParts[0], &numberParts[1], iType );
    }

    if ( resultVal == INVALID_SET_FILE_POINTER )
        return -1;

    return 0;
#elif defined(__linux__)
    // todo.
#else
    return -1;
#endif //OS DEPENDANT CODE
}

long CRawFile::Tell( void ) const
{
#ifdef _WIN32
    LARGE_INTEGER posToMoveTo;
    posToMoveTo.LowPart = 0;
    posToMoveTo.HighPart = 0;

    LARGE_INTEGER currentPos;

    BOOL success = SetFilePointerEx( this->m_file, posToMoveTo, &currentPos, FILE_CURRENT );

    if ( success == FALSE )
        return -1;

    return (long)( currentPos.LowPart );
#elif defined(__linux__)
    return ftell( m_file );
#else
    return -1;
#endif //OS DEPENDANT CODE
}

fsOffsetNumber_t CRawFile::TellNative( void ) const
{
#ifdef _WIN32
    LARGE_INTEGER posToMoveTo;
    posToMoveTo.LowPart = 0;
    posToMoveTo.HighPart = 0;

    union
    {
        LARGE_INTEGER currentPos;
        DWORD currentPos_split[ sizeof( LARGE_INTEGER ) / sizeof( DWORD ) ];
    };

    BOOL success = SetFilePointerEx( this->m_file, posToMoveTo, &currentPos, FILE_CURRENT );

    if ( success == FALSE )
        return (fsOffsetNumber_t)0;

    // Create a FileSystem number.
    fsOffsetNumber_t resultNumber = (fsOffsetNumber_t)0;

    ConvertToWideNumber( resultNumber, &currentPos_split[0], NUMELMS(currentPos_split), NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    return resultNumber;
#elif defined(__linux__)
    // todo.
#else
    return (fsOffsetNumber_t)0;
#endif //OS DEPENDANT CODE
}

bool CRawFile::IsEOF( void ) const
{
#ifdef _WIN32
    // Check that the current file seek is beyond or equal the maximum size.
    return this->TellNative() >= this->GetSizeNative();
#elif defined(__linux__)
    return feof( m_file );
#else
    return false;
#endif //OS DEPENDANT CODE
}

bool CRawFile::Stat( struct stat *pFileStats ) const
{
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION info;

    if (!GetFileInformationByHandle( m_file, &info ))
        return false;

    pFileStats->st_size = info.nFileSizeLow;
    pFileStats->st_dev = info.nFileIndexLow;
    pFileStats->st_atime = info.ftLastAccessTime.dwLowDateTime;
    pFileStats->st_ctime = info.ftCreationTime.dwLowDateTime;
    pFileStats->st_mtime = info.ftLastWriteTime.dwLowDateTime;
    pFileStats->st_dev = info.dwVolumeSerialNumber;
    pFileStats->st_mode = 0;
    pFileStats->st_nlink = (unsigned short)info.nNumberOfLinks;
    pFileStats->st_rdev = 0;
    pFileStats->st_gid = 0;
    return true;
#elif __linux
    return fstat( fileno( m_file ), pFileStats ) != 0;
#else
    return false;
#endif //OS DEPENDANT CODE
}

#ifdef _WIN32
inline static void TimetToFileTime( time_t t, LPFILETIME pft )
{
    LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD) ll;
    pft->dwHighDateTime = ll >>32;
}
#endif //_WIN32

void CRawFile::PushStat( const struct stat *stats )
{
#ifdef _WIN32
    FILETIME ctime;
    FILETIME atime;
    FILETIME mtime;

    TimetToFileTime( stats->st_ctime, &ctime );
    TimetToFileTime( stats->st_atime, &atime );
    TimetToFileTime( stats->st_mtime, &mtime );

    SetFileTime( m_file, &ctime, &atime, &mtime );
#elif defined(__linux__)
    struct utimbuf timeBuf;
    timeBuf.actime = stats->st_atime;
    timeBuf.modtime = stats->st_mtime;

    utime( m_path.c_str(), &timeBuf );
#endif //OS DEPENDANT CODE
}

void CRawFile::SetSeekEnd( void )
{
#ifdef _WIN32
    SetEndOfFile( m_file );
#elif defined(__linux__)
    ftruncate64( fileno( m_file ), ftello64( m_file ) );
#endif //OS DEPENDANT CODE
}

size_t CRawFile::GetSize( void ) const
{
#ifdef _WIN32
    return (size_t)GetFileSize( m_file, NULL );
#elif defined(__linux__)
    struct stat fileInfo;
    fstat( fileno( m_file ), &fileInfo );

    return fileInfo.st_size;
#else
    return 0;
#endif //OS DEPENDANT CODE
}

fsOffsetNumber_t CRawFile::GetSizeNative( void ) const
{
#ifdef _WIN32
    union
    {
        LARGE_INTEGER fileSizeOut;
        DWORD fileSizeOut_split[ sizeof( LARGE_INTEGER ) / sizeof( DWORD ) ];
    };

    BOOL success = GetFileSizeEx( this->m_file, &fileSizeOut );

    if ( success == FALSE )
        return (fsOffsetNumber_t)0;

    // Convert to a FileSystem native number.
    fsOffsetNumber_t bigFileSizeNumber = (fsOffsetNumber_t)0;

    ConvertToWideNumber( bigFileSizeNumber, &fileSizeOut_split[0], NUMELMS(fileSizeOut_split), NUMBER_LITTLE_ENDIAN, NUMBER_LITTLE_ENDIAN );

    return bigFileSizeNumber;
#elif defined(__linux__)
    // todo.
#else
    return (fsOffsetNumber_t)0;
#endif //OS DEPENDANT CODE
}

void CRawFile::Flush( void )
{
#ifdef _WIN32
    FlushFileBuffers( m_file );
#elif defined(__linux__)
    fflush( m_file );
#endif //OS DEPENDANT CODE
}

const filePath& CRawFile::GetPath( void ) const
{
    return m_path;
}

bool CRawFile::IsReadable( void ) const
{
    return ( m_access & FILE_ACCESS_READ ) != 0;
}

bool CRawFile::IsWriteable( void ) const
{
    return ( m_access & FILE_ACCESS_WRITE ) != 0;
}