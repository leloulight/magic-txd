#include "StdInc.h"

#include "rwdriver.d3d12.hxx"

#include "pluginutil.hxx"

namespace rw
{

d3d12DriverInterface::d3d12NativeDriver::d3d12NativeDriver( d3d12DriverInterface *env, Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;
    
    this->engineInterface = engineInterface;
    
    // Initialize the Direct3D 12 device.
    HRESULT deviceSuccess = 
        env->D3D12CreateDevice(
            NULL,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS( &this->m_device )
        );

    if ( !SUCCEEDED(deviceSuccess) )
    {
        throw RwException( "failed to allocate Direct3D 12 device handle" );
    }

    // Build our main command queue.
    // You issue render commands with this one, basically everything.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT commandQueueSuccess =
        m_device->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &this->m_commandQueue ) );

    if ( !SUCCEEDED(commandQueueSuccess) )
    {
        throw RwException( "failed to create main command queue for Direct3D 12 device" );
    }

    // Allocate sub resources.
    HRESULT commandAllocatorSuccess = 
        m_device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator) );

    if ( !SUCCEEDED(commandAllocatorSuccess) )
    {
        throw RwException( "failed to allocate Direct3D 12 main command allocator" );
    }

    // Cache some things.
    this->m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

    // Success!
}

d3d12DriverInterface::d3d12NativeDriver::d3d12NativeDriver( const d3d12NativeDriver& right )
{
    this->engineInterface = right.engineInterface;

    throw RwException( "cannot clone a Direct3D 12 driver" );
}

d3d12DriverInterface::d3d12NativeDriver::~d3d12NativeDriver( void )
{
    // We automatically release main device handles.
    // TODO: release advanced device resources (rasters, geometries, etc)
}

d3d12DriverInterface::d3d12DriverFactory_t d3d12DriverInterface::d3d12DriverFactory;

static PluginDependantStructRegister <d3d12DriverInterface, RwInterfaceFactory_t> d3d12DriverReg;

void registerD3D12DriverImplementation( void )
{
    // Put the driver into the ecosystem.
    d3d12DriverReg.RegisterPlugin( engineFactory );

    // TODO: register all driver plugins.
}

};