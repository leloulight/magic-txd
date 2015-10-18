#include "mainwindow.h"

#include "massexport.h"

#include <QByteArray>

#include <ShlObj.h>

#include "guiserialization.hxx"

#define SERIALIZE_SECTOR        0x5158

// Utilities for connecting the GUI to a filesystem repository.
struct mainWindowSerialization
{
    // Serialization things.
    CFileTranslator *appRoot;       // directory the application module is in.
    CFileTranslator *toolRoot;      // the current directory we launched in
    CFileTranslator *configRoot;    // writ-able root for configuration files.

    inline void loadSerialization( rw::BlockProvider& mainBlock, MainWindow *mainwnd )
    {
        rw::uint32 blockCount = mainBlock.readUInt32();

        for ( rw::uint32 n = 0; n < blockCount; n++ )
        {
            rw::BlockProvider cfgBlock( &mainBlock );
            
            cfgBlock.EnterContext();

            try
            {
                try
                {
                    rw::uint32 block_id = cfgBlock.getBlockID();

                    unsigned short cfg_id = (unsigned short)( block_id & 0xFFFF );
                    unsigned short checksum = (unsigned short)( ( block_id >> 16 ) & 0xFFFF );

                    if ( checksum == SERIALIZE_SECTOR )
                    {
                        magicSerializationProvider *prov = FindMainWindowSerializer( mainwnd, cfg_id );

                        if ( prov )
                        {
                            prov->Load( mainwnd, cfgBlock );
                        }
                    }
                }
                catch( ... )
                {
                    cfgBlock.LeaveContext();

                    throw;
                }

                cfgBlock.LeaveContext();
            }
            catch( rw::RwException& )
            {
                // Some module failed to load.
                // Try to load all the other modules anyway.
            }
        }
    }

    inline void saveSerialization( rw::BlockProvider& mainBlock, const MainWindow *mainwnd )
    {
        mainBlock.writeUInt32( GetAmountOfMainWindowSerializers( mainwnd ) );

        ForAllMainWindowSerializers( mainwnd,
            [&]( magicSerializationProvider *prov, unsigned short id )
        {
            rw::BlockProvider cfgBlock( &mainBlock );

            cfgBlock.EnterContext();

            try
            {
                try
                {
                    cfgBlock.setBlockID( (rw::uint32)SERIALIZE_SECTOR << 16 | id );

                    prov->Save( mainwnd, cfgBlock );
                }
                catch( ... )
                {
                    cfgBlock.LeaveContext();

                    throw;
                }

                cfgBlock.LeaveContext();
            }
            catch( rw::RwException& )
            {
                // If one component failed to serialize, it doesnt mean that everything else should fail to serialize aswell.
                // Continue here.
            }
        });
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

        // We want a handle to a writable configuration directory.
        // If we have no such handle, we might aswell not write any configuration.
        {
            CFileTranslator *configRoot = NULL;

            configRoot = mainwnd->fileSystem->CreateTranslator( pathBuffer, DIR_FLAG_WRITABLE );

            if ( configRoot == NULL )
            {
                // We try getting a directory in the local user application data folder.
                PWSTR localAppDataPath = NULL;

                HRESULT resGetPath = SHGetKnownFolderPath( FOLDERID_LocalAppData, 0, NULL, &localAppDataPath );

                if ( SUCCEEDED(resGetPath) )
                {
                    // Maybe this one will work.
                    filePath dirPath( localAppDataPath );
                    dirPath += "/Magic.TXD config/";

                    CoTaskMemFree( localAppDataPath );

                    // We should create a folder there.
                    BOOL createFolderSuccess = CreateDirectoryW( dirPath.w_str(), NULL );

                    DWORD lastError = GetLastError();

                    if ( createFolderSuccess == TRUE || lastError == ERROR_ALREADY_EXISTS )
                    {
                        configRoot = mainwnd->fileSystem->CreateTranslator( dirPath, DIR_FLAG_WRITABLE );
                    }
                }
            }

            this->configRoot = configRoot;
        }

        // Load the serialization.
        rw::Interface *rwEngine = mainwnd->GetEngine();

        if ( CFileTranslator *configRoot = this->configRoot )
        {
            try
            {
                CFile *configFile = this->configRoot->Open( L"app.bin", L"rb" );

                if ( configFile )
                {
                    try
                    {
                        rw::Stream *rwStream = RwStreamCreateTranslated( rwEngine, configFile );

                        if ( rwStream )
                        {
                            try
                            {
                                rw::BlockProvider mainCfgBlock( rwStream, rw::RWBLOCKMODE_READ, false );    // We expect properly written blocks, always.

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
    }

    inline void Shutdown( MainWindow *mainwnd )
    {
        // Write the status of the main window.
        rw::Interface *rwEngine = mainwnd->GetEngine();

        if ( CFileTranslator *configRoot = this->configRoot )
        {
            try
            {
                CFile *configFile = configRoot->Open( L"app.bin", L"wb" );

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
        }

        // Destroy all root handles.
        if ( CFileTranslator *appRoot = this->appRoot )
        {
            delete appRoot;
        }
        
        if ( CFileTranslator *configRoot = this->configRoot )
        {
            delete configRoot;
        }

        // Since we take the tool root from the CFileSystem module, we do not delete it.
    }
};

void InitializeGUISerialization( void )
{
    mainWindowFactory.RegisterDependantStructPlugin <mainWindowSerialization> ();
}