#include <d3d9.h>

#include "txdread.d3d.genmip.hxx"

namespace rw
{

inline bool getD3DFormatFromRasterType(eRasterFormat paletteRasterType, ePaletteType paletteType, eColorOrdering colorOrder, uint32 itemDepth, D3DFORMAT& d3dFormat)
{
    bool hasFormat = false;

    if ( paletteType != PALETTE_NONE )
    {
        if ( itemDepth == 8 )
        {
            if (colorOrder == COLOR_RGBA)
            {
                d3dFormat = D3DFMT_P8;

                hasFormat = true;
            }
        }
    }
    else
    {
        if ( paletteRasterType == RASTER_1555 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A1R5G5B5;

                    hasFormat = true;
                }
            }
        }
        else if ( paletteRasterType == RASTER_565 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_R5G6B5;

                    hasFormat = true;
                }
            }
        }
        else if ( paletteRasterType == RASTER_4444 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A4R4G4B4;

                    hasFormat = true;
                }
            }
        }
        else if ( paletteRasterType == RASTER_LUM8 )
        {
            if ( itemDepth == 8 )
            {
                d3dFormat = D3DFMT_L8;

                hasFormat = true;
            }
        }
        else if ( paletteRasterType == RASTER_8888 )
        {
            if ( itemDepth == 32 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_A8R8G8B8;

                    hasFormat = true;
                }
                else if (colorOrder == COLOR_RGBA)
                {
                    d3dFormat = D3DFMT_A8B8G8R8;

                    hasFormat = true;
                }
            }
        }
        else if ( paletteRasterType == RASTER_888 )
        {
            if (colorOrder == COLOR_BGRA)
            {
                if ( itemDepth == 32 )
                {
                    d3dFormat = D3DFMT_X8R8G8B8;

                    hasFormat = true;
                }
                else if ( itemDepth == 24 )
                {
                    d3dFormat = D3DFMT_R8G8B8;

                    hasFormat = true;
                }
            }
            else if (colorOrder == COLOR_RGBA)
            {
                if ( itemDepth == 32 )
                {
                    d3dFormat = D3DFMT_X8B8G8R8;

                    hasFormat = true;
                }
            }
        }
        else if ( paletteRasterType == RASTER_555 )
        {
            if ( itemDepth == 16 )
            {
                if (colorOrder == COLOR_BGRA)
                {
                    d3dFormat = D3DFMT_X1R5G5B5;

                    hasFormat = true;
                }
            }
        }
    }

    return hasFormat;
}

struct NativeTextureD3D : public d3dpublic::d3dNativeTextureInterface
{
    enum ePlatformType
    {
        PLATFORM_D3D8,
        PLATFORM_D3D9
    };

    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureD3D( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->platformType = PLATFORM_D3D9;
        this->palette = NULL;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_8888;
        this->depth = 0;
        this->isCubeTexture = false;
        this->autoMipmaps = false;
        this->d3dFormat = D3DFMT_A8R8G8B8;
        this->hasD3DFormat = true;
        this->dxtCompression = 0;
        this->rasterType = 4;
        this->hasAlpha = true;
        this->colorOrdering = COLOR_BGRA;
    }

    inline NativeTextureD3D( const NativeTextureD3D& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        {
	        if (right.palette)
            {
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(right.rasterFormat);

                size_t wholeDataSize = getRasterDataSize( right.paletteSize, palRasterDepth );

		        this->palette = engineInterface->PixelAllocate( wholeDataSize );

		        memcpy(this->palette, right.palette, wholeDataSize);
	        }
            else
            {
		        this->palette = NULL;
	        }

            this->paletteSize = right.paletteSize;
            this->paletteType = right.paletteType;
        }

        // Copy image texel information.
        {
            copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );

            this->rasterFormat = right.rasterFormat;
            this->depth = right.depth;
        }

        this->platformType =        right.platformType;
        this->isCubeTexture =       right.isCubeTexture;
        this->autoMipmaps =         right.autoMipmaps;
        this->d3dFormat =           right.d3dFormat;
        this->hasD3DFormat =        right.hasD3DFormat;
        this->dxtCompression =      right.dxtCompression;
        this->rasterType =          right.rasterType;
        this->hasAlpha =            right.hasAlpha;
        this->colorOrdering =       right.colorOrdering;
    }

    inline void clearTexelData( void )
    {
        if ( this->palette )
        {
	        this->engineInterface->PixelFree( palette );

	        palette = NULL;
        }

        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureD3D( void )
    {
        this->clearTexelData();
    }

    void compress( float quality )
    {
        // Compress it with DXT.
        // If the compression quality is above or equal to 1.0, we want to allow DXT5 compression when alpha is there.
        uint32 dxtType = 1;

        if ( this->hasAlpha )
        {
            if ( quality >= 1.0f )
            {
                dxtType = 5;
            }
            else
            {
                dxtType = 3;
            }
        }

        // This may not be the optimal routine for compressing to DXT.
        // A different method should be used if more accuracy is required.
        //compressDxt( dxtType );
    }

    // Implement the public API.

    bool GetD3DFormat( DWORD& d3dFormat ) const
    {
        bool hasD3DFormat = this->hasD3DFormat;

        if ( hasD3DFormat )
        {
            d3dFormat = (DWORD)this->d3dFormat;
        }

        return hasD3DFormat;
    }

    void SetPlatformType( d3dpublic::ePlatformType platformType )
    {
        ePlatformType nativePlatformType;
        bool hasNativePlatformType = false;

        if ( platformType == d3dpublic::PLATFORM_D3D8 )
        {
            nativePlatformType = PLATFORM_D3D8;

            hasNativePlatformType = true;
        }
        else if ( platformType == d3dpublic::PLATFORM_D3D9 )
        {
            nativePlatformType = PLATFORM_D3D9;

            hasNativePlatformType = true;
        }

        if ( hasNativePlatformType )
        {
            NativeSetPlatformType( nativePlatformType );
        }
    }

    d3dpublic::ePlatformType GetPlatformType( void ) const
    {
        d3dpublic::ePlatformType platformType = d3dpublic::PLATFORM_UNKNOWN;

        ePlatformType nativePlatformType = this->platformType;

        if ( nativePlatformType == PLATFORM_D3D8 )
        {
            platformType = d3dpublic::PLATFORM_D3D8;
        }
        else if ( nativePlatformType == PLATFORM_D3D9 )
        {
            platformType = d3dpublic::PLATFORM_D3D9;
        }

        return platformType;
    }

    // PUBLIC API END

private:
    void NativeSetPlatformType( ePlatformType newPlatform );

public:
    typedef genmip::mipmapLayer mipmapLayer;

    // Platform descriptor.
    ePlatformType platformType;

    eRasterFormat rasterFormat;

    uint32 depth;

	std::vector <mipmapLayer> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

	// PC/XBOX
    bool isCubeTexture;
    bool autoMipmaps;

    D3DFORMAT d3dFormat;
    uint32 dxtCompression;
    uint32 rasterType;

    bool hasD3DFormat;

    inline void updateD3DFormat( void )
    {
        // Execute it whenever the rasterFormat, the palette type, the color order or depth may change.
        D3DFORMAT newD3DFormat;

        bool hasD3DFormat = false;

        uint32 dxtType = this->dxtCompression;

        if ( dxtType != 0 )
        {
            if ( dxtType == 1 )
            {
                newD3DFormat = D3DFMT_DXT1;

                hasD3DFormat = true;
            }
            else if ( dxtType == 2 )
            {
                newD3DFormat = D3DFMT_DXT2;

                hasD3DFormat = true;
            }
            else if ( dxtType == 3 )
            {
                newD3DFormat = D3DFMT_DXT3;

                hasD3DFormat = true;
            }
            else if ( dxtType == 4 )
            {
                newD3DFormat = D3DFMT_DXT4;

                hasD3DFormat = true;
            }
            else if ( dxtType == 5 )
            {
                newD3DFormat = D3DFMT_DXT5;

                hasD3DFormat = true;
            }
        }
        else
        {
            hasD3DFormat = getD3DFormatFromRasterType( this->rasterFormat, this->paletteType, this->colorOrdering, this->depth, newD3DFormat );
        }

        if ( hasD3DFormat )
        {
            this->d3dFormat = newD3DFormat;
        }
        this->hasD3DFormat = hasD3DFormat;
    }

    bool hasAlpha;

    eColorOrdering colorOrdering;
};

struct d3dNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTextureD3D( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTextureD3D( *(const NativeTextureD3D*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTextureD3D*)objMem ).~NativeTextureD3D();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = true;
        capsOut.supportsDXT3 = true;
        capsOut.supportsDXT4 = true;
        capsOut.supportsDXT5 = true;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = true;
        storeCaps.pixelCaps.supportsDXT3 = true;
        storeCaps.pixelCaps.supportsDXT4 = true;
        storeCaps.pixelCaps.supportsDXT5 = true;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureD3D *nativeTex = (const NativeTextureD3D*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void* GetNativeInterface( void *objMem )
    {
        NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

        // The native interface is part of the texture.
        d3dpublic::d3dNativeTextureInterface *nativeAPI = nativeTex;

        return (void*)nativeAPI;
    }

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );

    ePaletteType GetTexturePaletteType( const void *objMem )
    {
        const NativeTextureD3D *nativeTex = (const NativeTextureD3D*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem )
    {
        const NativeTextureD3D *nativeTex = (const NativeTextureD3D*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    bool DoesTextureHaveAlpha( const void *objMem )
    {
        const NativeTextureD3D *nativeTex = (const NativeTextureD3D*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetDriverIdentifier( void *objMem ) const
    {
        // Depends on the platform type of our texture.
        NativeTextureD3D *nativeTex = (NativeTextureD3D*)objMem;

        NativeTextureD3D::ePlatformType platformType = nativeTex->platformType;

        if ( platformType == NativeTextureD3D::PLATFORM_D3D8 )
        {
            // Direct3D 8 driver.
            return 1;
        }
        else if ( platformType == NativeTextureD3D::PLATFORM_D3D9 )
        {
            // Direct3D 9 driver.
            return 2;
        }

        // We do not know which driver.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {

    }

    inline void Shutdown( Interface *engineInterface )
    {

    }
};

inline uint32 getD3DPaletteCount(ePaletteType paletteType)
{
    uint32 reqPalCount = 0;

    if (paletteType == PALETTE_4BIT)
    {
        reqPalCount = 32;
    }
    else if (paletteType == PALETTE_8BIT)
    {
        reqPalCount = 256;
    }
    else
    {
        assert( 0 );
    }

    return reqPalCount;
}

namespace d3d
{

#pragma pack(1)
struct textureMetaHeaderStructGeneric
{
    uint32 platformDescriptor;

    rw::texFormatInfo texFormat;
    
    char name[32];
    char maskName[32];

    uint32 rasterFormat;
};

struct textureMetaHeaderStructDimInfo
{
    uint16 width;               // 0
    uint16 height;              // 2
    uint8 depth;                // 4
    uint8 mipmapCount;          // 5
    uint8 rasterType : 3;       // 6
    uint8 pad1 : 5;
};

struct textureContentInfoStruct
{
    uint8 hasAlpha : 1;
    uint8 isCubeTexture : 1;
    uint8 autoMipMaps : 1;
    uint8 isCompressed : 1;
    uint8 pad : 4;
};
#pragma pack()

};

};