#include <StdInc.h>

#include "pluginutil.hxx"

namespace rw
{

size_t Stream::read( void *out_buf, size_t readCount ) throw( ... )
{
    throw RwStreamException( "stream reading not implemented" );

    return 0;
}

size_t Stream::write( const void *in_buf, size_t writeCount ) throw( ... )
{
    throw RwStreamException( "stream writing not implemented" );

    return 0;
}

void Stream::skip( int64 skipCount ) throw( ... )
{
    throw RwStreamException( "stream skipping not supported" );
}

int64 Stream::tell( void ) const
{
    return -1;
}

void Stream::seek( int64 seek_off, eSeekMode seek_mode ) throw( ... )
{
    throw RwStreamException( "seeking not supported" );
}

int64 Stream::size( void ) const throw( ... )
{
    throw RwStreamException( "size is not supported" );
}

bool Stream::supportsSize( void ) const
{
    return false;
}

// File stream.
struct FileStream : public Stream
{
    inline FileStream( Interface *engineInterface, void *construction_params ) : Stream( engineInterface, construction_params )
    {
        this->file_handle = NULL;
    }

    inline ~FileStream( void )
    {
        // Clear the file (if allocated).
        if ( Interface *engineInterface = this->engineInterface )
        {
            if ( FileInterface::filePtr_t fileHandle = this->file_handle )
            {
                engineInterface->GetFileInterface()->CloseStream( fileHandle );
            }
        }
    }

    // Stream methods.
    size_t read( void *out_buf, size_t readCount ) override
    {
        size_t actualReadCount = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            actualReadCount = engineInterface->GetFileInterface()->ReadStream( this->file_handle, out_buf, readCount );
        }

        return actualReadCount;
    }

    size_t write( const void *in_buf, size_t writeCount ) override
    {
        size_t actualWriteCount = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            actualWriteCount = engineInterface->GetFileInterface()->WriteStream( this->file_handle, in_buf, writeCount );
        }

        return actualWriteCount;
    }

    void skip( int64 skipCount ) override
    {
        if ( Interface *engineInterface = this->engineInterface )
        {
            engineInterface->GetFileInterface()->SeekStream( this->file_handle, (long)skipCount, SEEK_CUR );
        }
    }

    // Advanced functions.
    int64 tell( void ) const override
    {
        int64 curSeek = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            curSeek = engineInterface->GetFileInterface()->TellStream( this->file_handle );
        }

        return curSeek;
    }

    void seek( int64 seek_off, eSeekMode seek_mode ) override
    {
        if ( Interface *engineInterface = this->engineInterface )
        {
            int ansi_seek = SEEK_CUR;

            if ( seek_mode == RWSEEK_BEG )
            {
                ansi_seek = SEEK_SET;
            }
            else if ( seek_mode == RWSEEK_CUR )
            {
                ansi_seek = SEEK_CUR;
            }
            else if ( seek_mode == RWSEEK_END )
            {
                ansi_seek = SEEK_END;
            }   

            engineInterface->GetFileInterface()->SeekStream( this->file_handle, (long)seek_off, ansi_seek );
        }
    }

    int64 size( void ) const override
    {
        int64 sizeOut = 0;

        if ( Interface *engineInterface = this->engineInterface )
        {
            sizeOut = engineInterface->GetFileInterface()->SizeStream( this->file_handle );
        }

        return sizeOut;
    }

    bool supportsSize( void ) const override
    {
        return true;
    }

    FileInterface::filePtr_t file_handle;
};

// Memory stream.
struct MemoryStream : public Stream
{
    inline MemoryStream( Interface *engineInterface, void *construction_params ) : Stream( engineInterface, construction_params )
    {
        return;
    }

    size_t read( void *out_buf, size_t readCount )
    {
        return 0;
    }

    size_t write( const void *in_buf, size_t writeCount )
    {
        return 0;
    }

    void skip( int64 skipCount )
    {

    }

    int64 tell( void ) const
    {
        return 0;
    }

    void seek( int64 seek_off, eSeekMode seek_mode )
    {

    }

    int64 size( void ) const
    {
        return 0;
    }

    bool supportsSize( void ) const
    {
        return true;
    }
};

// Custom stream.
// This is a simple wrapper so that every implementation can create native RenderWare streams without knowing the internals.
struct CustomStream : public Stream
{
    inline CustomStream( Interface *engineInterface, void *construction_params ) : Stream( engineInterface, construction_params )
    {
        this->streamProvider = NULL;
    }

    size_t read( void *out_buf, size_t readCount )
    {
        size_t actualReadCount = 0;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            actualReadCount = streamProvider->Read( metaBuf, out_buf, readCount );
        }

        return actualReadCount;
    }

    size_t write( const void *in_buf, size_t writeCount )
    {
        size_t actualWriteCount = 0;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            actualWriteCount = streamProvider->Write( metaBuf, in_buf, writeCount );
        }

        return actualWriteCount;
    }

    void skip( int64 skipCount )
    {
        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            streamProvider->Skip( metaBuf, skipCount );
        }
    }

    int64 tell( void ) const
    {
        int64 streamPos = -1;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            streamPos = streamProvider->Tell( metaBuf );
        }

        return streamPos;
    }

    void seek( int64 seek_off, eSeekMode seek_mode )
    {
        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            void *metaBuf = ( this + 1 );

            streamProvider->Seek( metaBuf, seek_off, seek_mode );
        }
    }

    int64 size( void ) const
    {
        int64 streamSize = -1;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            streamSize = streamProvider->Size( metaBuf );
        }

        return streamSize;
    }

    bool supportsSize( void ) const
    {
        bool supportsSize = false;

        if ( customStreamInterface *streamProvider = this->streamProvider )
        {
            const void *metaBuf = ( this + 1 );

            supportsSize = streamProvider->SupportsSize( metaBuf );
        }

        return supportsSize;
    }

    customStreamInterface *streamProvider;
};

// Register the stream plugin to the interface.
struct streamSystemPlugin
{
    inline void Initialize( Interface *engine )
    {
        this->fileStreamTypeInfo = NULL;
        this->memoryStreamTypeInfo = NULL;

        if ( engine->streamTypeInfo != NULL )
        {
            this->fileStreamTypeInfo = engine->typeSystem.RegisterStructType <FileStream> ( "file_stream", engine->streamTypeInfo );
            this->memoryStreamTypeInfo = engine->typeSystem.RegisterStructType <MemoryStream> ( "memory_stream", engine->streamTypeInfo );
        }
    }

    inline void Shutdown( Interface *engine )
    {
        // Delete all custom types first.
        for ( typeInfoList_t::const_iterator iter = custom_types.begin(); iter != custom_types.end(); iter++ )
        {
            RwTypeSystem::typeInfoBase *theType = *iter;

            engine->typeSystem.DeleteType( theType );
        }
        custom_types.clear();

        if ( RwTypeSystem::typeInfoBase *fileStreamTypeInfo = this->fileStreamTypeInfo )
        {
            engine->typeSystem.DeleteType( fileStreamTypeInfo );
        }

        if ( RwTypeSystem::typeInfoBase *memoryStreamTypeInfo = this->memoryStreamTypeInfo )
        {
            engine->typeSystem.DeleteType( memoryStreamTypeInfo );
        }
    }

    // Built-in stream types.
    RwTypeSystem::typeInfoBase *fileStreamTypeInfo;
    RwTypeSystem::typeInfoBase *memoryStreamTypeInfo;
    
    // Custom stream types.
    typedef std::vector <RwTypeSystem::typeInfoBase*> typeInfoList_t;

    typeInfoList_t custom_types;
};

static PluginDependantStructRegister <streamSystemPlugin, RwInterfaceFactory_t> streamSystemPluginRegister;

struct customStreamConstructionParams
{
    eStreamMode streamMode;
    void *userdata;
};

struct streamCustomTypeInterface : public RwTypeSystem::typeInterface
{
    void Construct( void *mem, Interface *engineInterface, void *construction_params ) const
    {
        // We must receive a custom userdata struct.
        customStreamConstructionParams *custom_param = (customStreamConstructionParams*)construction_params;

        // First construct the actual object.
        new (mem) CustomStream( engineInterface, NULL );

        try
        {
            // Next construct the custom interface.
            void *customBuf = (CustomStream*)mem + 1;

            this->meta_info->OnConstruct( custom_param->streamMode, custom_param->userdata, customBuf, this->objCustomBufferSize );
        }
        catch( ... )
        {
            // Destroy the main class again, since an exception was thrown.
            ( (CustomStream*)mem )->~CustomStream();

            throw;
        }
    }

    void CopyConstruct( void *mem, const void *srcMem ) const
    {
        // Impossible.
        throw RwException( "cannot clone custom streams" );
    }

    void Destruct( void *mem ) const
    {
        // First delete the meta struct.
        {
            void *customBuf = (CustomStream*)mem + 1;

            this->meta_info->OnDestruct( customBuf, this->objCustomBufferSize );
        }

        // Now delete the actual object.
        ( (CustomStream*)mem )->~CustomStream();
    }

    size_t GetTypeSize( Interface *engineInterface, void *construct_params ) const
    {
        return ( sizeof( CustomStream ) + this->objCustomBufferSize );
    }

    size_t GetTypeSizeByObject( Interface *engineInterface, const void *mem ) const
    {
        return ( sizeof( CustomStream ) + this->objCustomBufferSize );
    }

    size_t objCustomBufferSize;
    customStreamInterface *meta_info;
};

bool Interface::RegisterStream( const char *typeName, size_t memBufSize, customStreamInterface *streamInterface )
{
    bool registerSuccess = false;

    // Get the stream environment first.
    streamSystemPlugin *streamSysEnv = streamSystemPluginRegister.GetPluginStruct( (EngineInterface*)this );

    if ( streamSysEnv )
    {
        // We need a special type descriptor, just like in the native texture registration.
        streamCustomTypeInterface *tInterface = _newstruct <streamCustomTypeInterface> ( *this->typeSystem._memAlloc );

        if ( tInterface )
        {
            try
            {
                // Set up this type interface.
                tInterface->objCustomBufferSize = memBufSize;
                tInterface->meta_info = streamInterface;

                // Attempt to register it into the system.
                RwTypeSystem::typeInfoBase *customTypeInfo = this->typeSystem.RegisterCommonTypeInterface( typeName, tInterface, this->streamTypeInfo );

                if ( customTypeInfo )
                {
                    // We have successfully registered our type, so now we must register it into the system.
                    streamSysEnv->custom_types.push_back( customTypeInfo );
                    
                    // Success!
                    registerSuccess = true;
                }
            }
            catch( ... )
            {
                registerSuccess = false;

                // We do not rethrow, as we simply return a boolean.
            }

            if ( registerSuccess == false )
            {
                _delstruct <streamCustomTypeInterface> ( tInterface, *this->typeSystem._memAlloc );
            }
        }
    }

    return registerSuccess;
}

Stream* Interface::CreateStream( eBuiltinStreamType streamType, eStreamMode streamMode, streamConstructionParam_t *param )
{
    Stream *outputStream = NULL;

    streamSystemPlugin *streamSysEnv = streamSystemPluginRegister.GetPluginStruct( (EngineInterface*)this );

    if ( streamSysEnv )
    {
        if ( streamType == RWSTREAMTYPE_FILE || streamType == RWSTREAMTYPE_FILE_W )
        {
            FileInterface::filePtr_t fileHandle = NULL;

            // Only proceed if we have a file stream type.
            if ( RwTypeSystem::typeInfoBase *fileStreamTypeInfo = streamSysEnv->fileStreamTypeInfo )
            {
                if ( streamType == RWSTREAMTYPE_FILE )
                {
                    if ( param->dwSize >= sizeof( streamConstructionFileParam_t ) )
                    {
                        streamConstructionFileParam_t *file_param = (streamConstructionFileParam_t*)param;

                        // Dispatch the given stream mode into an ANSI descriptor.
                        const char *ansi_mode = "";

                        if ( streamMode == RWSTREAMMODE_READONLY )
                        {
                            ansi_mode = "rb";
                        }
                        else if ( streamMode == RWSTREAMMODE_READWRITE )
                        {
                            ansi_mode = "rb+";
                        }
                        else if ( streamMode == RWSTREAMMODE_WRITEONLY )
                        {
                            ansi_mode = "wb";
                        }
                        else if ( streamMode == RWSTREAMMODE_CREATE )
                        {
                            ansi_mode = "wb+";
                        }

                        fileHandle = this->GetFileInterface()->OpenStream( file_param->filename, ansi_mode );
                    }
                }
                else if ( streamType == RWSTREAMTYPE_FILE_W )
                {
                    if ( param->dwSize >= sizeof( streamConstructionFileParamW_t ) )
                    {
                        streamConstructionFileParamW_t *file_param = (streamConstructionFileParamW_t*)param;

                        // Dispatch the given stream mode into an ANSI descriptor.
                        const wchar_t *ansi_mode = L"";

                        if ( streamMode == RWSTREAMMODE_READONLY )
                        {
                            ansi_mode = L"rb";
                        }
                        else if ( streamMode == RWSTREAMMODE_READWRITE )
                        {
                            ansi_mode = L"rb+";
                        }
                        else if ( streamMode == RWSTREAMMODE_WRITEONLY )
                        {
                            ansi_mode = L"wb";
                        }
                        else if ( streamMode == RWSTREAMMODE_CREATE )
                        {
                            ansi_mode = L"wb+";
                        }

                        fileHandle = this->GetFileInterface()->OpenStreamW( file_param->filename, ansi_mode );
                    }
                }

                if ( fileHandle )
                {
                    GenericRTTI *rttiObj = this->typeSystem.Construct( this, fileStreamTypeInfo, NULL );

                    if ( rttiObj )
                    {
                        FileStream *fileStream = (FileStream*)RwTypeSystem::GetObjectFromTypeStruct( rttiObj );

                        // Give the file handle to the stream.
                        // It will take care of freeing that handle.
                        fileStream->file_handle = fileHandle;

                        outputStream = fileStream;
                    }

                    if ( outputStream == NULL )
                    {
                        this->GetFileInterface()->CloseStream( fileHandle );
                    }
                }
            }
        }
        else if ( streamType == RWSTREAMTYPE_MEMORY )
        {

        }
        else if ( streamType == RWSTREAMTYPE_CUSTOM )
        {
            // We need to get the stream type info to proceed.
            if ( RwTypeSystem::typeInfoBase *streamTypeInfo = this->streamTypeInfo )
            {
                if ( param->dwSize == sizeof( streamConstructionCustomParam_t ) )
                {
                    streamConstructionCustomParam_t *customParam = (streamConstructionCustomParam_t*)param;

                    // Attempt construction with the custom param.
                    // Since we cannot do too much verification here, we hope that the vendor does things right.

                    // Get the type info associated.
                    if ( RwTypeSystem::typeInfoBase *customTypeInfo = this->typeSystem.FindTypeInfo( customParam->typeName, streamTypeInfo ) )
                    {
                        // We need to fetch our custom type information struct.
                        // If we cannot resolve it, the type info is not a valid custom stream type info.
                        streamCustomTypeInterface *typeProvider = dynamic_cast <streamCustomTypeInterface*> ( customTypeInfo->tInterface );

                        if ( typeProvider )
                        {
                            customStreamConstructionParams cstr_params;
                            cstr_params.streamMode = streamMode;
                            cstr_params.userdata = customParam->userdata;

                            // Construct the object.
                            GenericRTTI *rtObj = this->typeSystem.Construct( this, customTypeInfo, &cstr_params );

                            if ( rtObj )
                            {
                                // We are successful!
                                CustomStream *customStream = (CustomStream*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

                                // Give the functionality provider to the stream.
                                customStream->streamProvider = typeProvider->meta_info;

                                outputStream = customStream;
                            }
                        }
                    }
                }
            }
        }
    }

    return outputStream;
}

void Interface::DeleteStream( Stream *theStream )
{
    // Just rek it.
    this->typeSystem.Destroy( this, RwTypeSystem::GetTypeStructFromObject( theStream ) );
}

void registerStreamGlobalPlugins( void )
{
    streamSystemPluginRegister.RegisterPlugin( engineFactory );
}

};