#include <renderware.h>

namespace rw
{
    LibraryVersion app_version( void )
    {
        LibraryVersion ver;

        ver.rwLibMajor = 3;
        ver.rwLibMinor = 7;
        ver.rwRevMajor = 0;
        ver.rwRevMinor = 0;

        return ver;
    }

    static void window_closing_event_handler( RwObject *obj, event_t eventID, void *callbackData, void *ud )
    {
        // We request destruction of the window.
        obj->engineInterface->DeleteRwObject( obj );
    }

    int32 rwmain( Interface *engineInterface )
    {
        // Give information about the running application to the runtime.
        softwareMetaInfo metaInfo;
        metaInfo.applicationName = "RenderWare Sample";
        metaInfo.applicationVersion = "test";
        metaInfo.description = "A test application for the rwtools runtime";

        engineInterface->SetApplicationInfo( metaInfo );

        // Create a window and render into it.
        Window *rwWindow = MakeWindow( engineInterface, 640, 480 );

        if ( !rwWindow )
            return -1;

        uint32 wndBaseRefCount = GetRefCount( rwWindow );

        // We hold an extra reference.
        AcquireObject( rwWindow );

        // Handle the window closing event.
        RegisterEventHandler( rwWindow, event_t::WINDOW_CLOSING, window_closing_event_handler );
        RegisterEventHandler( rwWindow, event_t::WINDOW_QUIT, window_closing_event_handler );

        // Show the window, since we have set it up by now.
        rwWindow->SetVisible( true );
        
        // Create the game renderer.

        // Execute the main loop
        while ( GetRefCount( rwWindow ) > wndBaseRefCount )   // we wait until somebody requested to destroy the window.
        {
            // Draw the game scene.

            // Give cycles to the window manager.
            // In the multi-threaded environment, this will effectively be a no-op.
            PulseWindowingSystem( engineInterface );

            // We want to give some cycles to the OS.
            // Otherwise our thread would starve everything.
            YieldExecution( 1 );
        }

        // Release our window reference.
        // This will destroy it.
        engineInterface->DeleteRwObject( rwWindow );

        return 0;
    }
};

// We use the windows subsystem.
BOOL WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow )
{
    // Redirect to RenderWare.
    return rw::frameworkEntryPoint_win32( hInst, hPrevInst, lpCmdLine, nCmdShow );
}