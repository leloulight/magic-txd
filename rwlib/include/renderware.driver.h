/*
    RenderWare driver virtualization framework.
    Drivers are a reconception of the original pipeline design of Criterion.
    A driver has its own copies of instanced geometry, textures and materials.
    Those objects have implicit references to instanced handles.

    If an atomic is asked to be rendered, it is automatically instanced.
    Instancing can be sheduled asynchronically way before the rendering to improve performance.
*/

typedef void* DriverRaster;
typedef void* DriverGeometry;
typedef void* DriverMaterial;

struct Driver;

struct DriverSwapChain
{
    friend struct Driver;

    inline DriverSwapChain( Interface *engineInterface, Driver *theDriver, Window *ownedWindow )
    {
        AcquireObject( ownedWindow );

        this->driver = theDriver;
        this->ownedWindow = ownedWindow;
    }

    inline ~DriverSwapChain( void )
    {
        ReleaseObject( this->ownedWindow );
    }

    Driver* GetDriver( void ) const
    {
        return this->driver;
    }

    // Returns the window that this swap chain is attached to.
    Window* GetAssociatedWindow( void ) const
    {
        return this->ownedWindow;
    }

    inline void* GetImplementation( void )
    {
        return ( this + 1 );
    }

private:
    Driver *driver;
    Window *ownedWindow;
};

struct Driver
{
    inline Driver( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
    }

    inline Driver( const Driver& right )
    {
        this->engineInterface = right.engineInterface;
    }

    // Swap chains are used to draw render target to a window.
    DriverSwapChain* CreateSwapChain( Window *outputWindow, uint32 frameCount );
    void DestroySwapChain( DriverSwapChain *swapChain );

    // Object creation API.
    // This creates instanced objects to use for rendering.
    DriverRaster* CreateInstancedRaster( Raster *sysRaster );

    inline void* GetImplementation( void )
    {
        return ( this + 1 );
    }

private:
    Interface *engineInterface;
};

Driver* CreateDriver( Interface *engineInterface, const char *typeName );
void DestroyDriver( Interface *engineInterface, Driver *theDriver );