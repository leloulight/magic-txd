// The Gamecube native texture is stored in big-endian format.
#define PLATFORMDESC_GAMECUBE   6

#include "txdread.d3d.genmip.hxx"

namespace rw
{

enum eGCNativeTextureFormat : unsigned char
{
    GVRFMT_LUM_4BIT,
    GVRFMT_LUM_8BIT,
    GVRFMT_LUM_4BIT_ALPHA,
    GVRFMT_LUM_8BIT_ALPHA,
    GVRFMT_RGB565,
    GVRFMT_RGB5A3,
    GVRFMT_RGBA8888,
    GVRFMT_PAL_4BIT = 0x8,
    GVRFMT_PAL_8BIT,
    GVRFMT_CMP = 0xE
};

enum eGCPixelFormat : unsigned char
{
    GVRPIX_LUM_ALPHA,
    GVRPIX_RGB565,
    GVRPIX_RGB5A3
};

inline uint32 getGVRPixelFormatDepth( eGCPixelFormat format )
{
    uint32 depth = 0;

    if ( format == GVRPIX_LUM_ALPHA )
    {
        depth = 16;
    }
    else if ( format == GVRPIX_RGB565 )
    {
        depth = 16;
    }
    else if ( format == GVRPIX_RGB5A3 )
    {
        depth = 16;
    }

    return depth;
}

struct NativeTextureGC
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureGC( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = NULL;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->internalFormat = GVRFMT_RGBA8888;
        this->palettePixelFormat = GVRPIX_LUM_ALPHA;
        this->autoMipmaps = false;
        this->rasterType = 4;
        this->hasAlpha = false;
    }

    inline NativeTextureGC( const NativeTextureGC& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;

        // Copy palette information.
        {
	        if (right.palette)
            {
                uint32 palRasterDepth = getGVRPixelFormatDepth(this->palettePixelFormat);

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
            this->palettePixelFormat = right.palettePixelFormat;
        }

        // Copy image texel information.
        {
            copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );
        }

        // Copy advanced properties.
        this->internalFormat = right.internalFormat;
        this->autoMipmaps = right.autoMipmaps;
        this->rasterType = right.rasterType;
        this->hasAlpha = right.hasAlpha;
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

    inline ~NativeTextureGC( void )
    {
        this->clearTexelData();
    }

public:
    typedef genmip::mipmapLayer mipmapLayer;

	std::vector <mipmapLayer> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

    eGCNativeTextureFormat internalFormat;
    eGCPixelFormat palettePixelFormat;

    bool autoMipmaps;
    uint8 rasterType;

    bool hasAlpha;
};

struct gamecubeNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureGC( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureGC( *(const NativeTextureGC*)srcObjMem );
    }

    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ((NativeTextureGC*)objMem)->~NativeTextureGC();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = false;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = false;
        storeCaps.pixelCaps.supportsPalette = true;

        storeCaps.isCompressedFormat = false;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

        nativeTex->texVersion = version;

        // TODO: handle different RW versions.
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;
        
        return ( nativeTex->internalFormat == GVRFMT_CMP );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // I guess we want to be 4 byte aligned.
        return 4;
    }

    void* GetNativeInterface( void *objMem ) override
    {
        // TODO.
        return NULL;
    }

    void* GetDriverNativeInterface( void ) const override
    {
        // TODO.
        return NULL;
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Undefined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Gamecube", this, sizeof( NativeTextureGC ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Gamecube" );
    }
};

namespace gamecube
{

#pragma pack(1)
struct textureMetaHeaderStructGeneric
{
    endian::big_endian <uint32> platformDescriptor;

    rw::texFormatInfo_serialized <endian::big_endian <uint32>> texFormat;

    endian::big_endian <uint32> unk1;   // gotta be some sort of gamecube registers.
    endian::big_endian <uint32> unk2;
    endian::big_endian <uint32> unk3;
    endian::big_endian <uint32> unk4;
    
    char name[32];
    char maskName[32];

    endian::big_endian <uint32> rasterFormat;

    endian::big_endian <uint16> width;
    endian::big_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    eGCNativeTextureFormat internalFormat;
    uint8 unk5;

    endian::big_endian <uint32> hasAlpha;
    endian::big_endian <uint32> imageDataSectionSize;
};
#pragma pack()

};

};