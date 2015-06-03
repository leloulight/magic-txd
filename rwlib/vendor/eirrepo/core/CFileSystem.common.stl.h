/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        Shared/core/CFileSystem.common.stl.h
*  PURPOSE:     Definitions for interoperability with the STL library
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_STL_COMPAT_
#define _FILESYSTEM_STL_COMPAT_

#include <algorithm>

namespace FileSystem
{
    // CFile to std::streambuf wrapper.
    struct fileStreamBuf : public std::streambuf
    {
        CFile *underlyingStream;

        fileStreamBuf( CFile *theStream )
        {
            this->underlyingStream = theStream;
        }

    protected:
        std::streampos seekoff( std::streamoff offset, std::ios_base::seekdir way, std::ios_base::openmode openmode )
        {
            // We don't care about openmode.

            int offsetANSI;

            if ( way == std::ios_base::beg )
            {
                offsetANSI = SEEK_SET;
            }
            else if ( way == std::ios_base::cur )
            {
                offsetANSI = SEEK_CUR;
            }
            else if ( way == std::ios_base::end )
            {
                offsetANSI = SEEK_END;
            }
            else
            {
                assert( 0 );
            }

            long long streamOffsetANSI = offset;

            int seekSuccess = this->underlyingStream->SeekNative( streamOffsetANSI, offsetANSI );

            if ( seekSuccess == -1 )
                return -1;

            return this->underlyingStream->Tell();
        }

        std::streampos seekpos( std::streamoff offset, std::ios_base::openmode openmode )
        {
            long long streamOffsetANSI = offset;

            int seekSuccess = this->underlyingStream->SeekNative( streamOffsetANSI, SEEK_SET );

            if ( seekSuccess == -1 )
                return -1;

            return this->underlyingStream->Tell();
        }

        int sync( void )
        {
            this->underlyingStream->Flush();

            return 0;
        }

        std::streamsize showmanyc( void )
        {
            fsOffsetNumber_t curPos = this->underlyingStream->TellNative();
            fsOffsetNumber_t fileSize = this->underlyingStream->GetSizeNative();

            return (std::streamsize)( std::max( (fsOffsetNumber_t)0, fileSize - curPos ) );
        }

        std::streamsize xsgetn( char *outBuffer, std::streamsize n )
        {
            if ( n < 0 )
                return -1;

            if ( n > 0x7FFFFFFF )
                return -1;

            long realSize = (long)n;

            size_t readCount = this->underlyingStream->Read( outBuffer, 1, realSize );

            return (std::streamsize)readCount;
        }

        std::streamsize xsputn( const char *inputBuffer, std::streamsize n )
        {
            if ( n < 0 )
                return -1;

            if ( n > 0x7FFFFFFF )
                return -1;

            long realSize = (long)n;

            size_t writeCount = this->underlyingStream->Write( inputBuffer, 1, realSize );

            return (std::streamsize)writeCount;
        }

        int overflow( int c )
        {
            size_t writeCount = this->underlyingStream->Write( &c, 1, 1 );

            if ( writeCount == 0 )
                return EOF;

            this->underlyingStream->Seek( -(long)writeCount, SEEK_CUR );

            return c;
        }

        int uflow( void )
        {
            unsigned char theChar;

            size_t readCount = this->underlyingStream->Read( &theChar, 1, 1 );

            if ( readCount == 0 )
                return EOF;

            return theChar;
        }

        int underflow( void )
        {
            unsigned char theChar;

            size_t readCount = this->underlyingStream->Read( &theChar, 1, 1 );

            if ( readCount == 0 )
                return EOF;

            this->underlyingStream->Seek( -(long)readCount, SEEK_CUR );

            return theChar;
        }
    };
};

#include <sstream>

// Very basic definitions that are required all the time.
namespace NumberUtil
{
    AINLINE std::string to_string( int num )
    {
        std::stringstream stream;

        stream << num;

        return stream.str();
    }

    AINLINE std::string to_string_hex( int num )
    {
        std::stringstream stream;
        
        stream << std::hex;

        stream << num;

        return stream.str();
    }
};

#endif //_FILESYSTEM_STL_COMPAT_