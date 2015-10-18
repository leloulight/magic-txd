#include "mainwindow.h"

#include "dirtools.h"

#include "txdbuild.h"

static rw::TextureBase* RwMakeTextureFromStream( rw::Interface *rwEngine, rw::Stream *imgStream, rwkind::eTargetGame targetGame, rwkind::eTargetPlatform targetPlatform )
{
    // Since we do not care about warnings, we can just process things here.
    // Otherwise we have a loader implementation at the texture add dialog, which is really great.

    rw::int64 streamBeg = imgStream->tell();

    // First check whether it is a texture chunk.
    try
    {
        rw::RwObject *rwObj = rwEngine->Deserialize( imgStream );

        if ( rwObj )
        {
            try
            {
                if ( rw::TextureBase *texChunk = rw::ToTexture( rwEngine, rwObj ) )
                {
                    return texChunk;
                }
            }
            catch( ... )
            {
                rwEngine->DeleteRwObject( rwObj );
                throw;
            }

            rwEngine->DeleteRwObject( rwObj );
        }
    }
    catch( rw::RwException& )
    {
        // Ignore failed texture chunk deserialization.
        // We still have other options.
    }

    imgStream->seek( streamBeg, rw::RWSEEK_BEG );

    // Next check whether we have an image.
    try
    {
        rw::Raster *texRaster = rw::CreateRaster( rwEngine );

        if ( texRaster )
        {
            try
            {
                // We need to give this raster a start native format.
                // For that we should give it the format it actually should have.
                const char *nativeName = rwkind::GetTargetNativeFormatName( targetPlatform, targetGame );

                if ( nativeName )
                {
                    texRaster->newNativeData( nativeName );
                }
                else
                {
                    assert( 0 );
                }

                texRaster->readImage( imgStream );

                // OK, we got an image. This means we should put the raster into a texture and return it!
                if ( rw::TextureBase *texHandle = rw::CreateTexture( rwEngine, texRaster ) )
                {
                    // We have to release our reference to the raster.
                    rw::DeleteRaster( texRaster );

                    return texHandle;
                }
            }
            catch( ... )
            {
                rw::DeleteRaster( texRaster );
                throw;
            }

            rw::DeleteRaster( texRaster );
        }
    }
    catch( rw::RwException& )
    {
        // Alright, we failed this one too.
    }

    // We got nothing.
    return NULL;
}

void BuildTXDArchives( rw::Interface *rwEngine, TxdBuildModule *module, CFileTranslator *gameRoot, CFileTranslator *outputRoot, const TxdBuildModule::run_config& config )
{
    // Process things.
    auto dir_callback = [&]( const filePath& dirPath )
    {
        try
        {
            rw::TexDictionary *texDict = rw::CreateTexDictionary( rwEngine );

            if ( !texDict )
            {
                throw rw::RwException( "failed to allocate texture dictionary object" );
            }
        
            try
            {
                auto per_dir_file_cb = [&]( const filePath& texturePath )
                {
                    // We first have to establish a stream to the file.
                    CFile *fsImgStream = gameRoot->Open( texturePath, L"rb" );

                    if ( fsImgStream )
                    {
                        try
                        {
                            // Decompress if we find compressed things. ;)
                            fsImgStream = module->WrapStreamCodec( fsImgStream );
                        }
                        catch( ... )
                        {
                            delete fsImgStream;

                            throw;
                        }
                    }

                    if ( fsImgStream )
                    {
                        try
                        {
                            // Try to turn this file into a texture.
                            try
                            {
                                rw::Stream *imgStream = RwStreamCreateTranslated( rwEngine, fsImgStream );

                                if ( imgStream )
                                {
                                    try
                                    {
                                        // We got all streams prepared!
                                        // Try turning it into a texture now.
                                        rw::TextureBase *imgTex = RwMakeTextureFromStream( rwEngine, imgStream, config.targetGame, config.targetPlatform );

                                        if ( imgTex )
                                        {
                                            try
                                            {
                                                // Give the texture a name based on the original filename.
                                                filePath texName = FileSystem::GetFileNameItem( texturePath, false );

                                                std::string ansiTexName = texName.convert_ansi();
                                            
                                                imgTex->SetName( ansiTexName.c_str() );

                                                // Set some default rendering properties.
                                                imgTex->SetUAddressing( rw::RWTEXADDRESS_WRAP );
                                                imgTex->SetVAddressing( rw::RWTEXADDRESS_WRAP );

                                                // ;)
                                                imgTex->improveFiltering();

                                                // Add our texture to the dictionary!
                                                imgTex->AddToDictionary( texDict );
                                            }
                                            catch( ... )
                                            {
                                                // In very rare cases we might have encountered an error.
                                                // This means that we decided against the texture, so delete it.
                                                rwEngine->DeleteRwObject( imgTex );

                                                throw;
                                            }
                                        }
                                    }
                                    catch( ... )
                                    {
                                        rwEngine->DeleteStream( imgStream );

                                        throw;
                                    }

                                    rwEngine->DeleteStream( imgStream );
                                }
                            }
                            catch( rw::RwException& )
                            {
                                // If we failed to parse anything, ignore the error.
                            }
                        }
                        catch( ... )
                        {
                            delete fsImgStream;

                            throw;
                        }

                        delete fsImgStream;
                    }
                };

                gameRoot->ScanDirectory( dirPath, "*", false, NULL, std::move( per_dir_file_cb ), NULL );

                // If we have at least one texture in this texture dictionary, we can initialize it and write away.
                if ( texDict->GetTextureCount() != 0 )
                {
                    // We give this TXD the version of the first texture inside, for good measure.
                    rw::TextureBase *firstTex = texDict->GetTextureIterator().Resolve();

                    texDict->SetEngineVersion( firstTex->GetEngineVersion() );

                    // Now write it to disk.
                    // We want to write it with the same name as the directory had.
                    // Here we can use a trick: trimm of the last character of the directory path, always a slash, and replace it with ".txd" !
                    // The path has to be relative, as we want to write it into the output root.
                    filePath txdWritePath;

                    bool hasPath = gameRoot->GetRelativePathFromRoot( dirPath, false, txdWritePath );

                    if ( hasPath )
                    {
                        // Trimm off the slash, if it exists.
                        {
                            size_t outPathLen = txdWritePath.size();

                            if ( outPathLen > 0 )
                            {
                                txdWritePath.resize( outPathLen - 1 );  // Here cannot be encoding issues as long as the character is a traditional slash.
                            }
                        }

                        txdWritePath += L".txd";

                        // Now establish the stream and push it!
                        CFile *fsTXDStream = outputRoot->Open( txdWritePath, L"wb" );

                        if ( fsTXDStream )
                        {
                            try
                            {
                                rw::Stream *txdStream = RwStreamCreateTranslated( rwEngine, fsTXDStream );

                                if ( txdStream )
                                {
                                    try
                                    {
                                        // Finally, get to write this thing.
                                        rwEngine->Serialize( texDict, txdStream );
                                    }
                                    catch( ... )
                                    {
                                        rwEngine->DeleteStream( txdStream );

                                        throw;
                                    }

                                    rwEngine->DeleteStream( txdStream );
                                }
                            }
                            catch( ... )
                            {
                                delete fsTXDStream;

                                throw;
                            }

                            delete fsTXDStream;
                        }
                    }
                }
            }
            catch( ... )
            {
                rwEngine->DeleteRwObject( texDict );

                throw;
            }

            rwEngine->DeleteRwObject( texDict );
        }
        catch( rw::RwException& )
        {
            // Ignore any errors we encounter at processing a TXD, so other TXDs can try processing.
        }
    };

    // Let us use the kickass C++11 lambdas :)
    gameRoot->ScanDirectory( "@", "*", true, std::move( dir_callback ), NULL, NULL );
}

bool TxdBuildModule::RunApplication( const run_config& config )
{
    rw::Interface *rwEngine = this->rwEngine;

    // Isolate us.
    rw::AssignThreadedRuntimeConfig( rwEngine );

    try
    {
        rwEngine->SetWarningLevel( 0 );
        rwEngine->SetWarningManager( NULL );

        // Get handles to the input and output directories.
        CFileTranslator *gameRootTranslator = NULL;

        bool hasGameRoot = obtainAbsolutePath( config.gameRoot.c_str(), gameRootTranslator, false );

        if ( hasGameRoot )
        {
            try
            {
                CFileTranslator *outputRootTranslator = NULL;

                bool hasOutputRoot = obtainAbsolutePath( config.outputRoot.c_str(), outputRootTranslator, true );

                if ( hasOutputRoot )
                {
                    try
                    {
                        if ( hasGameRoot && hasOutputRoot )
                        {
                            BuildTXDArchives( this->rwEngine, this, gameRootTranslator, outputRootTranslator, config );
                        }
                    }
                    catch( ... )
                    {
                        delete outputRootTranslator;

                        throw;
                    }

                    delete outputRootTranslator;
                }
            }
            catch( ... )
            {
                delete gameRootTranslator;

                throw;
            }
        
            delete gameRootTranslator;
        }
    }
    catch( ... )
    {
        rw::ReleaseThreadedRuntimeConfig( rwEngine );
        
        throw;
    }

    rw::ReleaseThreadedRuntimeConfig( rwEngine );

    return true;
}