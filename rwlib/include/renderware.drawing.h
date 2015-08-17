/*
    RenderWare drawing system definitions.
    Those are used to issue drawing commands to the environment.
    Drawing environments execute under a specific context that keeps track of certain states.
    There is a separate 2D context and a separate 3D context.

    The RenderStates are closely 
*/

#ifndef _RENDERWARE_DRAWING_DEFINES_
#define _RENDERWARE_DRAWING_DEFINES_

// RenderState enums.
// The implementation has to map these to internal device states.
enum eRwDeviceCmd : uint32
{
    RWSTATE_COMBINEDTEXADDRESSMODE,
    RWSTATE_UTEXADDRESSMODE,
    RWSTATE_VTEXADDRESSMODE,
    RWSTATE_WTEXADDRESSMODE,
    RWSTATE_ZTESTENABLE,
    RWSTATE_ZWRITEENABLE,
    RWSTATE_TEXFILTER,
    RWSTATE_SRCBLEND,
    RWSTATE_DSTBLEND,
    RWSTATE_ALPHABLENDENABLE,
    RWSTATE_CULLMODE,
    RWSTATE_STENCILENABLE,
    RWSTATE_STENCILFAIL,
    RWSTATE_STENCILZFAIL,
    RWSTATE_STENCILPASS,
    RWSTATE_STENCILFUNC,
    RWSTATE_STENCILREF,
    RWSTATE_STENCILMASK,
    RWSTATE_STENCILWRITEMASK,
    RWSTATE_ALPHAFUNC,
    RWSTATE_ALPHAREF
};

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

// Definition of a 2D drawing layer.
struct DrawingLayer2D
{
    // Basic render state setting function.
    enum eRwDeviceCmd : uint32
    {
        RWSTATE_FIRST,

        RWSTATE_UTEXADDRESSMODE,
        RWSTATE_VTEXADDRESSMODE,
        RWSTATE_ZWRITEENABLE,
        RWSTATE_TEXFILTER,
        RWSTATE_SRCBLEND,
        RWSTATE_DSTBLEND,
        RWSTATE_ALPHABLENDENABLE,
        RWSTATE_ALPHAFUNC,
        RWSTATE_ALPHAREF,

        RWSTATE_MAX
    };

    bool SetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t value );
    bool GetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t& value ) const;

    // Advanced resource functions.
    bool SetRaster( Raster *theRaster );
    Raster* GetRaster( void ) const;

    // Context acquisition.
    void Begin();
    void End();

    // Drawing commands.
    void DrawRect( uint32 x, uint32 y, uint32 width, uint32 height );
    void DrawLine( uint32 x, uint32 y, uint32 end_x, uint32 end_y );
    void DrawPoint( uint32 x, uint32 y );
};

// Drawing context creation API.
DrawingLayer2D* CreateDrawingLayer2D( Driver *usedDriver );
void DeleteDrawingLayer2D( DrawingLayer2D *layer );

#endif //_RENDERWARE_DRAWING_DEFINES_