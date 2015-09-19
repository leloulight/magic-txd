#include "mainwindow.h"

// Utilities for connecting the GUI to a filesystem repository.
struct mainWindowSerialization
{
    // Serialization things.
    CFileTranslator *appRoot;       // directory the application module is in.
    CFileTranslator *toolRoot;      // the current directory we launched in

    inline void Initialize( MainWindow *mainwnd )
    {
        // First create a translator that resides in the application path.
        HMODULE appHandle = GetModuleHandle( NULL );
        
        wchar_t pathBuffer[ 1024 ];

        DWORD pathLen = GetModuleFileNameW( appHandle, pathBuffer, NUMELMS( pathBuffer ) );

        this->appRoot = mainwnd->fileSystem->CreateTranslator( pathBuffer );

        // Now create the root to the current directory.
        this->toolRoot = fileRoot;
    }

    inline void Shutdown( MainWindow *mainwnd )
    {
        // Destroy all root handles.
        delete this->appRoot;

        // Since we take the tool root from the CFileSystem module, we do not delete it.
    }
};

void InitializeGUISerialization( void )
{
    mainWindowFactory.RegisterDependantStructPlugin <mainWindowSerialization> ();
}