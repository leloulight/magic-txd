#include "mainwindow.h"

struct rwFileSystemStreamWrapEnv
{
    struct eirFileSystemMetaInfo
    {
        inline eirFileSystemMetaInfo( void )
        {
            this->theStream = NULL;
        }

        inline ~eirFileSystemMetaInfo( void )
        {
            return;
        }

        CFile *theStream;
    };

    struct eirFileSystemWrapperProvider : public rw::customStreamInterface
    {
        void OnConstruct( rw::eStreamMode streamMode, void *userdata, void *membuf, size_t memSize ) const override
        {
            eirFileSystemMetaInfo *meta = new (membuf) eirFileSystemMetaInfo;

            meta->theStream = (CFile*)userdata;
        }

        void OnDestruct( void *memBuf, size_t memSize ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->~eirFileSystemMetaInfo();
        }

        size_t Read( void *memBuf, void *out_buf, size_t readCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->Read( out_buf, 1, readCount );
        }

        size_t Write( void *memBuf, const void *in_buf, size_t writeCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->Write( in_buf, 1, writeCount );
        }

        void Skip( void *memBuf, rw::int64 skipCount ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->theStream->SeekNative( skipCount, SEEK_CUR );
        }

        rw::int64 Tell( const void *memBuf ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->TellNative();
        }

        void Seek( void *memBuf, rw::int64 stream_offset, rw::eSeekMode seek_mode ) const override
        {
            int ansi_seek = SEEK_SET;

            if ( seek_mode == rw::RWSEEK_BEG )
            {
                ansi_seek = SEEK_SET;
            }
            else if ( seek_mode == rw::RWSEEK_CUR )
            {
                ansi_seek = SEEK_CUR;
            }
            else if ( seek_mode == rw::RWSEEK_END )
            {
                ansi_seek = SEEK_END;
            }
            else
            {
                assert( 0 );
            }

            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            meta->theStream->SeekNative( stream_offset, ansi_seek );
        }

        rw::int64 Size( const void *memBuf ) const override
        {
            eirFileSystemMetaInfo *meta = (eirFileSystemMetaInfo*)memBuf;

            return meta->theStream->GetSizeNative();
        }

        bool SupportsSize( const void *memBuf ) const override
        {
            return true;
        }

        CFileSystem *nativeFileSystem;
    };

    eirFileSystemWrapperProvider eirfs_file_wrap;

    inline void Initialize( MainWindow *mainwnd )
    {
        // Register the native file system wrapper type.
        eirfs_file_wrap.nativeFileSystem = mainwnd->fileSystem;

        mainwnd->GetEngine()->RegisterStream( "eirfs_file", sizeof( eirFileSystemMetaInfo ), &eirfs_file_wrap );
    }

    inline void Shutdown( MainWindow *mainwnd )
    {
        // Streams are unregistered automatically when the engine is destroyed.
        // TODO: could be dangerous. unregistering is way cleaner.
    }
};

rw::Stream* RwStreamCreateTranslated( MainWindow *mainwnd, CFile *eirStream )
{
    rw::streamConstructionCustomParam_t customParam( "eirfs_file", eirStream );

    rw::Stream *result = mainwnd->GetEngine()->CreateStream( rw::RWSTREAMTYPE_CUSTOM, rw::RWSTREAMMODE_READWRITE, &customParam );

    return result;
}

void InitializeRWFileSystemWrap( void )
{
    mainWindowFactory.RegisterDependantStructPlugin <rwFileSystemStreamWrapEnv> ();
}