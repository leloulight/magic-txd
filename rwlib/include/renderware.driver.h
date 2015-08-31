/*
    RenderWare driver virtualization framework.
    Drivers are a reconception of the original pipeline design of Criterion.
    A driver has its own copies of instanced geometry, textures and materials.
    Those objects have implicit references to instanced handles.

    If an atomic is asked to be rendered, it is automatically instanced.
    Instancing can be sheduled asynchronically way before the rendering to improve performance.
*/

// Virtual driver device states.
typedef uint32 rwDeviceValue_t;

enum rwFogModeState : rwDeviceValue_t
{
    RWFOG_DISABLE,
    RWFOG_LINEAR,
    RWFOG_EXP,
    RWFOG_EXP2
};
enum rwShadeModeState : rwDeviceValue_t
{
    RWSHADE_FLAT = 1,
    RWSHADE_GOURAUD
};
enum rwStencilOpState : rwDeviceValue_t
{
    RWSTENCIL_KEEP = 1,
    RWSTENCIL_ZERO,
    RWSTENCIL_REPLACE,
    RWSTENCIL_INCRSAT,
    RWSTENCIL_DECRSAT,
    RWSTENCIL_INVERT,
    RWSTENCIL_INCR,
    RWSTENCIL_DECR
};
enum rwCompareOpState : rwDeviceValue_t
{
    RWCMP_NEVER = 1,
    RWCMP_LESS,
    RWCMP_EQUAL,
    RWCMP_LESSEQUAL,
    RWCMP_GREATER,
    RWCMP_NOTEQUAL,
    RWCMP_GREATEREQUAL,
    RWCMP_ALWAYS
};
enum rwCullModeState : rwDeviceValue_t
{
    RWCULL_NONE = 1,
    RWCULL_CLOCKWISE,
    RWCULL_COUNTERCLOCKWISE
};
enum rwBlendModeState : rwDeviceValue_t
{
    RWBLEND_ZERO = 1,
    RWBLEND_ONE,
    RWBLEND_SRCCOLOR,
    RWBLEND_INVSRCCOLOR,
    RWBLEND_SRCALPHA,
    RWBLEND_INVSRCALPHA,
    RWBLEND_DESTALPHA,
    RWBLEND_INVDESTALPHA,
    RWBLEND_DESTCOLOR,
    RWBLEND_INVDESTCOLOR,
    RWBLEND_SRCALPHASAT
};
enum rwBlendOp : rwDeviceValue_t
{
    RWBLENDOP_ADD,
    RWBLENDOP_SUBTRACT,
    RWBLENDOP_REV_SUBTRACT,
    RWBLENDOP_MIN,
    RWBLENDOP_MAX
};
enum rwLogicOp : rwDeviceValue_t
{
    RWLOGICOP_CLEAR,
    RWLOGICOP_SET,
    RWLOGICOP_COPY,
    RWLOGICOP_COPY_INV,
    RWLOGICOP_NOOP,
    RWLOGICOP_INVERT,
    RWLOGICOP_AND,
    RWLOGICOP_NAND,
    RWLOGICOP_OR,
    RWLOGICOP_NOR,
    RWLOGICOP_XOR,
    RWLOGICOP_EQUIV,
    RWLOGICOP_AND_REVERSE,
    RWLOGICOP_AND_INV,
    RWLOGICOP_OR_REVERSE,
    RWLOGICOP_OR_INV
};
enum rwRenderFillMode : rwDeviceValue_t
{
    RWFILLMODE_WIREFRAME,
    RWFILLMODE_SOLID
};

// The actual primitive topology that is being input.
// This is basically a stronger declaration that the topology type.
enum class ePrimitiveTopology : uint32
{
    POINTLIST,
    LINELIST,
    LINESTRIP,
    TRIANGLELIST,
    TRIANGLESTRIP
};

// Vaguelly says what a graphics state should render.
enum class ePrimitiveTopologyType : uint32
{
    POINT,
    LINE,
    TRIANGLE,
    PATCH
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
    uint32 semanticIndex;                       // there are multiple variants of semanticType (tex coord 0, tex coord 1, color target 0, etc)
    eVertexAttribValueType attribType;          // value type
    uint32 count;                               // count of the values of attribType value type
    uint32 alignedByteOffset;                   // byte offset in the vertex data item aligned by the attrib type byte size
};

// Driver graphics state objects.
struct gfxBlendState
{
    inline gfxBlendState( void )
    {
        this->enableBlend = false;
        this->enableLogicOp = false;
        this->srcBlend = RWBLEND_ONE;
        this->dstBlend = RWBLEND_ZERO;
        this->blendOp = RWBLENDOP_ADD;
        this->srcAlphaBlend = RWBLEND_ONE;
        this->dstAlphaBlend = RWBLEND_ZERO;
        this->alphaBlendOp = RWBLENDOP_ADD;
        this->logicOp = RWLOGICOP_NOOP;
    }

    bool enableBlend;
    bool enableLogicOp;
    rwBlendModeState srcBlend;
    rwBlendModeState dstBlend;
    rwBlendOp blendOp;
    rwBlendModeState srcAlphaBlend;
    rwBlendModeState dstAlphaBlend;
    rwBlendOp alphaBlendOp;
    rwLogicOp logicOp;
};

struct gfxRasterizerState
{
    inline gfxRasterizerState( void )
    {
        this->fillMode = RWFILLMODE_SOLID;
        this->cullMode = RWCULL_CLOCKWISE;
        this->depthBias = 0;
        this->depthBiasClamp = 0;
        this->slopeScaledDepthBias = 0.0f;
    }

    rwRenderFillMode fillMode;
    rwCullModeState cullMode;
    int32 depthBias;
    float depthBiasClamp;
    float slopeScaledDepthBias;
};

struct gfxDepthStencilOpState
{
    inline gfxDepthStencilOpState( void )
    {
        this->failOp = RWSTENCIL_KEEP;
        this->depthFailOp = RWSTENCIL_KEEP;
        this->passOp = RWSTENCIL_KEEP;
        this->func = RWCMP_NEVER;
    }

    rwStencilOpState failOp;
    rwStencilOpState depthFailOp;
    rwStencilOpState passOp;
    rwCompareOpState func;
};

struct gfxDepthStencilState
{
    inline gfxDepthStencilState( void )
    {
        this->enableDepthTest = false;
        this->enableDepthWrite = false;
        this->depthFunc = RWCMP_NEVER;
        this->enableStencilTest = false;
        this->stencilReadMask = std::numeric_limits <uint8>::max();
        this->stencilWriteMask = std::numeric_limits <uint8>::max();
    }

    bool enableDepthTest;
    bool enableDepthWrite;
    rwCompareOpState depthFunc;
    bool enableStencilTest;
    uint8 stencilReadMask;
    uint8 stencilWriteMask;
    gfxDepthStencilOpState frontFace;
    gfxDepthStencilOpState backFace;
};

// Driver supported sample value types.
// Hopefully we can map most of them to eRasterFormat!
enum class gfxRasterFormat
{
    UNSPECIFIED,

    // Some very high quality formats for when you really need it.
    R32G32B32A32_TYPELESS,
    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32_TYPELESS,
    R32G32B32_FLOAT,
    R32G32B32_UINT,
    R32G32B32_SINT,

    // RenderWare old-style formats.
    R8G8B8A8,
    B5G6R5,
    B5G5R5A1,
    LUM8,       // actually red component.
    DEPTH16,
    DEPTH24,
    DEPTH32,

    // it goes without saying that any format that is not listed here
    // will have to be software processed into a hardware compatible format.

    // Good old DXT.
    DXT1,
    DXT3,       // same as DXT2
    DXT5,       // same as DXT4
};

enum class gfxMappingDeclare
{
    CONSTANT_BUFFER,    // a buffer that should stay constant across many draws
    SHADER_RESOURCE,    // a buffer that should stay constant across its lifetime
    DYNAMIC_BUFFER,     // a mutable buffer, both by CPU and GPU logic
    SAMPLER,            // a sampler definition.
};

enum class gfxRootParameterType
{
    DESCRIPTOR_TABLE,   // multiple dynamically resolved declarations
    CONSTANT_BUFFER,    // a single constant buffer
    SHADER_RESOURCE,    // a single shader resource
    DYNAMIC_BUFFER,     // a single dynamic buffer
};

struct gfxDescriptorRange
{
    gfxMappingDeclare contentType;
    uint32 numPointers;
    uint32 baseShaderRegister;
    uint32 heapOffset;
};

enum class gfxPipelineVisibilityType
{
    ALL,
    VERTEX_SHADER,
    HULL_SHADER,
    DOMAIN_SHADER,
    GEOM_SHADER,
    PIXEL_SHADER
};

struct gfxRootParameter
{
    gfxRootParameterType slotType;
    union
    {
        struct  // the default way to map resource usage
        {
            uint32 numRanges;
            const gfxDescriptorRange *ranges;
        } descriptor_table;
        struct  // limited mapping using inline 32bit values
        {
            uint32 shaderRegister;
            uint32 numValues;
        } constants;
        struct  // a pointer that is stored in the root signature
        {
            uint32 shaderRegister;
        } descriptor;
    };
    gfxPipelineVisibilityType visibility;
};

struct gfxStaticSampler
{
    inline gfxStaticSampler( void )
    {
        this->filterMode = RWFILTER_POINT;
        this->uAddressing = RWTEXADDRESS_WRAP;
        this->vAddressing = RWTEXADDRESS_WRAP;
        this->wAddressing = RWTEXADDRESS_WRAP;
        this->mipLODBias = 0.0f;
        this->maxAnisotropy = 16;
        this->minLOD = std::numeric_limits <float>::min();
        this->maxLOD = std::numeric_limits <float>::max();
        this->shaderRegister = 0;
        this->visibility = gfxPipelineVisibilityType::ALL;
    }

    eRasterStageFilterMode filterMode;
    eRasterStageAddressMode uAddressing;
    eRasterStageAddressMode vAddressing;
    eRasterStageAddressMode wAddressing;
    float mipLODBias;
    uint32 maxAnisotropy;
    float minLOD;
    float maxLOD;
    uint32 shaderRegister;
    gfxPipelineVisibilityType visibility;
};

enum class gfxPipelineAccess
{
    NONE = 0x0,
    INPUT_ASSEMBLER = 0x1,
    VERTEX_SHADER = 0x2,
    HULL_SHADER = 0x4,
    DOMAIN_SHADER = 0x8,
    GEOMETRY_SHADER = 0x10,
    PIXEL_SHADER = 0x20
};

// Register mappings of GPU programs.
struct gfxRegisterMapping
{
    inline gfxRegisterMapping( void )
    {
        this->numMappings = 0;
        this->mappings = NULL;
        this->numStaticSamplers = 0;
        this->staticSamplers = NULL;
        this->accessBitfield = gfxPipelineAccess::NONE;
    }

    uint32 numMappings;
    const gfxRootParameter *mappings;
    uint32 numStaticSamplers;
    const gfxStaticSampler *staticSamplers;
    gfxPipelineAccess accessBitfield;
};

struct gfxProgramBytecode
{
    inline gfxProgramBytecode( void )
    {
        this->buf = NULL;
        this->memSize = 0;
    }

    const void *buf;
    size_t memSize;
};

struct gfxInputLayout
{
    inline gfxInputLayout( void )
    {
        this->paramCount = 0;
        this->params = NULL;
    }

    uint32 paramCount;
    const vertexAttrib *params;
};

// Struct that defines a compiled graphics state.
struct gfxGraphicsState
{
    inline gfxGraphicsState( void )
    {
        this->sampleMask = std::numeric_limits <uint32>::max();
        this->topologyType = ePrimitiveTopologyType::TRIANGLE;
        this->numRenderTargets = 0;
        
        for ( size_t n = 0; n < 8; n++ )
        {
            this->renderTargetFormats[ n ] = gfxRasterFormat::UNSPECIFIED;
        }

        this->depthStencilFormat = gfxRasterFormat::UNSPECIFIED;
    }

    // Dynamic programmable states.
    gfxRegisterMapping regMapping;
    gfxProgramBytecode VS;
    gfxProgramBytecode PS;
    gfxProgramBytecode DS;
    gfxProgramBytecode HS;
    gfxProgramBytecode GS;

    // Fixed function states.
    gfxBlendState blendState;
    uint32 sampleMask;
    gfxRasterizerState rasterizerState;
    gfxDepthStencilState depthStencilState;
    gfxInputLayout inputLayout;
    ePrimitiveTopologyType topologyType;
    uint32 numRenderTargets;
    gfxRasterFormat renderTargetFormats[8];
    gfxRasterFormat depthStencilFormat;
};

// Instanced object types.
typedef void* DriverRaster;
typedef void* DriverGeometry;
typedef void* DriverMaterial;
typedef void* DriverGraphicsState;

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

    // Swap chains are used to draw render targets to a window.
    DriverSwapChain* CreateSwapChain( Window *outputWindow, uint32 frameCount );
    void DestroySwapChain( DriverSwapChain *swapChain );

    // Graphics state management.
    DriverGraphicsState* CreateGraphicsState( const gfxGraphicsState& psoState );
    void DestroyGraphicsState( DriverGraphicsState *pso );

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