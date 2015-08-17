#include "StdInc.h"

#include "rwdriver.hxx"

#include "rwdrawing.hxx"

namespace rw
{

// We need a render state manager for 2D drawing context states.
struct RenderStateManager_Layer2D
{
    typedef deviceStateValue valueType;

    struct stateAddress
    {
        DrawingLayer2D::eRwDeviceCmd type;

        AINLINE stateAddress( void )
        {
            type = DrawingLayer2D::RWSTATE_FIRST;
        }

        AINLINE void Increment( void )
        {
            if ( !IsEnd() )
            {
                type = (DrawingLayer2D::eRwDeviceCmd)( type + 1 );
            }
        }

        AINLINE bool IsEnd( void ) const
        {
            return ( type == DrawingLayer2D::RWSTATE_MAX );
        }

        AINLINE unsigned int GetArrayIndex( void ) const
        {
            return (unsigned int)type;
        }

        AINLINE bool operator == ( const stateAddress& right ) const
        {
            return ( this->type == right.type );
        }
    };

    AINLINE RenderStateManager_Layer2D( void ) : invalidState( -1 )
    {
        return;
    }

    AINLINE ~RenderStateManager_Layer2D( void )
    {
        return;
    }

    AINLINE bool GetDeviceState( const stateAddress& address, valueType& value )
    {
        // TODO.
        return false;
    }

    AINLINE void SetDeviceState( const stateAddress& address, const valueType& value )
    {
        // TODO.
    }

    AINLINE void ResetDeviceState( const stateAddress& address )
    {
        // Nothing to do for render states.
        return;
    }

    AINLINE bool CompareDeviceStates( const valueType& left, const valueType& right ) const
    {
        return ( left == right );
    }

    template <typename referenceDeviceType>
    AINLINE bool IsDeviceStateRequired( const stateAddress& address, referenceDeviceType& refDevice )
    {
        return true;
    }

    const valueType invalidState;

    AINLINE const valueType& GetInvalidDeviceState( void )
    {
        return invalidState;
    }
};

typedef RwStateManager <RenderStateManager_Layer2D> RwRenderStateManager_Layer2D;

// Cached primitive buffer that is extendable.
struct cached_primitive_buffer
{
    ePrimitiveTopology topology;

    DriverImmediatePushbuffer *vertexBuf;
};

struct vertex_rect_2d
{
    
};

struct DrawingLayer2DImpl : public DrawingLayer2D
{
    inline DrawingLayer2DImpl( Driver *theDriver ) : rsMan( (EngineInterface*)theDriver->GetEngineInterface() )
    {
        this->theDriver = theDriver;
        this->isDrawing = false;

        this->rsMan.Initialize();
    }

    inline ~DrawingLayer2DImpl( void )
    {
        this->rsMan.Invalidate();
    }

    Driver *theDriver;

    RwRenderStateManager_Layer2D rsMan;

    bool isDrawing;
};

// Implement the Drawing Layer 2D interface.
bool DrawingLayer2D::SetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t value )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    RenderStateManager_Layer2D::stateAddress addr;
    addr.type = cmd;

    layerImpl->rsMan.SetDeviceState( addr, value );

    return true;
}

bool DrawingLayer2D::GetRenderState( eRwDeviceCmd cmd, rwDeviceValue_t& value ) const
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    RenderStateManager_Layer2D::stateAddress addr;
    addr.type = cmd;

    value = layerImpl->rsMan.GetDeviceState( addr );

    return true;
}

bool DrawingLayer2D::SetRaster( Raster *theRaster )
{
    return false;
}

Raster* DrawingLayer2D::GetRaster( void ) const
{
    return NULL;
}

void DrawingLayer2D::Begin( void )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    assert( layerImpl->isDrawing == false );

    // Commit any device states.
    layerImpl->rsMan.ApplyDeviceStates();

    // Prepare the drawing process.
    layerImpl->rsMan.BeginBucketPass();

    layerImpl->isDrawing = true;
}

void DrawingLayer2D::End( void )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    assert( layerImpl->isDrawing == true );

    // Finish the drawing process.
    layerImpl->rsMan.EndBucketPass();

    layerImpl->isDrawing = false;
}

void DrawingLayer2D::DrawRect( uint32 x, uint32 y, uint32 width, uint32 height )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

}

void DrawingLayer2D::DrawLine( uint32 x, uint32 y, uint32 end_x, uint32 end_y )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

}

void DrawingLayer2D::DrawPoint( uint32 x, uint32 y )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

}

// Creation API.
DrawingLayer2D* CreateDrawingLayer2D( Driver *theDriver )
{
    DrawingLayer2DImpl *layerImpl = new DrawingLayer2DImpl( theDriver );

    return layerImpl;
}

void DeleteDrawingLayer2D( DrawingLayer2D *layer )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)layer;

    delete layerImpl;
}

// Environment registration.
void registerDrawingLayerEnvironment( void )
{
    // TODO.
}

};