#include "mainwindow.h"

#define MAGICTXD_UNICODE_STRING_ID      0xBABE0001
#define MAGICTXD_CONFIG_BLOCK           0xBABE0002

// Our string blocks.
void RwWriteUnicodeString( rw::BlockProvider& prov, const std::wstring& in )
{
    rw::BlockProvider stringBlock( &prov, false );

    stringBlock.EnterContext();

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

// Utilities for connecting the GUI to a filesystem repository.
struct mainWindowSerialization
{
    // Serialization things.
    CFileTranslator *appRoot;       // directory the application module is in.
    CFileTranslator *toolRoot;      // the current directory we launched in

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
    }

    inline void saveSerialization( rw::BlockProvider& mainBlock, const MainWindow *mainwnd )
    {
        RwWriteUnicodeString( mainBlock, mainwnd->lastTXDSaveDir.toStdWString() );
        RwWriteUnicodeString( mainBlock, mainwnd->lastImageFileOpenDir.toStdWString() );
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
                    rw::Stream *rwStream = RwStreamCreateTranslated( mainwnd, configFile );

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
                    rw::Stream *rwStream = RwStreamCreateTranslated( mainwnd, configFile );

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