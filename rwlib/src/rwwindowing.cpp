#include "StdInc.h"

#include "rwwindowing.hxx"

namespace rw
{

struct window_construction_params
{
    uint32 clientWidth, clientHeight;
};

Window::Window( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
{
    window_construction_params *params = (window_construction_params*)construction_params;

    this->clientWidth = params->clientWidth;
    this->clientHeight = params->clientHeight;
}

Window::Window( const Window& right ) : RwObject( right )
{
    this->clientWidth = right.clientWidth;
    this->clientHeight = right.clientHeight;
}

Window::~Window( void )
{
    // Nothing to do here.
}

PluginDependantStructRegister <windowingSystemWin32, RwInterfaceFactory_t> windowingSystemWin32::win32Windowing;

// General window routines.
Window* MakeWindow( Interface *intf, uint32 clientWidth, uint32 clientHeight )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    Window *handle = NULL;

    windowingSystemWin32 *winSys = windowingSystemWin32::GetWindowingSystem( engineInterface );

    if ( winSys )
    {
        window_construction_params params;
        params.clientWidth = clientWidth;
        params.clientHeight = clientHeight;

        GenericRTTI *winObj = engineInterface->typeSystem.Construct( engineInterface, winSys->windowTypeInfo, &params );
        
        if ( winObj )
        {
            windowingSystemWin32::Win32Window *win32Wnd = (windowingSystemWin32::Win32Window*)RwTypeSystem::GetObjectFromTypeStruct( winObj );

            // The window is fully created.
            // Signal the messaging thread.
            win32Wnd->KickOffMessagingThread();

            handle = win32Wnd;
        }
    }

    return handle;
}

void Window::SetVisible( bool vis )
{
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    ShowWindow( window->windowHandle, vis ? SW_SHOW : SW_HIDE );
}

bool Window::IsVisible( void ) const
{
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    BOOL isVisible = IsWindowVisible( window->windowHandle );

    return ( isVisible ? true : false );
}

void Window::SetClientSize( uint32 clientWidth, uint32 clientHeight )
{
    windowingSystemWin32::Win32Window *window = (windowingSystemWin32::Win32Window*)this;

    scoped_rwlock_writer <rwlock> wndLock( window->wndPropertyLock );

    // Transform it into a client rect.
    RECT wndRect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect( &wndRect, window->dwStyle, false );

    uint32 windowWidth = ( wndRect.right - wndRect.left );
    uint32 windowHeight = ( wndRect.bottom - wndRect.top );

    SetWindowPos( window->windowHandle, NULL, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
}

void PulseWindowingSystem( Interface *engineInterface )
{
    windowingSystemWin32 *wndSys = windowingSystemWin32::GetWindowingSystem( engineInterface );

    if ( wndSys )
    {
        wndSys->Update();
    }
}

void YieldExecution( uint32 ms )
{
    Sleep( ms );
}

// Windowing system registration function.
// Fow now Windows OS only.
void registerWindowingSystem( void )
{
    windowingSystemWin32::win32Windowing.RegisterPlugin( engineFactory );
}

};