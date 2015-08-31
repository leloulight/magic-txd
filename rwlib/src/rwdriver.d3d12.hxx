#ifndef _RENDERWARE_DIRECT3D12_DRIVER_
#define _RENDERWARE_DIRECT3D12_DRIVER_

#include "rwdriver.hxx"

#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <wrl/client.h>

using namespace Microsoft::WRL;

namespace rw
{

struct d3d12DriverInterface : public nativeDriverImplementation
{
    // Driver object definitions.
    // Constructed by a factory for plugin support.
    struct d3d12NativeDriver
    {
        Interface *engineInterface;

        d3d12NativeDriver( d3d12DriverInterface *env, Interface *engineInterface );
        d3d12NativeDriver( const d3d12NativeDriver& right );

        ~d3d12NativeDriver( void );

	    ComPtr<ID3D12Device> m_device;                      // DIRECT3D12!!!!
	    ComPtr<ID3D12CommandAllocator> m_commandAllocator;  // this is used to allocate the commands... ?
	    ComPtr<ID3D12CommandQueue> m_commandQueue;          // every device needs a main command queue.
	    ComPtr<ID3D12RootSignature> m_rootSignature;        // we try to get everything done with one root signature, for now.
	    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	    ComPtr<ID3D12DescriptorHeap> m_resourceViewHeap;

        // Cached values.
        UINT m_rtvDescriptorSize;
    };

    typedef StaticPluginClassFactory <d3d12NativeDriver> d3d12DriverFactory_t;

    static d3d12DriverFactory_t d3d12DriverFactory;

    struct d3d12NativeRaster
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeRaster( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeRaster( const d3d12NativeRaster& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeRaster( void )
        {

        }
    };

    struct d3d12NativeGeometry
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeGeometry( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeGeometry( const d3d12NativeGeometry& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeGeometry( void )
        {

        }
    };

    struct d3d12NativeMaterial
    {
        d3d12NativeDriver *driver;

        inline d3d12NativeMaterial( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver )
        {
            this->driver = driver;
        }

        inline d3d12NativeMaterial( const d3d12NativeMaterial& right )
        {
            this->driver = right.driver;
        }

        inline ~d3d12NativeMaterial( void )
        {

        }
    };

    struct d3d12SwapChain
    {
        d3d12NativeDriver *driver;

        d3d12SwapChain( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, Window *sysWnd, uint32 frameCount );
        d3d12SwapChain( const d3d12SwapChain& right );

        ~d3d12SwapChain( void );

        Window *sysWnd;

        ComPtr <IDXGISwapChain3> m_swapChain;
	    ComPtr <ID3D12DescriptorHeap> m_rtvHeap;
        std::vector <ComPtr <ID3D12Resource>> m_chainBuffers;

        // Current back buffer index.
        uint32 m_chainBufferIndex;
    };

    struct d3d12PipelineStateObject
    {
        d3d12NativeDriver *driver;

        // Makes no sense to clone an immutable object, so no copy constructor.

        d3d12PipelineStateObject( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, const gfxGraphicsState& psoState );
        ~d3d12PipelineStateObject( void );

        ComPtr <ID3D12RootSignature> rootSignature;
        ComPtr <ID3D12PipelineState> psoPtr;
    };

    // Special entry points.
    typedef HRESULT (WINAPI*D3D12CreateDevice_t)(
        IUnknown *pAdapter,
        D3D_FEATURE_LEVEL MinimumFeatureLevel,
        REFIID riid,
        void **ppDevice
    );
    typedef HRESULT (WINAPI*CreateDXGIFactory1_t)(
        REFIID riid,
        void **ppFactory
    );
#ifdef _DEBUG
    typedef HRESULT (WINAPI*D3D12GetDebugInterface_t)( REFIID riif, void **ppvDebug );
#endif //_DEBUG
    typedef HRESULT (WINAPI*D3D12SerializeRootSignature_t)(
        const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
        D3D_ROOT_SIGNATURE_VERSION Version,
        ID3DBlob **ppBlob,
        ID3DBlob **ppErrorBlob
    );

    HMODULE d3d12Module;
    HMODULE dxgiModule;

    D3D12CreateDevice_t D3D12CreateDevice;
    CreateDXGIFactory1_t CreateDXGIFactory1;
#ifdef _DEBUG
    D3D12GetDebugInterface_t D3D12GetDebugInterface;
#endif //_DEBUG
    D3D12SerializeRootSignature_t D3D12SerializeRootSignature;

    struct driverFactoryConstructor
    {
        inline driverFactoryConstructor( d3d12DriverInterface *env, Interface *engineInterface )
        {
            this->env = env;
            this->engineInterface = engineInterface;
        }

        inline d3d12NativeDriver* Construct( void *mem ) const
        {
            return new (mem) d3d12NativeDriver( this->env, this->engineInterface );
        }

        d3d12DriverInterface *env;
        Interface *engineInterface;
    };

    // Driver object construction.
    void OnDriverConstruct( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) override
    {
        driverFactoryConstructor constructor( this, engineInterface );

        d3d12DriverFactory.ConstructPlacementEx( driverObjMem, constructor );
    }

    void OnDriverCopyConstruct( Interface *engineInterface, void *driverObjMem, const void *srcDriverObjMem, size_t driverMemSize ) override
    {
        d3d12DriverFactory.ClonePlacement( driverObjMem, (const d3d12NativeDriver*)srcDriverObjMem );
    }

    void OnDriverDestroy( Interface *engineInterface, void *driverObjMem, size_t driverMemSize ) override
    {
        d3d12DriverFactory.DestroyPlacement( (d3d12NativeDriver*)driverObjMem );
    }

    // Object construction.
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Raster, d3d12NativeRaster, d3d12NativeDriver );
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Geometry, d3d12NativeGeometry, d3d12NativeDriver );
    NATIVE_DRIVER_OBJ_CONSTRUCT_IMPL( Material, d3d12NativeMaterial, d3d12NativeDriver );

    // Object instancing forward declarations.
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Raster );
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Geometry );
    NATIVE_DRIVER_DEFINE_INSTANCING_FORWARD( Material );

    // Special swap chain object.
    NATIVE_DRIVER_SWAPCHAIN_CONSTRUCT() override
    {
        new (objMem) d3d12SwapChain( this, engineInterface, (d3d12NativeDriver*)driverObjMem, sysWnd, frameCount );
    }
    NATIVE_DRIVER_SWAPCHAIN_DESTROY() override
    {
        ((d3d12SwapChain*)objMem)->~d3d12SwapChain();
    }

    // Taking care of the pipeline state object mappings.
    NATIVE_DRIVER_GRAPHICS_STATE_CONSTRUCT()
    {
        new (objMem) d3d12PipelineStateObject( this, engineInterface, (d3d12NativeDriver*)driverObjMem, gfxState );
    }
    NATIVE_DRIVER_GRAPHICS_STATE_DESTROY()
    {
        ((d3d12PipelineStateObject*)objMem)->~d3d12PipelineStateObject();
    }

    bool hasRegisteredDriver;

    inline void InitializeD3D12Module( void )
    {
        this->d3d12Module = LoadLibraryA( "d3d12.dll" );

        if ( HMODULE theModule = this->d3d12Module )
        {
            // Initialize the Direct3D environment.
            this->D3D12CreateDevice = (D3D12CreateDevice_t)GetProcAddress( theModule, "D3D12CreateDevice" );
#ifdef _DEBUG
            this->D3D12GetDebugInterface = (D3D12GetDebugInterface_t)GetProcAddress( theModule, "D3D12GetDebugInterface" );
#endif //_DEBUG
            this->D3D12SerializeRootSignature = (D3D12SerializeRootSignature_t)GetProcAddress( theModule, "D3D12SerializeRootSignature" );
        }
        else
        {
            this->D3D12CreateDevice = NULL;
#ifdef _DEBUG
            this->D3D12GetDebugInterface = NULL;
#endif //_DEBUG
            this->D3D12SerializeRootSignature = NULL;
        }
    }

    inline void InitializeDXGIModule( void )
    {
        this->dxgiModule = LoadLibraryA( "dxgi.dll" );

        if ( HMODULE theModule = this->dxgiModule )
        {
            this->CreateDXGIFactory1 = (CreateDXGIFactory1_t)GetProcAddress( theModule, "CreateDXGIFactory1" );
        }
        else
        {
            this->CreateDXGIFactory1 = NULL;
        }
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        bool hasRegistered = false;

        // Initialize modules.
        this->InitializeD3D12Module();
        this->InitializeDXGIModule();

        // We have to be able to create a Direct3D 12 device to do things.
        if ( this->D3D12CreateDevice != NULL )
        {
#ifdef _DEBUG
            // Enable debugging layer, if available.
            if ( D3D12GetDebugInterface_t debugfunc = this->D3D12GetDebugInterface )
            {
                ComPtr<ID3D12Debug> debugController;

                HRESULT debugGet = debugfunc( IID_PPV_ARGS( &debugController ) );

                if ( SUCCEEDED(debugGet) )
                {
                    debugController->EnableDebugLayer();
                }
            }
#endif //_DEBUG

            driverConstructionProps props;
            props.rasterMemSize = sizeof( d3d12NativeRaster );
            props.geomMemSize = sizeof( d3d12NativeGeometry );
            props.matMemSize = sizeof( d3d12NativeMaterial );
            props.swapChainMemSize = sizeof( d3d12SwapChain );
            props.graphicsStateMemSize = sizeof( d3d12PipelineStateObject );

            hasRegistered = RegisterDriver( engineInterface, "Direct3D12", props, this, d3d12DriverFactory.GetClassSize() );
        }

        this->hasRegisteredDriver = hasRegistered;
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( this->hasRegisteredDriver )
        {
            UnregisterDriver( engineInterface, this );
        }

        if ( HMODULE theModule = this->dxgiModule )
        {
            FreeLibrary( theModule );
        }

        if ( HMODULE theModule = this->d3d12Module )
        {
            FreeLibrary( theModule );
        }
    }
};

};

#endif //_RENDERWARE_DIRECT3D12_DRIVER_