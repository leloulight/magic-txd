#include "txdread.d3d.hxx"

#define PLATFORM_D3D8   8

namespace rw
{

struct NativeTextureD3D8
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureD3D8( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = NULL;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_8888;
        this->depth = 0;
        this->autoMipmaps = false;
        this->dxtCompression = 0;
        this->rasterType = 4;
        this->hasAlpha = true;
        this->colorOrdering = COLOR_BGRA;
    }

    inline NativeTextureD3D8( const NativeTextureD3D8& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        {
	        if (right.palette)
            {
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(right.rasterFormat);

                size_t wholeDataSize = getPaletteDataSize( right.paletteSize, palRasterDepth );

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

        this->autoMipmaps =         right.autoMipmaps;
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

    inline ~NativeTextureD3D8( void )
    {
        this->clearTexelData();
    }

public:
    typedef genmip::mipmapLayer mipmapLayer;

    eRasterFormat rasterFormat;

    uint32 depth;

	std::vector <mipmapLayer> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

	// PC/XBOX
    bool autoMipmaps;

    uint32 dxtCompression;
    uint32 rasterType;

    bool hasAlpha;

    eColorOrdering colorOrdering;
};

struct d3d8NativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D8( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D8( *(const NativeTextureD3D8*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureD3D8*)objMem ).~NativeTextureD3D8();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = true;
        capsOut.supportsDXT3 = true;
        capsOut.supportsDXT4 = true;
        capsOut.supportsDXT5 = true;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const override
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

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTextureD3D8 *nativeTex = (NativeTextureD3D8*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void* GetNativeInterface( void *objMem ) override
    {
        // TODO.
        return NULL;
    }

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureD3D8 *nativeTex = (const NativeTextureD3D8*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Direct3D 8 and 9 work with DWORD aligned texture data rows.
        // We found this out when looking at the return values of GetLevelDesc.
        return getD3DTextureDataRowAlignment();
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Direct3D 8 driver.
        return 1;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Direct3D8", this, sizeof( NativeTextureD3D8 ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Direct3D8" );
    }
};

namespace d3d8
{

#pragma pack(1)
struct textureMetaHeaderStructGeneric
{
    endian::little_endian <uint32> platformDescriptor;

    rw::texFormatInfo_serialized <rw::endian::little_endian <uint32>> texFormat;
    
    char name[32];
    char maskName[32];

    endian::little_endian <uint32> rasterFormat;
    endian::little_endian <uint32> hasAlpha;

    endian::little_endian <uint16> width;
    endian::little_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType : 3;
    uint8 pad1 : 5;
    uint8 dxtCompression;
};
#pragma pack()

};

}