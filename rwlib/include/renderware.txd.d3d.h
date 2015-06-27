// Public Texture Dictionary Direct3D RenderWare extensions.
// Use those if you need internal access to D3D native textures.
// Be cautious, as internals do rapidly change between RenderWare versions.

namespace d3dpublic
{

inline uint32 GetAPIRevision( void )
{
    // Second revision API.
    // Other than in the first revision, we have split Direct3D 8 and 9.
    return 2;
}

struct d3dNativeTextureInterface abstract
{
    virtual bool GetD3DFormat( DWORD& d3dFormat ) const = 0;
};

};