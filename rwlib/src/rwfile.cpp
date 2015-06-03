#include <StdInc.h>

#include <sys/stat.h>

namespace rw
{

#pragma warning(push)
#pragma warning(disable:4996)

// The default file interface.
struct ANSIFileInterface : public FileInterface
{
    filePtr_t   OpenStream( const char *streamPath, const char *streamMode )
    {
        return (filePtr_t)fopen( streamPath, streamMode );
    }

    void    CloseStream( filePtr_t ptr )
    {
        fclose( (FILE*)ptr );
    }

    size_t  ReadStream( filePtr_t ptr, void *outBuf, size_t readCount )
    {
        return fread( outBuf, 1, readCount, (FILE*)ptr );
    }

    size_t  WriteStream( filePtr_t ptr, const void *inBuf, size_t writeCount )
    {
        return fwrite( inBuf, 1, writeCount, (FILE*)ptr );
    }

    bool    SeekStream( filePtr_t ptr, long streamOffset, int type )
    {
        return ( fseek( (FILE*)ptr, streamOffset, type ) == 0 );
    }

    long    TellStream( filePtr_t ptr )
    {
        return ftell( (FILE*)ptr );
    }

    bool    IsEOFStream( filePtr_t ptr )
    {
        return ( feof( (FILE*)ptr ) != 0 );
    }

    long    SizeStream( filePtr_t ptr )
    {
        struct stat stats;

        int result = fstat( fileno( (FILE*)ptr ), &stats );

        if ( result != 0 )
            return -1;

        return stats.st_size;
    }

    void    FlushStream( filePtr_t ptr )
    {
        fflush( (FILE*)ptr );
    }
};
static ANSIFileInterface _defaultFileInterface;

#pragma warning(pop)

// Use this function to change the location of library file activity.
void Interface::SetFileInterface( FileInterface *intf )
{
    this->customFileInterface = intf;
}

FileInterface* Interface::GetFileInterface( void )
{
    FileInterface *ourInterface = this->customFileInterface;

    if ( ourInterface == NULL )
    {
        ourInterface = &_defaultFileInterface;
    }

    return ourInterface;
}

};