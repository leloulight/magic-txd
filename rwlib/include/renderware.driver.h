/*
    RenderWare driver virtualization framework.
    Drivers are a reconception of the original pipeline design of Criterion.
    A driver has its own copies of instanced geometry, textures and materials.
    Those objects have implicit references to instanced handles.

    If an atomic is asked to be rendered, it is automatically instanced.
    Instancing can be sheduled asynchronically way before the rendering to improve performance.
*/

// Types of primitives that can be rendered by any driver.
enum class ePrimitiveTopology : uint32
{
    POINTLIST,
    LINELIST,
    LINESTRIP,
    TRIANGLELIST,
    TRIANGLESTRIP
};

// Types of vertex attribute values that are supported by any driver.
enum class eVertexAttribValueType : uint32
{
    INT8,
    INT16,
    INT32,
    UINT8,
    UINT16,
    UINT32,
    FLOAT32
};

// Type of vertex attribute semantic.
// This defines how a vertex attribute should be used in the pipeline.
enum class eVertexAttribSemanticType : uint32
{
    POSITION,
    COLOR,
    NORMAL,
    TEXCOORD
};

// Defines a vertex declaration.
// The driver can create a vertex attrib descriptor out of an array of these.
struct vertexAttrib
{
    eVertexAttribSemanticType semanticType;     // binding point semantic type (required for vertex shader processing)
    eVertexAttribValueType attribType;          // value type
    uint32 count;                               // count of the values of attribType value type
    uint32 alignedByteOffset;                   // byte offset in the vertex data item aligned by the attrib type byte size
};

// Cached vertex attribute desc.
struct DriverVertexDeclaration
{
    // API to query the vertex attributes of this object.
    uint32 GetAttributeCount( void ) const;
    bool GetAttributes( vertexAttrib *outBuf, uint32 bufcount ) const;

    // This is an immutable object.
};

// Instanced object types.
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

    inline Interface* GetEngineInterface( void ) const
    {
        return this->engineInterface;
    }

    // Capability API.
    bool SupportsPrimitiveTopology( ePrimitiveTopology topology ) const;
    bool SupportsVertexAttribute( eVertexAttribValueType attribType ) const;

    // Vertex attribute API.
    DriverVertexDeclaration* CreateVertexDeclaration( vertexAttrib *attribs, uint32 attribCount );
    void DeleteVertexDeclaration( DriverVertexDeclaration *decl );

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