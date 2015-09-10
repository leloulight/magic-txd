#include "txdread.nativetex.hxx"

#include "txdread.d3d.genmip.hxx"

#define NATIVE_TEXTURE_XBOX     5

namespace rw
{

inline uint32 getXBOXTextureDataRowAlignment( void )
{
    return sizeof( DWORD );
}

struct NativeTextureXBOX
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureXBOX( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = NULL;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->dxtCompression = 0;
        this->hasAlpha = false;
        this->colorOrder = COLOR_BGRA;
        this->rasterType = 4;   // by default it is a texture raster.
        this->autoMipmaps = false;
    }

    inline NativeTextureXBOX( const NativeTextureXBOX& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = right.engineInterface;
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

            // Copy over attributes.
            this->rasterFormat = right.rasterFormat;
            this->depth = right.depth;
        }

        this->dxtCompression = right.dxtCompression;
        this->hasAlpha = right.hasAlpha;

        this->colorOrder = right.colorOrder;
        this->rasterType = right.rasterType;
        this->autoMipmaps = right.autoMipmaps;
    }

    inline void clearTexelData( void )
    {
        if ( this->palette )
        {
	        this->engineInterface->PixelFree( palette );

	        palette = NULL;
        }

	    deleteMipmapLayers( this->engineInterface, this->mipmaps );

        this->mipmaps.clear();
    }

    inline ~NativeTextureXBOX( void )
    {
        this->clearTexelData();
    }

    eRasterFormat rasterFormat;

    uint32 depth;

    typedef genmip::mipmapLayer mipmapLayer;

    std::vector <mipmapLayer> mipmaps;

	void *palette;
	uint32 paletteSize;

    ePaletteType paletteType;

    uint8 rasterType;

    eColorOrdering colorOrder;

    bool hasAlpha;

    bool autoMipmaps;

	// PC/XBOX
	uint32 dxtCompression;

    struct swizzleMipmapTraversal
    {
        // Input.
        uint32 mipWidth, mipHeight;
        uint32 depth;
        uint32 rowAlignment;
        void *texels;
        uint32 dataSize;

        // Output.
        uint32 newWidth, newHeight;
        void *newtexels;
        uint32 newDataSize;
    };

    static void swizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData );
    static void unswizzleMipmap( Interface *engineInterface, swizzleMipmapTraversal& pixelData );
};

inline eCompressionType getDXTCompressionTypeFromXBOX( uint32 xboxCompressionType )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    if ( xboxCompressionType != 0 )
    {
        if ( xboxCompressionType == 0xc )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( xboxCompressionType == 0xd )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( xboxCompressionType == 0xe )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( xboxCompressionType == 0xf )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else if ( xboxCompressionType == 0x10 )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else
        {
            assert( 0 );
        }
    }

    return rwCompressionType;
}

inline void getXBOXNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;
}

struct xboxNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTextureXBOX( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTextureXBOX( *(const NativeTextureXBOX*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTextureXBOX*)objMem ).~NativeTextureXBOX();
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
        NativeTextureXBOX *nativeTex = (NativeTextureXBOX*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return getDXTCompressionTypeFromXBOX( nativeTex->dxtCompression );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureXBOX *nativeTex = (const NativeTextureXBOX*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // The XBOX is assumed to have the same row alignment like Direct3D 8.
        // This means it is aligned to DWORD.
        return getXBOXTextureDataRowAlignment();
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getXBOXNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The XBOX native texture is a very old format, derived from very limited hardware.
        // So we restrict it very firmly, in comparison to Direct3D 8 and 9.
        getXBOXNativeTextureSizeRules( rulesOut );
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Always the generic XBOX driver.
        return 8;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "XBOX", this, sizeof( NativeTextureXBOX ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "XBOX" );
    }
};

#pragma pack(1)
struct textureMetaHeaderStructXbox
{
    rw::texFormatInfo_serialized <rw::endian::little_endian <uint32>> formatInfo;

    char name[32];
    char maskName[32];

    endian::little_endian <uint32> rasterFormat;
    endian::little_endian <uint32> hasAlpha;
    endian::little_endian <uint16> width, height;

    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType;
    uint8 dxtCompression;

    endian::little_endian <uint32> imageDataSectionSize;
};
#pragma pack()

};