#include "mainwindow.h"

#include "massconvert.h"

#define MAGICTXD_UNICODE_STRING_ID      0xBABE0001
#define MAGICTXD_CONFIG_BLOCK           0xBABE0002
#define MAGICTXD_ANSI_STRING_ID         0xBABE0003

// Our string blocks.
void RwWriteUnicodeString( rw::BlockProvider& prov, const std::wstring& in )
{
    rw::BlockProvider stringBlock( &prov, false );

    stringBlock.EnterContext();
    
    // NOTE: this function is not cross-platform, because wchar_t is platform dependant.

    try
    {
        stringBlock.setBlockID( MAGICTXD_UNICODE_STRING_ID );

        // Simply write stuff, without zero termination.
        stringBlock.write( in.c_str(), in.length() * sizeof( wchar_t ) );

        // Done.
    }
    catch( ... )
    {
        stringBlock.LeaveContext();

        throw;
    }

    stringBlock.LeaveContext();
}

bool RwReadUnicodeString( rw::BlockProvider& prov, std::wstring& out )
{
    bool gotString = false;

    rw::BlockProvider stringBlock( &prov, false );

    stringBlock.EnterContext();

    try
    {
        if ( stringBlock.getBlockID() == MAGICTXD_UNICODE_STRING_ID )
        {
            // Simply read stuff.
            rw::int64 blockLength = stringBlock.getBlockLength();

            // We need to get a valid unicode string length.
            size_t unicodeLength = ( blockLength / sizeof( wchar_t ) );

            rw::int64 unicodeDataLength = ( unicodeLength * sizeof( wchar_t ) );

            out.resize( unicodeLength );

            // Read into the unicode string implementation.
            stringBlock.read( (wchar_t*)out.data(), unicodeDataLength );

            // Skip the remainder.
            stringBlock.skip( blockLength - unicodeDataLength );

            gotString = true;
        }
    }
    catch( ... )
    {
        stringBlock.LeaveContext();

        throw;
    }

    stringBlock.LeaveContext();

    return gotString;
}

// ANSI string stuff.
void RwWriteANSIString( rw::BlockProvider& parentBlock, const std::string& str )
{
    rw::BlockProvider stringBlock( &parentBlock );

    stringBlock.EnterContext();

    try
    {
        stringBlock.setBlockID( MAGICTXD_ANSI_STRING_ID );

        stringBlock.write( str.c_str(), str.size() );
    }
    catch( ... )
    {
        stringBlock.LeaveContext();

        throw;
    }

    stringBlock.LeaveContext();
}

bool RwReadANSIString( rw::BlockProvider& parentBlock, std::string& stringOut )
{
    bool gotString = false;

    rw::BlockProvider stringBlock( &parentBlock );

    stringBlock.EnterContext();

    try
    {
        if ( stringBlock.getBlockID() == MAGICTXD_ANSI_STRING_ID )
        {
            // We read as much as we can into a memory buffer.
            rw::int64 blockSize = stringBlock.getBlockLength();

            size_t ansiStringLength = (size_t)blockSize;

            stringOut.resize( ansiStringLength );

            stringBlock.read( (void*)stringOut.data(), ansiStringLength );

            // Skip the rest.
            stringBlock.skip( blockSize - ansiStringLength );

            gotString = true;
        }
    }
    catch( ... )
    {
        stringBlock.LeaveContext();

        throw;
    }

    stringBlock.LeaveContext();

    return gotString;
}

// Utilities for connecting the GUI to a filesystem repository.
struct mainWindowSerialization
{
    // Serialization things.
    CFileTranslator *appRoot;       // directory the application module is in.
    CFileTranslator *toolRoot;      // the current directory we launched in

    struct mtxd_cfg_struct
    {
        bool addImageGenMipmaps;
        bool lockDownTXDPlatform;
    };

    struct txdgen_cfg_struct
    {
        endian::little_endian <rw::KnownVersions::eGameVersion> c_gameVersion;
        endian::little_endian <TxdGenModule::eTargetPlatform> c_targetPlatform;
        
        bool c_clearMipmaps;
        bool c_generateMipmaps;
        
        endian::little_endian <rw::eMipmapGenerationMode> c_mipGenMode;
        endian::little_endian <rw::uint32> c_mipGenMaxLevel;

        bool c_improveFiltering;
        bool compressTextures;

        endian::little_endian <rw::ePaletteRuntimeType> c_palRuntimeType;
        endian::little_endian <rw::eDXTCompressionMethod> c_dxtRuntimeType;

        bool c_reconstructIMGArchives;
        bool c_fixIncompatibleRasters;
        bool c_dxtPackedDecompression;
        bool c_imgArchivesCompressed;
        bool c_ignoreSerializationRegions;
        
        endian::little_endian <rw::float32> c_compressionQuality;

        bool c_outputDebug;

        endian::little_endian <rw::int32> c_warningLevel;

        bool c_ignoreSecureWarnings;
    };

    inline void loadSerialization( rw::BlockProvider& mainBlock, MainWindow *mainwnd )
    {
        // last directory we were in to save TXD file.
        {
            std::wstring lastTXDSaveDir;

            bool gotDir = RwReadUnicodeString( mainBlock, lastTXDSaveDir );

            if ( gotDir )
            {
                mainwnd->lastTXDSaveDir = QString::fromStdWString( lastTXDSaveDir );
            }
        }

        // last directory we were in to add an image file.
        {
            std::wstring lastImageFileOpenDir;

            bool gotDir = RwReadUnicodeString( mainBlock, lastImageFileOpenDir );

            if ( gotDir )
            {
                mainwnd->lastImageFileOpenDir = QString::fromStdWString( lastImageFileOpenDir );
            }
        }

        // Export all window stuff.
        {
            rw::BlockProvider exportAllBlock( &mainBlock );

            exportAllBlock.EnterContext();
            
            try
            {
                RwReadANSIString( exportAllBlock, mainwnd->lastUsedAllExportFormat );
                RwReadUnicodeString( exportAllBlock, mainwnd->lastAllExportTarget );
            }
            catch( ... )
            {
                exportAllBlock.LeaveContext();

                throw;
            }

            exportAllBlock.LeaveContext();
        }

        // MTXD configuration block.
        {
            rw::BlockProvider mtxdConfig( &mainBlock );

            mtxdConfig.EnterContext();

            try
            {
                if ( mtxdConfig.getBlockID() == rw::CHUNK_STRUCT )
                {
                    mtxd_cfg_struct cfgStruct;
                    mtxdConfig.readStruct( cfgStruct );

                    mainwnd->addImageGenMipmaps = cfgStruct.addImageGenMipmaps;
                    mainwnd->lockDownTXDPlatform = cfgStruct.lockDownTXDPlatform;
                }
            }
            catch( ... )
            {
                mtxdConfig.LeaveContext();

                throw;
            }

            mtxdConfig.LeaveContext();
        }

        // TxdGen configuration block.
        if ( massconvEnv *massconv = massconvEnvRegister.GetPluginStruct( mainwnd ) )
        {
            rw::BlockProvider massconvBlock( &mainBlock );

            massconvBlock.EnterContext();

            try
            {
                RwReadUnicodeString( massconvBlock, massconv->txdgenConfig.c_gameRoot );
                RwReadUnicodeString( massconvBlock, massconv->txdgenConfig.c_outputRoot );

                txdgen_cfg_struct cfgStruct;
                massconvBlock.readStruct( cfgStruct );

                massconv->txdgenConfig.c_gameVersion = cfgStruct.c_gameVersion;
                massconv->txdgenConfig.c_targetPlatform = cfgStruct.c_targetPlatform;
                massconv->txdgenConfig.c_clearMipmaps = cfgStruct.c_clearMipmaps;
                massconv->txdgenConfig.c_generateMipmaps = cfgStruct.c_generateMipmaps;
                massconv->txdgenConfig.c_mipGenMode = cfgStruct.c_mipGenMode;
                massconv->txdgenConfig.c_mipGenMaxLevel = cfgStruct.c_mipGenMaxLevel;
                massconv->txdgenConfig.c_improveFiltering = cfgStruct.c_improveFiltering;
                massconv->txdgenConfig.compressTextures = cfgStruct.compressTextures;
                massconv->txdgenConfig.c_palRuntimeType = cfgStruct.c_palRuntimeType;
                massconv->txdgenConfig.c_dxtRuntimeType = cfgStruct.c_dxtRuntimeType;
                massconv->txdgenConfig.c_reconstructIMGArchives = cfgStruct.c_reconstructIMGArchives;
                massconv->txdgenConfig.c_fixIncompatibleRasters = cfgStruct.c_fixIncompatibleRasters;
                massconv->txdgenConfig.c_dxtPackedDecompression = cfgStruct.c_dxtPackedDecompression;
                massconv->txdgenConfig.c_imgArchivesCompressed = cfgStruct.c_imgArchivesCompressed;
                massconv->txdgenConfig.c_ignoreSerializationRegions = cfgStruct.c_ignoreSerializationRegions;
                massconv->txdgenConfig.c_compressionQuality = cfgStruct.c_compressionQuality;
                massconv->txdgenConfig.c_outputDebug = cfgStruct.c_outputDebug;
                massconv->txdgenConfig.c_warningLevel = cfgStruct.c_warningLevel;
                massconv->txdgenConfig.c_ignoreSecureWarnings = cfgStruct.c_ignoreSecureWarnings;
            }
            catch( ... )
            {
                massconvBlock.LeaveContext();

                throw;
            }

            massconvBlock.LeaveContext();
        }
    }

    inline void saveSerialization( rw::BlockProvider& mainBlock, const MainWindow *mainwnd )
    {
        RwWriteUnicodeString( mainBlock, mainwnd->lastTXDSaveDir.toStdWString() );
        RwWriteUnicodeString( mainBlock, mainwnd->lastImageFileOpenDir.toStdWString() );

        // Export all window.
        {
            rw::BlockProvider exportAllBlock( &mainBlock );

            exportAllBlock.EnterContext();

            try
            {
                RwWriteANSIString( exportAllBlock, mainwnd->lastUsedAllExportFormat );
                RwWriteUnicodeString( exportAllBlock, mainwnd->lastAllExportTarget );
            }
            catch( ... )
            {
                exportAllBlock.LeaveContext();

                throw;
            }

            exportAllBlock.LeaveContext();
        }

        // MTXD config block.
        {
            rw::BlockProvider mtxdConfig( &mainBlock );

            mtxdConfig.EnterContext();

            try
            {
                mtxd_cfg_struct cfgStruct;
                cfgStruct.addImageGenMipmaps = mainwnd->addImageGenMipmaps;
                cfgStruct.lockDownTXDPlatform = mainwnd->lockDownTXDPlatform;

                mtxdConfig.writeStruct( cfgStruct );
            }
            catch( ... )
            {
                mtxdConfig.LeaveContext();

                throw;
            }

            mtxdConfig.LeaveContext();
        }

        // TxdGen config block.
        if ( const massconvEnv *massconv = massconvEnvRegister.GetConstPluginStruct( mainwnd ) )
        {
            rw::BlockProvider massconvBlock( &mainBlock );

            massconvBlock.EnterContext();

            try
            {
                RwWriteUnicodeString( massconvBlock, massconv->txdgenConfig.c_gameRoot );
                RwWriteUnicodeString( massconvBlock, massconv->txdgenConfig.c_outputRoot );

                txdgen_cfg_struct cfgStruct;
                cfgStruct.c_gameVersion = massconv->txdgenConfig.c_gameVersion;
                cfgStruct.c_targetPlatform = massconv->txdgenConfig.c_targetPlatform;
                cfgStruct.c_clearMipmaps = massconv->txdgenConfig.c_clearMipmaps;
                cfgStruct.c_generateMipmaps = massconv->txdgenConfig.c_generateMipmaps;
                cfgStruct.c_mipGenMode = massconv->txdgenConfig.c_mipGenMode;
                cfgStruct.c_mipGenMaxLevel = massconv->txdgenConfig.c_mipGenMaxLevel;
                cfgStruct.c_improveFiltering = massconv->txdgenConfig.c_improveFiltering;
                cfgStruct.compressTextures = massconv->txdgenConfig.compressTextures;
                cfgStruct.c_palRuntimeType = massconv->txdgenConfig.c_palRuntimeType;
                cfgStruct.c_dxtRuntimeType = massconv->txdgenConfig.c_dxtRuntimeType;
                cfgStruct.c_reconstructIMGArchives = massconv->txdgenConfig.c_reconstructIMGArchives;
                cfgStruct.c_fixIncompatibleRasters = massconv->txdgenConfig.c_fixIncompatibleRasters;
                cfgStruct.c_dxtPackedDecompression = massconv->txdgenConfig.c_dxtPackedDecompression;
                cfgStruct.c_imgArchivesCompressed = massconv->txdgenConfig.c_imgArchivesCompressed;
                cfgStruct.c_ignoreSerializationRegions = massconv->txdgenConfig.c_ignoreSerializationRegions;
                cfgStruct.c_compressionQuality = massconv->txdgenConfig.c_compressionQuality;
                cfgStruct.c_outputDebug = massconv->txdgenConfig.c_outputDebug;
                cfgStruct.c_warningLevel = massconv->txdgenConfig.c_warningLevel;
                cfgStruct.c_ignoreSecureWarnings = massconv->txdgenConfig.c_ignoreSecureWarnings;

                massconvBlock.writeStruct( cfgStruct );
            }
            catch( ... )
            {
                massconvBlock.LeaveContext();

                throw;
            }

            massconvBlock.LeaveContext();
        }
    }

    inline void Initialize( MainWindow *mainwnd )
    {
        // First create a translator that resides in the application path.
        HMODULE appHandle = GetModuleHandle( NULL );

        wchar_t pathBuffer[ 1024 ];

        DWORD pathLen = GetModuleFileNameW( appHandle, pathBuffer, NUMELMS( pathBuffer ) );

        this->appRoot = mainwnd->fileSystem->CreateTranslator( pathBuffer );

        // Now create the root to the current directory.
        this->toolRoot = fileRoot;

        // Load the serialization.
        rw::Interface *rwEngine = mainwnd->GetEngine();

        try
        {
            CFile *configFile = this->appRoot->Open( L"app.bin", L"rb" );

            if ( configFile )
            {
                try
                {
                    rw::Stream *rwStream = RwStreamCreateTranslated( rwEngine, configFile );

                    if ( rwStream )
                    {
                        try
                        {
                            rw::BlockProvider mainCfgBlock( rwStream, rw::RWBLOCKMODE_READ );

                            mainCfgBlock.EnterContext();

                            try
                            {
                                // Read stuff.
                                if ( mainCfgBlock.getBlockID() == MAGICTXD_CONFIG_BLOCK )
                                {
                                    loadSerialization( mainCfgBlock, mainwnd );
                                }
                            }
                            catch( ... )
                            {
                                mainCfgBlock.LeaveContext();

                                throw;
                            }

                            mainCfgBlock.LeaveContext();
                        }
                        catch( ... )
                        {
                            rwEngine->DeleteStream( rwStream );

                            throw;
                        }

                        rwEngine->DeleteStream( rwStream );
                    }
                }
                catch( ... )
                {
                    delete configFile;

                    throw;
                }

                delete configFile;
            }
        }
        catch( rw::RwException& )
        {
            // Also ignore this error.
        }
    }

    inline void Shutdown( MainWindow *mainwnd )
    {
        // Write the status of the main window.
        rw::Interface *rwEngine = mainwnd->GetEngine();

        try
        {
            CFile *configFile = this->appRoot->Open( L"app.bin", L"wb" );

            if ( configFile )
            {
                try
                {
                    rw::Stream *rwStream = RwStreamCreateTranslated( rwEngine, configFile );

                    if ( rwStream )
                    {
                        try
                        {
                            // Write the serialization in RenderWare blocks.
                            rw::BlockProvider mainCfgBlock( rwStream, rw::RWBLOCKMODE_WRITE );

                            mainCfgBlock.EnterContext();

                            try
                            {
                                mainCfgBlock.setBlockID( MAGICTXD_CONFIG_BLOCK );

                                // Write shit.
                                saveSerialization( mainCfgBlock, mainwnd );
                            }
                            catch( ... )
                            {
                                mainCfgBlock.LeaveContext();

                                throw;
                            }

                            mainCfgBlock.LeaveContext();
                        }
                        catch( ... )
                        {
                            rwEngine->DeleteStream( rwStream );

                            throw;
                        }

                        rwEngine->DeleteStream( rwStream );
                    }
                }
                catch( ... )
                {
                    delete configFile;

                    throw;
                }

                delete configFile;
            }
        }
        catch( rw::RwException& )
        {
            // Ignore those errors.
            // Maybe log it somewhere.
        }

        // Destroy all root handles.
        delete this->appRoot;

        // Since we take the tool root from the CFileSystem module, we do not delete it.
    }
};

void InitializeGUISerialization( void )
{
    mainWindowFactory.RegisterDependantStructPlugin <mainWindowSerialization> ();
}