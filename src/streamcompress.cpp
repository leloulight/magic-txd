#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

struct streamCompressionEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        // Only establish the temporary root on demand.
        this->tmpRoot = NULL;

        this->lockRootConsistency = rw::CreateReadWriteLock( mainWnd->GetEngine() );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        rw::CloseReadWriteLock( mainWnd->GetEngine(), this->lockRootConsistency );

        // If we have a temporary repository, destroy it.
        if ( this->tmpRoot )
        {
            mainWnd->fileSystem->DeleteTempRepository( this->tmpRoot );
        }
    }

    inline CFileTranslator* GetRepository( MainWindow *mainWnd )
    {
        if ( !this->tmpRoot )
        {
            rw::scoped_rwlock_writer <> consistency( this->lockRootConsistency );

            if ( !this->tmpRoot )
            {
                // We need to establish a temporary root for writing files into that we decompressed.
                this->tmpRoot = mainWnd->fileSystem->GenerateTempRepository();
            }
        }

        return this->tmpRoot;
    }

    rw::rwlock *lockRootConsistency;

    typedef std::vector <compressionManager*> compressors_t;

    compressors_t compressors;

    CFileTranslator *volatile tmpRoot;
};

static PluginDependantStructRegister <streamCompressionEnv, mainWindowFactory_t> streamCompressionEnvRegister;

CFile* CreateDecompressedStream( MainWindow *mainWnd, CFile *compressed )
{
    // We want to pipe the stream if we find out that it really is compressed.
    // For those we will create a special stream that will point to the new stream.

    CFile *resultFile = compressed;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.GetPluginStruct( mainWnd ) )
    {
        bool needsStreamReset = false;

        compressionManager *theManager = NULL;

        for ( compressionManager *manager : env->compressors )
        {
            if ( needsStreamReset )
            {
                compressed->Seek( 0, SEEK_SET );

                needsStreamReset = false;
            }

            bool isCorrectFormat = manager->IsStreamCompressed( compressed );

            needsStreamReset = true;

            if ( isCorrectFormat )
            {
                theManager = manager;
                break;
            }
        }

        if ( needsStreamReset )
        {
            compressed->Seek( 0, SEEK_SET );

            needsStreamReset = false;
        }

        // If we found a compressed format...
        if ( theManager )
        {
            // ... we want to create a random file and decompress into it.
            CFileTranslator *repo = env->GetRepository( mainWnd );

            if ( repo )
            {
                CFile *decFile = mainWnd->fileSystem->GenerateRandomFile( repo );

                if ( decFile )
                {
                    try
                    {
                        // Create a compression provider we will use.
                        compressionProvider *compressor = theManager->CreateProvider();

                        if ( compressor )
                        {
                            try
                            {
                                // Decompress!
                                bool couldDecompress = compressor->Decompress( compressed, decFile );

                                if ( couldDecompress )
                                {
                                    // Simply return the decompressed file.
                                    decFile->Seek( 0, SEEK_SET );

                                    resultFile = decFile;

                                    // We can free the other handle.
                                    delete compressed;

                                    compressed = NULL;
                                }
                                else
                                {
                                    // We kinda failed. Just return the original stream.
                                    compressed->Seek( 0, SEEK_SET );
                                }
                            }
                            catch( ... )
                            {
                                theManager->DestroyProvider( compressor );

                                throw;
                            }

                            theManager->DestroyProvider( compressor );
                        }
                    }
                    catch( ... )
                    {
                        delete decFile;

                        throw;
                    }

                    if ( resultFile == compressed )
                    {
                        delete decFile;
                    }
                }
            }
        }
    }

    return resultFile;
}

bool RegisterStreamCompressionManager( MainWindow *mainWnd, compressionManager *manager )
{
    bool success = false;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.GetPluginStruct( mainWnd ) )
    {
        env->compressors.push_back( manager );

        success = true;
    }

    return success;
}

bool UnregisterStreamCompressionManager( MainWindow *mainWnd, compressionManager *manager )
{
    bool success = false;

    if ( streamCompressionEnv *env = streamCompressionEnvRegister.GetPluginStruct( mainWnd ) )
    {
        streamCompressionEnv::compressors_t::iterator iter =
            std::find( env->compressors.begin(), env->compressors.end(), manager );

        if ( iter != env->compressors.end() )
        {
            env->compressors.erase( iter );

            success = true;
        }
    }

    return success;
}

// Sub modules.
extern void InitializeLZOStreamCompression( void );

void InitializeStreamCompressionEnvironment( void )
{
    streamCompressionEnvRegister.RegisterPlugin( mainWindowFactory );

    // Register sub modules.
    InitializeLZOStreamCompression();
}