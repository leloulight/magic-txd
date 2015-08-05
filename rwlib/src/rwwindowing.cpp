#include "StdInc.h"

#include "pluginutil.hxx"

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

struct windowingSystemWin32
{
    WNDCLASSEXW _windowClass;
    ATOM _windowClassHandle;

    static inline windowingSystemWin32* GetWindowingSystem( Interface *engineInterface )
    {
        return win32Windowing.GetPluginStruct( (EngineInterface*)engineInterface );
    }

    // We use a special window object for Windows stuff.
    struct Win32Window : public Window
    {
        inline Win32Window( Interface *engineInterface, void *construction_params ) : Window( engineInterface, construction_params )
        {
            windowingSystemWin32 *windowSys = GetWindowingSystem( engineInterface );

            if ( !windowSys )
            {
                throw RwException( "win32 windowing system not initialized" );
            }

            this->windowSys = windowSys;

            // Determine what title our window should have.
            std::string windowTitle;

            if ( engineInterface->applicationName.length() != 0 )
            {
                windowTitle += engineInterface->applicationName;

                if ( engineInterface->applicationVersion.length() != 0 )
                {
                    windowTitle += " [" + engineInterface->applicationVersion + "]";
                }
            }
            else
            {
                windowTitle += "RenderWare";
            }

            // Append the RenderWare version.
            windowTitle += " [rwver: " + this->objVersion.toString() + "]";

            // Adjust the windowing system rectangle correctly.
            DWORD dwStyle = ( WS_OVERLAPPEDWINDOW );

            this->dwStyle = dwStyle;

            RECT windowRect = { 0, 0, this->clientWidth, this->clientHeight };
            AdjustWindowRect( &windowRect, dwStyle, false );

            // Create a window.
            HWND wndHandle = CreateWindowExA(
                NULL,
                (LPCSTR)windowSys->_windowClassHandle,
                windowTitle.c_str(),
                dwStyle,
                300, 300,
                windowRect.right - windowRect.left,
                windowRect.bottom - windowRect.top,
                NULL, NULL,
                GetModuleHandle( NULL ),
                NULL
            );

            if ( wndHandle == NULL )
            {
                throw RwException( "window creation failed" );
            }

            // Store our class link into the window.
            SetWindowLongPtr( wndHandle, GWLP_USERDATA, (LONG)this );
            
            this->windowHandle = wndHandle;

            LIST_INSERT( windowSys->windowList.root, node );
        }

        inline Win32Window( const Win32Window& right ) : Window( right )
        {
            throw RwException( "cannot clone win32 windows, yet" );

            this->windowSys = right.windowSys;

            LIST_INSERT( this->windowSys->windowList.root, node );
        }

        inline ~Win32Window( void )
        {
            // Remove us from the list of actively managed windows.
            LIST_REMOVE( node );

            // Remove our userdata pointer.
            SetWindowLongPtr( this->windowHandle, GWLP_USERDATA, NULL );

            // Destroy our window.
            DestroyWindow( this->windowHandle );
        }

        static LRESULT CALLBACK _rwwindow_message_handler_win32( HWND wndHandle, UINT uMsg, WPARAM wParam, LPARAM lParam )
        {
            Win32Window *wndClass = (Win32Window*)GetWindowLongPtr( wndHandle, GWLP_USERDATA );

            if ( wndClass )
            {
                AcquireObject( wndClass );

                bool hasHandled = false;

                switch( uMsg )
                {
                case WM_SIZE:
                    {
                        // Update our logical client area.
                        RECT clientRect;

                        BOOL gotClientRect = GetClientRect( wndHandle, &clientRect );

                        if ( gotClientRect )
                        {
                            wndClass->clientWidth = ( clientRect.right - clientRect.left );
                            wndClass->clientHeight = ( clientRect.bottom - clientRect.top );
                        }
                    }

                    // Tell the system that this window resized.
                    TriggerEvent( wndClass, WINDOW_RESIZE, NULL );

                    hasHandled = true;
                    break;
                case WM_CLOSE:
                    // We call an event.
                    TriggerEvent( wndClass, WINDOW_CLOSING, NULL );
                
                    hasHandled = true;
                    break;
                case WM_QUIT:
                    // We received a message for immediate termination.
                    // The application layer should get to the exit ASAP!
                    TriggerEvent( wndClass, WINDOW_QUIT, NULL );

                    hasHandled = true;
                    break;
                }

                ReleaseObject( wndClass );

                if ( hasHandled )
                {
                    return 0;
                }
            }

            return DefWindowProcW( wndHandle, uMsg, wParam, lParam );
        }

        windowingSystemWin32 *windowSys;

        HWND windowHandle;
        DWORD dwStyle;

        RwListEntry <Win32Window> node;
    };

    inline void Initialize( Interface *engineInterface )
    {
        // Register the windowing system class.
	    _windowClass = { 0 };
	    _windowClass.cbSize = sizeof(WNDCLASSEX);
	    _windowClass.style = CS_HREDRAW | CS_VREDRAW;
	    _windowClass.lpfnWndProc = Win32Window::_rwwindow_message_handler_win32;
	    _windowClass.hInstance = GetModuleHandle( NULL );
	    _windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	    _windowClass.lpszClassName = L"RwWindow_framework";
	    
        _windowClassHandle = RegisterClassExW(&_windowClass);

        // Register the window type.
        this->windowTypeInfo = engineInterface->typeSystem.RegisterStructType <Win32Window> ( "window", engineInterface->rwobjTypeInfo );

        if ( RwTypeSystem::typeInfoBase *windowTypeInfo = this->windowTypeInfo )
        {
            // Make sure we are an exclusive type.
            // This is because we have to use construction parameters.
            engineInterface->typeSystem.SetTypeInfoExclusive( windowTypeInfo, true );
        }

        LIST_CLEAR( windowList.root );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        // Destroy all yet active windows.
        // This will force destruction no matter what.
        // It would be best if the game destroyed the window itself.
        while ( !LIST_EMPTY( windowList.root ) )
        {
            Win32Window *wndHandle = LIST_GETITEM( Win32Window, windowList.root.next, node );

            engineInterface->DeleteRwObject( wndHandle );
        }

        if ( RwTypeSystem::typeInfoBase *windowTypeInfo = this->windowTypeInfo )
        {
            engineInterface->typeSystem.DeleteType( windowTypeInfo );
        }

        // Unregister our window class.
        UnregisterClassA( (LPCSTR)this->_windowClassHandle, GetModuleHandle( NULL ) );
    }

    inline void Update( void )
    {
        // We fetch messages of all active windows.
        MSG msg;

        unsigned int msgFetched = 0;

		while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );

            msgFetched++;

            // If we fetched more than 50 messages, we should stop.
            // Let's try more next frame.
            if ( msgFetched > 50 )
            {
                break;
            }
		}
    }

    RwTypeSystem::typeInfoBase *windowTypeInfo;

    static PluginDependantStructRegister <windowingSystemWin32, RwInterfaceFactory_t> win32Windowing;

    // List of all registered windows.
    RwList <Win32Window> windowList;
};

PluginDependantStructRegister <windowingSystemWin32, RwInterfaceFactory_t> windowingSystemWin32::win32Windowing;

// General window routines.
Window* MakeWindow( Interface *engineInterface, uint32 clientWidth, uint32 clientHeight )
{
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