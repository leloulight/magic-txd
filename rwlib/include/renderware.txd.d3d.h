// Public Texture Dictionary Direct3D RenderWare extensions.
// Use those if you need internal access to D3D native textures.
// Be cautious, as internals do rapidly change between RenderWare versions.

namespace d3dpublic
{

inline uint32 GetAPIRevision( void )
{
    // First revision API.
    return 1;
}

// Currently, the Direct3D native texture can be interchanged between D3D 8 and D3D 9 internally.
// This is a pretty optimized implementation detail that can be used to your advantage.
enum ePlatformType
{
    PLATFORM_UNKNOWN,
    PLATFORM_D3D8,
    PLATFORM_D3D9
};

struct d3dNativeTextureInterface abstract
{
    virtual bool GetD3DFormat( DWORD& d3dFormat ) const = 0;

    virtual void SetPlatformType( ePlatformType platformType ) = 0;
    virtual ePlatformType GetPlatformType( void ) const = 0;
};

};