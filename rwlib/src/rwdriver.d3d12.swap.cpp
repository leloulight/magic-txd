#include "StdInc.h"

#include "rwdriver.d3d12.hxx"

// We need to know about the system window type.
#include "rwwindowing.hxx"

namespace rw
{

static void _swapchain_window_resize_event( RwObject *evtObj, event_t eventID, void *cbData, void *ud )
{
    // TODO: recreate the swap chain buffers, because the window has resized.
}

d3d12DriverInterface::d3d12SwapChain::d3d12SwapChain( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, Window *sysWnd, uint32 frameCount )
{
    this->driver = driver;
    this->sysWnd = sysWnd;

    CreateDXGIFactory1_t factCreate = env->CreateDXGIFactory1;

    if ( !factCreate )
    {
        throw RwException( "cannot find DXGI entry point (CreateDXGIFactory1)" );
    }

    // We need to initialize a factory to create the swap chain on.
    ComPtr <IDXGIFactory1> dxgiFact;

    HRESULT factorySuccess = factCreate( IID_PPV_ARGS( &dxgiFact ) );

    if ( !SUCCEEDED(factorySuccess) )
    {
        throw RwException( "failed to create DXGI factory" );
    }

    // Get the native interface of the window.
    windowingSystemWin32::Win32Window *wndNative = (windowingSystemWin32::Win32Window*)sysWnd;

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;
    swapChainDesc.BufferDesc.Width = sysWnd->GetClientWidth();
    swapChainDesc.BufferDesc.Height = sysWnd->GetClientHeight();
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = wndNative->windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    // Create the swap chain.
    ComPtr <IDXGISwapChain> abstractSwapChain;

    HRESULT swapChainSuccess =
        dxgiFact->CreateSwapChain(
            driver->m_commandQueue.Get(),
            &swapChainDesc,
            &abstractSwapChain
        );

    if ( !SUCCEEDED(swapChainSuccess) )
    {
        throw RwException( "failed to create Direct3D 12 swap chain" );
    }

    HRESULT castToNewSwapChain =
        abstractSwapChain.As( &m_swapChain );

    if ( !SUCCEEDED(castToNewSwapChain) )
    {
        throw RwException( "failed to cast old style swap chain to new style" );
    }

    this->m_chainBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    this->m_chainBuffers.resize( frameCount );

    ID3D12Device *device = driver->m_device.Get();

    // Create a resource view heap just for this swap chain.
    // We could do this more efficiently in the future with a view heap manager, but we want to get something working done first!
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = frameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT heapCreateSuccess = device->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap) );

        if ( !SUCCEEDED(heapCreateSuccess) )
        {
            throw RwException( "failed to create RTV descriptor heap for Direct3D 12 device swap chain" );
        }
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle( m_rtvHeap->GetCPUDescriptorHandleForHeapStart() );

        // Create a RTV for each frame.
        for (UINT n = 0; n < frameCount; n++)
        {
            HRESULT bufferGetSuccess = m_swapChain->GetBuffer( n, IID_PPV_ARGS(&m_chainBuffers[n]) );

            if ( !SUCCEEDED(bufferGetSuccess) )
            {
                throw RwException( "failed to get Direct3D 12 swap chain render target surface" );
            }

            device->CreateRenderTargetView( m_chainBuffers[n].Get(), nullptr, rtvHandle );

            // Increment the handle.
            rtvHandle.Offset( 1, driver->m_rtvDescriptorSize );
        }
    }

    // If the window is resized, we want to know about it.
    RegisterEventHandler( sysWnd, event_t::WINDOW_RESIZE, _swapchain_window_resize_event );
}

d3d12DriverInterface::d3d12SwapChain::d3d12SwapChain( const d3d12SwapChain& right )
{
    this->driver = right.driver;

    throw RwException( "cloning Direct3D 12 swapchains is not supported" );
}

d3d12DriverInterface::d3d12SwapChain::~d3d12SwapChain( void )
{
    // Unregister our event handler.
    UnregisterEventHandler( this->sysWnd, event_t::WINDOW_RESIZE, _swapchain_window_resize_event );

    // Resources are released automatically.
}

};