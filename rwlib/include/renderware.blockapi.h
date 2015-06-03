/*
    RenderWare block serialization helpers.
*/

enum eBlockMode
{
    RWBLOCKMODE_WRITE,
    RWBLOCKMODE_READ
};

struct RwBlockException : public RwException
{
    inline RwBlockException( const char *msg ) : RwException( msg )
    {
        return;
    }
};

struct BlockProvider
{
    typedef sliceOfData <int64> streamMemSlice_t;

    BlockProvider( Stream *contextStream, rw::eBlockMode blockMode );

    inline BlockProvider( BlockProvider *parentProvider )
    {
        this->parent = parentProvider;
        this->blockMode = parentProvider->blockMode;
        this->isInContext = false;
        this->contextStream = NULL;
        this->ignoreBlockRegions = parentProvider->ignoreBlockRegions;
    }

    inline BlockProvider( BlockProvider *parentProvider, bool ignoreBlockRegions )
    {
        this->parent = parentProvider;
        this->blockMode = parentProvider->blockMode;
        this->isInContext = false;
        this->contextStream = NULL;
        this->ignoreBlockRegions = ignoreBlockRegions;
    }

    inline ~BlockProvider( void )
    {
        assert( this->isInContext == false );
    }

protected:
    BlockProvider *parent;

    eBlockMode blockMode;
    bool isInContext;

    Stream *contextStream;

    bool ignoreBlockRegions;

    // Processing context of this stream.
    // This is stored for important points.
    struct Context
    {
        inline Context( void )
        {
            this->chunk_id = 0;
            this->chunk_beg_offset = -1;
            this->chunk_beg_offset_absolute = -1;
            this->chunk_length = -1;

            this->context_seek = -1;
        }

        uint32 chunk_id;
        int64 chunk_beg_offset;
        int64 chunk_beg_offset_absolute;
        int64 chunk_length;

        int64 context_seek;
        
        LibraryVersion chunk_version;
    };

    Context blockContext;

public:
    // Entering a block (i.e. parsing the header and setting limits).
    void EnterContext( void ) throw( ... );
    void LeaveContext( void );

    inline bool inContext( void ) const
    {
        return this->isInContext;
    }
    
    // Block modification API.
    void read( void *out_buf, size_t readCount ) throw( ... );
    void write( const void *in_buf, size_t writeCount ) throw( ... );

    void skip( size_t skipCount ) throw( ... );
    int64 tell( void ) const;
    int64 tell_absolute( void ) const;

    void seek( int64 pos, eSeekMode mode ) throw( ... );

    // Public verification API.
    void check_read_ahead( size_t readCount ) const;

protected:
    // Special helper algorithms.
    void read_native( void *out_buf, size_t readCount ) throw( ... );
    void write_native( const void *in_buf, size_t writeCount ) throw( ... );

    void skip_native( size_t skipCount ) throw( ... );

    void seek_native( int64 pos, eSeekMode mode ) throw( ... );
    int64 tell_native( void ) const;
    int64 tell_absolute_native( void ) const;

    Interface* getEngineInterface( void ) const;

public:
    // Block meta-data API.
    uint32 getBlockID( void ) const throw( ... );
    int64 getBlockLength( void ) const throw( ... );
    LibraryVersion getBlockVersion( void ) const throw( ... );

    void setBlockID( uint32 id );
    void setBlockVersion( LibraryVersion version );

    inline bool doesIgnoreBlockRegions( void ) const
    {
        return this->ignoreBlockRegions;
    }

    // Helper functions.
    template <typename structType>
    inline void writeStruct( const structType& theStruct )      { this->write( &theStruct, sizeof( theStruct ) ); }

    inline void writeUInt8( uint8 val )             { this->writeStruct( val ); }
    inline void writeUInt16( uint16 val )           { this->writeStruct( val ); }
    inline void writeUInt32( uint32 val )           { this->writeStruct( val ); }
    inline void writeUInt64( uint64 val )           { this->writeStruct( val ); }

    inline void writeInt8( int8 val )               { this->writeStruct( val );}
    inline void writeInt16( int16 val )             { this->writeStruct( val ); }
    inline void writeInt32( int32 val )             { this->writeStruct( val ); }
    inline void writeInt64( int64 val )             { this->writeStruct( val ); }

    template <typename structType>
    inline void readStruct( structType& outStruct )             { this->read( &outStruct, sizeof( outStruct ) ); }

    inline uint8 readUInt8( void )                  { uint8 val; this->readStruct( val ); return val; }
    inline uint16 readUInt16( void )                { uint16 val; this->readStruct( val ); return val; }
    inline uint32 readUInt32( void )                { uint32 val; this->readStruct( val ); return val; }
    inline uint64 readUInt64( void )                { uint64 val; this->readStruct( val ); return val; }

    inline int8 readInt8( void )                    { int8 val; this->readStruct( val ); return val; }
    inline int16 readInt16( void )                  { int16 val; this->readStruct( val ); return val; }
    inline int32 readInt32( void )                  { int32 val; this->readStruct( val ); return val; }
    inline int64 readInt64( void )                  { int64 val; this->readStruct( val ); return val; }

protected:
    // Validation API.
    void verifyLocalStreamAccess( const streamMemSlice_t& requestedMemory ) const;
    void verifyStreamAccess( const streamMemSlice_t& requestedMemory ) const;
};