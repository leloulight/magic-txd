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

#pragma pack(1)
struct vertex_2d
{
    RwV2d pos;
    RwV2d tex_coord;
    RwV4d color;
};
#pragma pack()

struct DrawingLayer2DImpl : public DrawingLayer2D
{
    // Cached primitive buffer that is extendable.
    struct cached_primitive_buffer
    {
        inline cached_primitive_buffer( void )
        {
            this->topology = ePrimitiveTopology::POINTLIST;
            this->rsContext = NULL;
            this->psoObj = NULL;
            this->vertexBuf = NULL;
        }

        inline ~cached_primitive_buffer( void )
        {
            assert( this->rsContext == NULL );
            assert( this->psoObj == NULL );
            assert( this->vertexBuf == NULL );
        }

        inline void Initialize( DrawingLayer2DImpl *layer )
        {
            LIST_INSERT( layer->active_draw_buffers.root, this->node );

            this->vertexBuf = AllocatePushbuffer( (EngineInterface*)layer->theDriver->GetEngineInterface() );
        }

        inline void Shutdown( DrawingLayer2DImpl *layer )
        {
            // Clean up objects.
            if ( this->rsContext )
            {
                layer->rsMan.FreeState( this->rsContext );

                this->rsContext = NULL;
            }

            if ( this->psoObj )
            {
                layer->theDriver->DestroyGraphicsState( this->psoObj );

                this->psoObj = NULL;
            }

            if ( this->vertexBuf )
            {
                FreePushbuffer( this->vertexBuf );

                this->vertexBuf = NULL;
            }

            LIST_REMOVE( this->node );
        }

        inline bool IsCurrent( DrawingLayer2DImpl *layer ) const
        {
            if ( this->topology != layer->topology )
                return false;

            if ( this->rsContext == NULL || this->rsContext->IsCurrent() == false )
                return false;

            return true;
        }

        ePrimitiveTopology topology;
    
        // States that we render the primitives in that reside in this buffer.
        // We need to know which primitives can be added to this buffer, and comparing
        // using these objects is very fast.
        RwRenderStateManager_Layer2D::capturedState *rsContext;

        DriverGraphicsState *psoObj;    // the meaning of this buffer, in internal speak.

        DriverImmediatePushbuffer *vertexBuf;

        RwListEntry <cached_primitive_buffer> node;
    };

    inline DrawingLayer2DImpl( Driver *theDriver ) : rsMan( (EngineInterface*)theDriver->GetEngineInterface() )
    {
        this->theDriver = theDriver;
        this->isDrawing = false;

        this->rsMan.Initialize();

        LIST_CLEAR( this->active_draw_buffers.root );

        this->draw_buffer = NULL;
    }

    inline ~DrawingLayer2DImpl( void )
    {
        this->draw_buffer = NULL;

        // Clear the list of draw buffers.
        while ( !LIST_EMPTY( this->active_draw_buffers.root ) )
        {
            cached_primitive_buffer *layer = LIST_GETITEM( cached_primitive_buffer, this->active_draw_buffers.root.next, node );

            layer->Shutdown( this );

            delete layer;
        }

        this->rsMan.Invalidate();
    }

    inline cached_primitive_buffer* FindContext( void )
    {
        LIST_FOREACH_BEGIN( cached_primitive_buffer, this->active_draw_buffers.root, node )

            if ( item->IsCurrent( this ) )
                return item;

        LIST_FOREACH_END

        return NULL;
    }

    inline void EstablishDrawingContext( void )
    {
        // Check whether we can continue the current drawing context.
        // If we can, then this operation is a NO-OP.
        bool is_bucket_current = false;
        {
            if ( this->draw_buffer != NULL )
            {
                is_bucket_current = ( draw_buffer->IsCurrent( this ) );
            }
        }

        if ( is_bucket_current )
            return;

        // Flush any drawing commands that have queued up by now.
        {
            // TODO.
        }

        // Try to find a bucket that we have already allocated and contains the states that
        // we would need now. We do this because an application likely reuses states a lot.
        cached_primitive_buffer *fitting_draw_buffer = FindContext();

        if ( !fitting_draw_buffer )
        {
            // Create new draw context.
            cached_primitive_buffer *new_buf = new cached_primitive_buffer();

            try
            {
                new_buf->Initialize( this );

                try
                {
                    new_buf->topology = this->topology;
            
                    if ( new_buf->rsContext == NULL )
                    {
                        new_buf->rsContext = this->rsMan.CaptureState();

                        if ( new_buf->rsContext == NULL )
                        {
                            throw RwException( "failed to allocate captured render states for 2D drawing layer" );
                        }
                    }
                    else
                    {
                        new_buf->rsContext->Capture();
                    }

                    // Create the graphics PSO.
                    gfxGraphicsState psoState;
            
                    new_buf->psoObj = this->theDriver->CreateGraphicsState( psoState );

                    if ( new_buf->psoObj == NULL )
                    {
                        throw RwException( "failed to allocate graphics state for 2D drawing layer" );
                    }
                }
                catch( ... )
                {
                    new_buf->Shutdown( this );

                    throw;
                }
            }
            catch( ... )
            {
                delete new_buf;

                throw;
            }
        }

        // Set it as current.
        this->draw_buffer = fitting_draw_buffer;
    }

    Driver *theDriver;

    // Current uncommitted states.
    ePrimitiveTopology topology;
    RwRenderStateManager_Layer2D rsMan;

    bool isDrawing;

    cached_primitive_buffer *draw_buffer;

    RwList <cached_primitive_buffer> active_draw_buffers;
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

    layerImpl->topology = ePrimitiveTopology::TRIANGLELIST;

    layerImpl->EstablishDrawingContext();
}

void DrawingLayer2D::DrawLine( uint32 x, uint32 y, uint32 end_x, uint32 end_y )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    layerImpl->topology = ePrimitiveTopology::LINELIST;

    layerImpl->EstablishDrawingContext();
}

void DrawingLayer2D::DrawPoint( uint32 x, uint32 y )
{
    DrawingLayer2DImpl *layerImpl = (DrawingLayer2DImpl*)this;

    layerImpl->topology = ePrimitiveTopology::POINTLIST;

    layerImpl->EstablishDrawingContext();
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