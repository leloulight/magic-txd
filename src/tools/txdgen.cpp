#include "mainwindow.h"

#include "txdgen.h"

#include <iostream>
#include <streambuf>
#include <gtaconfig/include.h>

#include "dirtools.h"


bool TxdGenModule::ProcessTXDArchive(
    CFileTranslator *srcRoot, CFile *srcStream, CFile *targetStream, eTargetPlatform targetPlatform,
    bool clearMipmaps,
    bool generateMipmaps, rw::eMipmapGenerationMode mipGenMode, rw::uint32 mipGenMaxLevel,
    bool improveFiltering,
    bool doCompress, float compressionQuality,
    bool outputDebug, CFileTranslator *debugRoot,
    rw::KnownVersions::eGameVersion gameVersion,
    std::string& errMsg
) const
{
    rw::Interface *rwEngine = this->rwEngine;

    bool hasProcessed = false;

    // Optimize the texture archive.
    rw::Stream *txd_stream = RwStreamCreateTranslated( rwEngine, srcStream );

    try
    {
        rw::TexDictionary *txd = NULL;
        
        if ( txd_stream != NULL )
        {
            try
            {
                rw::RwObject *rwObj = rwEngine->Deserialize( txd_stream );

                if ( rwObj )
                {
                    txd = rw::ToTexDictionary( rwEngine, rwObj );

                    if ( txd == NULL )
                    {
                        errMsg = "not a texture dictionary (";
                        errMsg += rwEngine->GetObjectTypeName( rwObj );
                        errMsg += ")";

                        rwEngine->DeleteRwObject( rwObj );
                    }
                }
                else
                {
                    errMsg = "unknown RenderWare stream (maybe compressed)";
                }
            }
            catch( rw::RwException& except )
            {
                errMsg = "error reading txd: " + except.message;
                
                throw;
            }
        }

        if ( txd )
        {
            // Update the version of this texture dictionary.
            txd->SetEngineVersion( rwEngine->GetVersion() );

            try
            {
                // Process all textures.
                bool processSuccessful = true;

                try
                {
                    for ( rw::TexDictionary::texIter_t iter = txd->GetTextureIterator(); !iter.IsEnd(); iter.Increment() )
                    {
                        rw::TextureBase *theTexture = iter.Resolve();

                        // Update the version of this texture.
                        theTexture->SetEngineVersion( rwEngine->GetVersion() );

                        // We need to modify the raster.
                        rw::Raster *texRaster = theTexture->GetRaster();

                        if ( texRaster )
                        {
                            // Decide whether to convert to target architecture beforehand or afterward.
                            bool shouldConvertBeforehand = ShouldRasterConvertBeforehand( texRaster, targetPlatform );

                            bool hasConvertedToTargetArchitecture = false;

                            if ( shouldConvertBeforehand == true )
                            {
                                ConvertRasterToPlatform( theTexture, texRaster, targetPlatform, gameVersion );

                                hasConvertedToTargetArchitecture = true;
                            }

                            // Clear mipmaps if requested.
                            if ( clearMipmaps )
                            {
                                texRaster->clearMipmaps();

                                theTexture->fixFiltering();
                            }

                            // Generate mipmaps on demand.
                            if ( generateMipmaps )
                            {
                                // We generate as many mipmaps as we can.
                                texRaster->generateMipmaps( mipGenMaxLevel + 1, mipGenMode );

                                theTexture->fixFiltering();
                            }

                            // Output debug stuff.
                            if ( outputDebug && debugRoot != NULL )
                            {
                                // We want to debug mipmap generation, so output debug textures only using mipmaps.
                                //if ( _meetsDebugCriteria( tex ) )
                                {
                                    const filePath& srcPath = srcStream->GetPath();

                                    filePath relSrcPath;

                                    bool hasRelSrcPath = srcRoot->GetRelativePathFromRoot( srcPath.c_str(), true, relSrcPath );

                                    if ( hasRelSrcPath )
                                    {
                                        // Create a unique filename for this texture.
                                        filePath directoryPart;

                                        filePath fileNamePart = FileSystem::GetFileNameItem( relSrcPath.c_str(), false, &directoryPart, NULL );

                                        if ( fileNamePart.size() != 0 )
                                        {
                                            filePath uniqueTextureNameTGA = directoryPart + fileNamePart + "_" + filePath( theTexture->GetName().c_str() ) + ".tga";

                                            CFile *debugOutputStream = NULL;
                                            {
                                                if ( const char *sysPath = uniqueTextureNameTGA.c_str() )
                                                {
                                                    debugOutputStream = debugRoot->Open( sysPath, "wb" );
                                                }
                                                else if ( const wchar_t *sysPath = uniqueTextureNameTGA.w_str() )
                                                {
                                                    debugOutputStream = debugRoot->Open( sysPath, L"wb" );
                                                }
                                            }

                                            if ( debugOutputStream )
                                            {
                                                // Create a debug raster.
                                                rw::Raster *newRaster = rw::CreateRaster( rwEngine );

                                                if ( newRaster )
                                                {
                                                    newRaster->newNativeData( "Direct3D9" );

                                                    // Put the debug content into it.
                                                    {
                                                        rw::Bitmap debugTexContent;

                                                        debugTexContent.setBgColor( 1, 1, 1 );

                                                        bool gotDebugContent = rw::DebugDrawMipmaps( rwEngine, texRaster, debugTexContent );

                                                        if ( gotDebugContent )
                                                        {
                                                            newRaster->setImageData( debugTexContent );
                                                        }
                                                    }

                                                    if ( newRaster->getMipmapCount() > 0 )
                                                    {
                                                        // Write the debug texture to it.
                                                        rw::Stream *outputStream = RwStreamCreateTranslated( rwEngine, debugOutputStream );

                                                        if ( outputStream )
                                                        {
                                                            newRaster->writeImage( outputStream, "TGA" );

                                                            rwEngine->DeleteStream( outputStream );
                                                        }
                                                    }

                                                    rw::DeleteRaster( newRaster );
                                                }

                                                // Free the stream handle.
                                                delete debugOutputStream;
                                            }
                                        }
                                    }
                                }
                            }

                            // Palettize the texture to save space.
                            if ( doCompress )
                            {
                                // If we are not target architecture already, make sure we are.
                                if ( hasConvertedToTargetArchitecture == false )
                                {
                                    ConvertRasterToPlatform( theTexture, texRaster, targetPlatform, gameVersion );

                                    hasConvertedToTargetArchitecture = true;
                                }

                                if ( targetPlatform == PLATFORM_PS2 )
                                {
                                    texRaster->optimizeForLowEnd( compressionQuality );
                                }
                                else if ( targetPlatform == PLATFORM_XBOX || targetPlatform == PLATFORM_PC )
                                {
                                    // Compress if we are not already compressed.
                                    texRaster->compress( compressionQuality );
                                }
                            }

                            // Improve the filtering mode if the user wants us to.
                            if ( improveFiltering )
                            {
                                theTexture->improveFiltering();
                            }

                            // Convert it into the target platform.
                            if ( shouldConvertBeforehand == false )
                            {
                                if ( hasConvertedToTargetArchitecture == false )
                                {
                                    ConvertRasterToPlatform( theTexture, texRaster, targetPlatform, gameVersion );

                                    hasConvertedToTargetArchitecture = true;
                                }
                            }
                        }
                    }
                }
                catch( rw::RwException& except )
                {
                    errMsg = "error processing textures: " + except.message;
                    
                    throw;
                }

#if 0
                // Introduce this stub when dealing with memory leaks.
                static int ok_num = 0;

                if ( ok_num++ > 100 )
                {
                    throw termination_request();
                }
#endif

                // We do not perform any error checking for now.
                if ( processSuccessful )
                {
                    // Write the TXD into the target stream.
                    rw::Stream *rwTargetStream = RwStreamCreateTranslated( rwEngine, targetStream );

                    if ( targetStream )
                    {
                        try
                        {
                            try
                            {
                                rwEngine->Serialize( txd, rwTargetStream );
                            }
                            catch( rw::RwException& except )
                            {
                                errMsg = "error writing txd: " + except.message;
                                
                                throw;
                            }
                        }
                        catch( ... )
                        {
                            rwEngine->DeleteStream( rwTargetStream );

                            throw;
                        }

                        rwEngine->DeleteStream( rwTargetStream );

                        hasProcessed = true;
                    }
                }
            }
            catch( ... )
            {
                rwEngine->DeleteRwObject( txd );

                txd = NULL;
                
                throw;
            }

            // Delete the TXD.
            rwEngine->DeleteRwObject( txd );
        }
    }
    catch( rw::RwException& )
    {
        hasProcessed = false;
    }
    catch( ... )
    {
        hasProcessed = false;

        if ( txd_stream )
        {
            rwEngine->DeleteStream( txd_stream );

            txd_stream = NULL;
        }

        throw;
    }

    if ( txd_stream )
    {
        rwEngine->DeleteStream( txd_stream );

        txd_stream = NULL;
    }

    return hasProcessed;
}

struct _discFileSentry
{
    TxdGenModule *module;
    TxdGenModule::eTargetPlatform targetPlatform;
    bool clearMipmaps;
    bool generateMipmaps;
    rw::eMipmapGenerationMode mipGenMode;
    rw::uint32 mipGenMaxLevel;
    bool improveFiltering;
    bool doCompress;
    float compressionQuality;
    rw::KnownVersions::eGameVersion gameVersion;
    bool outputDebug;
    CFileTranslator *debugTranslator;

    inline bool OnSingletonFile(
        CFileTranslator *sourceRoot, CFileTranslator *buildRoot, const filePath& relPathFromRoot,
        const filePath& fileName, const filePath& extention, CFile *sourceStream,
        bool isInArchive
    )
    {
        // If we are asked to terminate, just do it.
        rw::CheckThreadHazards( module->GetEngine() );

        // Decide whether we need a copy.
        bool requiresCopy = false;

        bool anyWork = false;

        if ( extention.equals( "TXD", false ) == true || isInArchive )
        {
            requiresCopy = true;
        }

        // Open the target stream.
        CFile *targetStream = NULL;

        if ( requiresCopy )
        {
            std::wstring widePath = relPathFromRoot.convert_unicode();

            targetStream = buildRoot->Open( widePath.c_str(), L"wb" );
        }

        if ( targetStream && sourceStream )
        {
            // Allow to perform custom logic on the source and target streams.
            bool hasCopiedFile = false;
            {
                if ( extention.equals( "TXD", false ) == true )
                {
                    module->OnMessage( "*** " + relPathFromRoot.convert_ansi() + " ..." );

                    std::string errorMessage;

                    bool couldProcessTXD = this->module->ProcessTXDArchive(
                        sourceRoot, sourceStream, targetStream, this->targetPlatform,
                        this->clearMipmaps,
                        this->generateMipmaps, this->mipGenMode, this->mipGenMaxLevel,
                        this->improveFiltering,
                        this->doCompress, this->compressionQuality,
                        this->outputDebug, this->debugTranslator,
                        this->gameVersion,
                        errorMessage
                    );

                    if ( couldProcessTXD )
                    {
                        hasCopiedFile = true;

                        anyWork = true;

                        module->OnMessage( "OK\n" );
                    }
                    else
                    {
                        module->OnMessage( "error:\n" + errorMessage + "\n" );
                    }

                    // Output any warnings.
                    module->_warningMan.Purge();
                }
            }

            if ( requiresCopy )
            {
                // If we have not yet created the new copy, we default to simple stream swap.
                if ( !hasCopiedFile && targetStream )
                {
                    // Make sure we copy from the beginning of the source stream.
                    sourceStream->Seek( 0, SEEK_SET );
                    
                    // Copy the stream contents.
                    FileSystem::StreamCopy( *sourceStream, *targetStream );

                    hasCopiedFile = true;
                }
            }
        }

        if ( targetStream )
        {
            delete targetStream;
        }

        return anyWork;
    }

    inline void OnArchiveFail( const filePath& fileName, const filePath& extention )
    {
        module->OnMessage( "failed to create new IMG archive for processing; defaulting to file-copy ...\n" );
    }
};

inline bool isGoodEngine( const rw::Interface *engineInterface )
{
    if ( engineInterface->IsObjectRegistered( "texture" ) == false )
        return false;

    if ( engineInterface->IsObjectRegistered( "texture_dictionary" ) == false )
        return false;

    // We are ready to go.
    return true;
}

TxdGenModule::run_config TxdGenModule::ParseConfig( CFileTranslator *root, const filePath& path ) const
{
    run_config cfg;

    CFile *cfgStream = NULL;
    {
        if ( const char *sysPath = path.c_str() )
        {
            cfgStream = root->Open( sysPath, "rb" );
        }
        else if ( const wchar_t *sysPath = path.w_str() )
        {
            cfgStream = root->Open( sysPath, L"rb" );
        }
    }

    if ( cfgStream )
    {
        CINI *configFile = LoadINI( cfgStream );

        delete cfgStream;

        if ( configFile )
        {
            if ( CINI::Entry *mainEntry = configFile->GetEntry( "Main" ) )
            {
                // Output root.
                if ( const char *newOutputRoot = mainEntry->Get( "outputRoot" ) )
                {
                    cfg.c_outputRoot = (std::wstring_convert <std::codecvt <wchar_t, char, std::mbstate_t>, wchar_t> ()).from_bytes( newOutputRoot );
                }

                // Game root.
                if ( const char *newGameRoot = mainEntry->Get( "gameRoot" ) )
                {
                    cfg.c_gameRoot = (std::wstring_convert <std::codecvt <wchar_t, char, std::mbstate_t>, wchar_t> ()).from_bytes( newGameRoot );
                }

                // Target Platform.
                const char *targetPlatform = mainEntry->Get( "targetPlatform" );

                if ( targetPlatform )
                {
                    if ( stricmp( targetPlatform, "PC" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_PC;
                    }
                    else if ( stricmp( targetPlatform, "PS2" ) == 0 ||
                                stricmp( targetPlatform, "Playstation 2" ) == 0 ||
                                stricmp( targetPlatform, "PlayStation2" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_PS2;
                    }
                    else if ( stricmp( targetPlatform, "XBOX" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_XBOX;
                    }
                    else if ( stricmp( targetPlatform, "DXT_MOBILE" ) == 0 ||
                                stricmp( targetPlatform, "S3TC_MOBILE" ) == 0 ||
                                stricmp( targetPlatform, "MOBILE_DXT" ) == 0 ||
                                stricmp( targetPlatform, "MOBILE_S3TC" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_DXT_MOBILE;
                    }
                    else if ( stricmp( targetPlatform, "PVR" ) == 0 ||
                                stricmp( targetPlatform, "PowerVR" ) == 0 ||
                                stricmp( targetPlatform, "PVRTC" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_PVR;
                    }
                    else if ( stricmp( targetPlatform, "ATC" ) == 0 ||
                                stricmp( targetPlatform, "ATI_Compress" ) == 0 ||
                                stricmp( targetPlatform, "ATI" ) == 0 ||
                                stricmp( targetPlatform, "ATITC" ) == 0 ||
                                stricmp( targetPlatform, "ATI TC" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_ATC;
                    }
                    else if ( stricmp( targetPlatform, "UNC" ) == 0 ||
                                stricmp( targetPlatform, "UNCOMPRESSED" ) == 0 ||
                                stricmp( targetPlatform, "unc_mobile" ) == 0 ||
                                stricmp( targetPlatform, "uncompressed_mobile" ) == 0 ||
                                stricmp( targetPlatform, "mobile_unc" ) == 0 ||
                                stricmp( targetPlatform, "mobile_uncompressed" ) == 0 )
                    {
                        cfg.c_targetPlatform = PLATFORM_UNC_MOBILE;
                    }
                }

                // Target game version.
                if ( const char *targetVersion = mainEntry->Get( "targetVersion" ) )
                {
                    rw::KnownVersions::eGameVersion gameVer;
                    bool hasGameVer = false;
                            
                    if ( stricmp( targetVersion, "SA" ) == 0 ||
                            stricmp( targetVersion, "SanAndreas" ) == 0 ||
                            stricmp( targetVersion, "San Andreas" ) == 0 ||
                            stricmp( targetVersion, "GTA SA" ) == 0 ||
                            stricmp( targetVersion, "GTASA" ) == 0 )
                    {
                        gameVer = rw::KnownVersions::SA;

                        hasGameVer = true;
                    }
                    else if ( stricmp( targetVersion, "VC" ) == 0 ||
                                stricmp( targetVersion, "ViceCity" ) == 0 ||
                                stricmp( targetVersion, "Vice City" ) == 0 ||
                                stricmp( targetVersion, "GTA VC" ) == 0 ||
                                stricmp( targetVersion, "GTAVC" ) == 0 )
                    {
                        if ( cfg.c_targetPlatform == PLATFORM_PS2 )
                        {
                            gameVer = rw::KnownVersions::VC_PS2;
                        }
                        else
                        {
                            gameVer = rw::KnownVersions::VC_PC;
                        }

                        hasGameVer = true;
                    }
                    else if ( stricmp( targetVersion, "GTAIII" ) == 0 ||
                                stricmp( targetVersion, "III" ) == 0 ||
                                stricmp( targetVersion, "GTA3" ) == 0 ||
                                stricmp( targetVersion, "GTA 3" ) == 0 )
                    {
                        gameVer = rw::KnownVersions::GTA3;

                        hasGameVer = true;
                    }
                    else if ( stricmp( targetVersion, "MANHUNT" ) == 0 ||
                                stricmp( targetVersion, "MHUNT" ) == 0 ||
                                stricmp( targetVersion, "MH" ) == 0 )
                    {
                        gameVer = rw::KnownVersions::MANHUNT;

                        hasGameVer = true;
                    }
                        
                    if ( hasGameVer )
                    {
                        cfg.c_gameVersion = gameVer;
                    }
                }

                // Mipmap clear flag.
                if ( mainEntry->Find( "clearMipmaps" ) )
                {
                    cfg.c_clearMipmaps = mainEntry->GetBool( "clearMipmaps" );
                }

                // Mipmap Generation enable.
                if ( mainEntry->Find( "generateMipmaps" ) )
                {
                    cfg.c_generateMipmaps = mainEntry->GetBool( "generateMipmaps" );
                }

                // Mipmap Generation Mode.
                if ( const char *mipGenMode = mainEntry->Get( "mipGenMode" ) )
                {
                    if ( stricmp( mipGenMode, "default" ) == 0 ||
                            stricmp( mipGenMode, "recommended" ) == 0 )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_DEFAULT;
                    }
                    else if ( stricmp( mipGenMode, "contrast" ) == 0 )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_CONTRAST;
                    }
                    else if ( stricmp( mipGenMode, "brighten" ) == 0 )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_BRIGHTEN;
                    }
                    else if ( stricmp( mipGenMode, "darken" ) == 0 )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_DARKEN;
                    }
                    else if ( stricmp( mipGenMode, "selectclose" ) == 0 )
                    {
                        cfg.c_mipGenMode = rw::MIPMAPGEN_SELECTCLOSE;
                    } 
                }

                // Mipmap generation maximum level.
                if ( mainEntry->Find( "mipGenMaxLevel" ) )
                {
                    int mipGenMaxLevelInt = mainEntry->GetInt( "mipGenMaxLevel" );

                    if ( mipGenMaxLevelInt >= 0 )
                    {
                        cfg.c_mipGenMaxLevel = (rw::uint32)mipGenMaxLevelInt;
                    }
                }

                // Filter mode improvement.
                if ( mainEntry->Find( "improveFiltering" ) )
                {
                    cfg.c_improveFiltering = mainEntry->GetBool( "improveFiltering" );
                }

                // Compression.
                if ( mainEntry->Find( "compressTextures" ) )
                {
                    cfg.compressTextures = mainEntry->GetBool( "compressTextures" );
                }

                // Compression quality.
                if ( mainEntry->Find( "compressionQuality" ) )
                {
                    cfg.c_compressionQuality = (float)mainEntry->GetFloat( "compressionQuality", 0.0 );
                }

                // Palette runtime type.
                if ( const char *palRuntimeType = mainEntry->Get( "palRuntimeType" ) )
                {
                    if ( stricmp( palRuntimeType, "native" ) == 0 )
                    {
                        cfg.c_palRuntimeType = rw::PALRUNTIME_NATIVE;
                    }
                    else if ( stricmp( palRuntimeType, "pngquant" ) == 0 )
                    {
                        cfg.c_palRuntimeType = rw::PALRUNTIME_PNGQUANT;
                    }
                }

                // DXT compression method.
                if ( const char *dxtCompressionMethod = mainEntry->Get( "dxtRuntimeType" ) )
                {
                    if ( stricmp( dxtCompressionMethod, "native" ) == 0 )
                    {
                        cfg.c_dxtRuntimeType = rw::DXTRUNTIME_NATIVE;
                    }
                    else if ( stricmp( dxtCompressionMethod, "squish" ) == 0 ||
                                stricmp( dxtCompressionMethod, "libsquish" ) == 0 ||
                                stricmp( dxtCompressionMethod, "recommended" ) == 0 )
                    {
                        cfg.c_dxtRuntimeType = rw::DXTRUNTIME_SQUISH;
                    }
                }

                // Warning level.
                if ( mainEntry->Find( "warningLevel" ) )
                {
                    cfg.c_warningLevel = mainEntry->GetInt( "warningLevel" );
                }

                // Ignore secure warnings.
                if ( mainEntry->Find( "ignoreSecureWarnings" ) )
                {
                    cfg.c_ignoreSecureWarnings = mainEntry->GetBool( "ignoreSecureWarnings" );
                }

                // Reconstruct IMG Archives.
                if ( mainEntry->Find( "reconstructIMGArchives" ) )
                {
                    cfg.c_reconstructIMGArchives = mainEntry->GetBool( "reconstructIMGArchives" );
                }

                // Fix incompatible rasters.
                if ( mainEntry->Find( "fixIncompatibleRasters" ) )
                {
                    cfg.c_fixIncompatibleRasters = mainEntry->GetBool( "fixIncompatibleRasters" );
                }

                // DXT packed decompression.
                if ( mainEntry->Find( "dxtPackedDecompression" ) )
                {
                    cfg.c_dxtPackedDecompression = mainEntry->GetBool( "dxtPackedDecompression" );
                }
                    
                // IMG archive compression
                if ( mainEntry->Find( "imgArchivesCompressed" ) )
                {
                    cfg.c_imgArchivesCompressed = mainEntry->GetBool( "imgArchivesCompressed" );
                }

                // Serialization compatibility setting.
                if ( mainEntry->Find( "ignoreSerializationRegions" ) )
                {
                    cfg.c_ignoreSerializationRegions = mainEntry->GetBool( "ignoreSerializationRegions" );
                }

                // Debug output flag.
                if ( mainEntry->Find( "outputDebug" ) )
                {
                    cfg.c_outputDebug = mainEntry->GetBool( "outputDebug" );
                }
            }

            // Kill the configuration.
            delete configFile;
        }
    }

    return cfg;
}

bool TxdGenModule::ApplicationMain( const run_config& cfg )
{
    this->OnMessage(
        "RenderWare TXD generator tool. Compiled on " __DATE__ "\n" \
        "Use this tool at your own risk!\n\n"
    );

    bool successful = true;

    if ( isGoodEngine( rwEngine ) )
    {
        // Set up the warning buffer.
        rwEngine->SetWarningManager( &_warningMan );

        // Set some configuration.
        rwEngine->SetVersion( rw::KnownVersions::getGameVersion( cfg.c_gameVersion ) );
        rwEngine->SetPaletteRuntime( cfg.c_palRuntimeType );
        rwEngine->SetDXTRuntime( cfg.c_dxtRuntimeType );

        rwEngine->SetFixIncompatibleRasters( cfg.c_fixIncompatibleRasters );

        rwEngine->SetIgnoreSerializationBlockRegions( cfg.c_ignoreSerializationRegions );

        rwEngine->SetWarningLevel( cfg.c_warningLevel );

        rwEngine->SetIgnoreSecureWarnings( cfg.c_ignoreSecureWarnings );

        // Output some debug info.
        this->OnMessage(
            "=========================\n" \
            "Configuration:\n" \
            "=========================\n"
        );

        this->OnMessage(
            L"* outputRoot: " + cfg.c_outputRoot + L"\n" \
            L"* gameRoot: " + cfg.c_gameRoot + L"\n"
        );

        rw::LibraryVersion targetVersion = rwEngine->GetVersion();

        const char *strTargetVersion = "unknown";

        if ( targetVersion.rwLibMajor == 3 && targetVersion.rwLibMinor == 6 )
        {
            if ( cfg.c_gameVersion == rw::KnownVersions::SA )
            {
                strTargetVersion = "San Andreas";
            }
            else if ( cfg.c_gameVersion == rw::KnownVersions::MANHUNT )
            {
                strTargetVersion = "Manhunt";
            }
        }
        else if ( targetVersion.rwLibMajor == 3 && ( targetVersion.rwLibMinor == 3 || targetVersion.rwLibMinor == 4 ) )
        {
            strTargetVersion = "Vice City";
        }
        else if ( targetVersion.rwLibMajor == 3 && ( targetVersion.rwLibMinor == 0 || targetVersion.rwLibMinor == 1 ) )
        {
            strTargetVersion = "GTA 3";
        }

        this->OnMessage(
            std::string( "* targetVersion: " ) + strTargetVersion + " [rwver: " + targetVersion.toString() + "]\n"
        );

        const char *strTargetPlatform = "unknown";

        if ( cfg.c_targetPlatform == PLATFORM_PC )
        {
            strTargetPlatform = "PC";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_PS2 )
        {
            strTargetPlatform = "PS2";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_XBOX )
        {
            strTargetPlatform = "XBOX";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_DXT_MOBILE )
        {
            strTargetPlatform = "S3TC [mobile]";
        }   
        else if ( cfg.c_targetPlatform == PLATFORM_PVR )
        {
            strTargetPlatform = "PowerVR [mobile]";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_ATC )
        {
            strTargetPlatform = "ATI [mobile]";
        }
        else if ( cfg.c_targetPlatform == PLATFORM_UNC_MOBILE )
        {
            strTargetPlatform = "uncompressed [mobile]";
        }

        this->OnMessage(
            std::string( "* targetPlatform: " ) + strTargetPlatform + "\n"
        );

        this->OnMessage(
            std::string( "* clearMipmaps: " ) + ( cfg.c_clearMipmaps ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* generateMipmaps: " ) + ( cfg.c_generateMipmaps ? "true" : "false" ) + "\n"
        );

        const char *mipGenModeString = "unknown";

        if ( cfg.c_mipGenMode == rw::MIPMAPGEN_DEFAULT )
        {
            mipGenModeString = "default";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_CONTRAST )
        {
            mipGenModeString = "contrast";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_BRIGHTEN )
        {
            mipGenModeString = "brighten";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_DARKEN )
        {
            mipGenModeString = "darken";
        }
        else if ( cfg.c_mipGenMode == rw::MIPMAPGEN_SELECTCLOSE )
        {
            mipGenModeString = "selectclose";
        }

        this->OnMessage(
            std::string( "* mipGenMode: " ) + mipGenModeString + "\n"
        );

        this->OnMessage(
            std::string( "* mipGenMaxLevel: " ) + std::to_string( cfg.c_mipGenMaxLevel ) + "\n"
        );

        this->OnMessage(
            std::string( "* improveFiltering: " ) + ( cfg.c_improveFiltering ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* compressTextures: " ) + ( cfg.compressTextures ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* compressionQuality: " ) + std::to_string( cfg.c_compressionQuality ) + "\n"
        );

        const char *strPalRuntimeType = "unknown";

        if ( cfg.c_palRuntimeType == rw::PALRUNTIME_NATIVE )
        {
            strPalRuntimeType = "native";
        }
        else if ( cfg.c_palRuntimeType == rw::PALRUNTIME_PNGQUANT )
        {
            strPalRuntimeType = "pngquant";
        }

        this->OnMessage(
            std::string( "* palRuntimeType: " ) + strPalRuntimeType + "\n"
        );

        const char *strDXTRuntimeType = "unknown";

        if ( cfg.c_dxtRuntimeType == rw::DXTRUNTIME_NATIVE )
        {
            strDXTRuntimeType = "native";
        }
        else if ( cfg.c_dxtRuntimeType == rw::DXTRUNTIME_SQUISH )
        {
            strDXTRuntimeType = "squish";
        }

        this->OnMessage(
            std::string( "* dxtRuntimeType: " ) + strDXTRuntimeType + "\n"
        );

        this->OnMessage(
            std::string( "* warningLevel: " ) + std::to_string( cfg.c_warningLevel ) + "\n"
        );

        this->OnMessage(
            std::string( "* ignoreSecureWarnings: " ) + ( cfg.c_ignoreSecureWarnings ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* reconstructIMGArchives: " ) + ( cfg.c_reconstructIMGArchives ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* fixIncompatibleRasters: " ) + ( cfg.c_fixIncompatibleRasters ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* dxtPackedDecompression: " ) + ( cfg.c_dxtPackedDecompression ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* imgArchivesCompressed: " ) + ( cfg.c_imgArchivesCompressed ? "true" : "false" ) + "\n"
        );

        this->OnMessage(
            std::string( "* ignoreSerializationRegions: " ) + ( cfg.c_ignoreSerializationRegions ? "true" : "false" ) + "\n"
        );

        // Finish with a newline.
        this->OnMessage( "\n" );

        // Do the conversion!
        {
            CFileTranslator *absGameRootTranslator = NULL;
            CFileTranslator *absOutputRootTranslator = NULL;

            bool hasGameRoot = obtainAbsolutePath( cfg.c_gameRoot.c_str(), absGameRootTranslator, false, true );
            bool hasOutputRoot = obtainAbsolutePath( cfg.c_outputRoot.c_str(), absOutputRootTranslator, true, true );

            // Create a debug directory if we want to output debug.
            CFileTranslator *absDebugOutputTranslator = NULL;

            bool hasDebugRoot = false;

            if ( cfg.c_outputDebug )
            {
                hasDebugRoot = obtainAbsolutePath( L"debug_output/", absDebugOutputTranslator, true, true );
            }

            if ( hasGameRoot && hasOutputRoot )
            {
                try
                {
                    // File roots are prepared.
                    // We can start processing files.
                    gtaFileProcessor <_discFileSentry> fileProc( this );

                    fileProc.setArchiveReconstruction( cfg.c_reconstructIMGArchives );

                    fileProc.setUseCompressedIMGArchives( cfg.c_imgArchivesCompressed );

                    _discFileSentry sentry;
                    sentry.module = this;
                    sentry.targetPlatform = cfg.c_targetPlatform;
                    sentry.clearMipmaps = cfg.c_clearMipmaps;
                    sentry.generateMipmaps = cfg.c_generateMipmaps;
                    sentry.mipGenMode = cfg.c_mipGenMode;
                    sentry.mipGenMaxLevel = cfg.c_mipGenMaxLevel;
                    sentry.improveFiltering = cfg.c_improveFiltering;
                    sentry.doCompress = cfg.compressTextures;
                    sentry.compressionQuality = cfg.c_compressionQuality;
                    sentry.gameVersion = cfg.c_gameVersion;
                    sentry.outputDebug = cfg.c_outputDebug;
                    sentry.debugTranslator = absDebugOutputTranslator;

                    fileProc.process( &sentry, absGameRootTranslator, absOutputRootTranslator );

                    // Output any warnings.
                    _warningMan.Purge();
                }
                catch( ... )
                {
                    // OK.
                    this->OnMessage(
                        "terminated module\n"
                    );

                    successful = false;
                }
            }
            else
            {
                if ( !hasGameRoot )
                {
                    this->OnMessage( "could not get a filesystem handle to the game root\n" );
                }

                if ( !hasOutputRoot )
                {
                    this->OnMessage( "could not get a filesystem handle to the output root\n" );
                }
            }

            // Clean up resources.
            if ( hasDebugRoot )
            {
                delete absDebugOutputTranslator;
            }

            if ( hasGameRoot )
            {
                delete absGameRootTranslator;
            }

            if ( hasOutputRoot )
            {
                delete absOutputRootTranslator;
            }
        }
    }
    else
    {
        this->OnMessage( "error: incompatible RenderWare environment.\n" );
    }

    return successful;
}