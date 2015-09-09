#include "txdread.d3d.hxx"

#define PLATFORM_D3D9   9

namespace rw
{

inline bool getD3DFormatFromRasterType(eRasterFormat rasterFormat, ePaletteType paletteType, eColorOrdering colorOrder, uint32 itemDepth, D3DFORMAT& d3dFormat)
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
        if ( rasterFormat == RASTER_1555 )
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
        else if ( rasterFormat == RASTER_565 )
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
        else if ( rasterFormat == RASTER_4444 )
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
        else if ( rasterFormat == RASTER_LUM )
        {
            if ( itemDepth == 8 )
            {
                d3dFormat = D3DFMT_L8;

                hasFormat = true;
            }
            // there is also 4bit LUM, but as you see it is not supported by D3D.
        }
        else if ( rasterFormat == RASTER_8888 )
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
        else if ( rasterFormat == RASTER_888 )
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
        else if ( rasterFormat == RASTER_555 )
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
        // NEW formats :3
        else if ( rasterFormat == RASTER_LUM_ALPHA )
        {
            if ( itemDepth == 8 )
            {
                d3dFormat = D3DFMT_A4L4;

                hasFormat = true;
            }
            else if ( itemDepth == 16 )
            {
                d3dFormat = D3DFMT_A8L8;

                hasFormat = true;
            }
        }
    }

    return hasFormat;
}

inline bool isRasterFormatOriginalRWCompatible(
    eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, ePaletteType paletteType
)
{
    // This function assumes that the format that it gets is compatible with the Direct3D 9 native texture.
    // For anything else the result is undefined behavior.

    if ( paletteType != PALETTE_NONE )
    {
        return true;
    }
    else
    {
        if ( rasterFormat == RASTER_1555 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_565 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_4444 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_LUM && depth == 8 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_8888 && depth == 32 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_888 && depth == 32 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_16 && depth == 16 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_24 && depth == 24 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_32 && depth == 32 )
        {
            return true;
        }
        else if ( rasterFormat == RASTER_555 && depth == 16 && colorOrder == COLOR_BGRA )
        {
            return true;
        }
    }

    return false;
}

inline bool getRasterFormatFromD3DFormat(
    D3DFORMAT d3dFormat, bool canHaveAlpha,
    eRasterFormat& rasterFormatOut, eColorOrdering& colorOrderOut, bool& isVirtualRasterFormatOut
)
{
    bool isValidFormat = false;
    bool isVirtualRasterFormat = false;

    if (d3dFormat == D3DFMT_A8R8G8B8)
    {
        rasterFormatOut = RASTER_8888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X8R8G8B8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_R8G8B8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_R5G6B5)
    {
        rasterFormatOut = RASTER_565;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X1R5G5B5)
    {
        rasterFormatOut = RASTER_555;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A1R5G5B5)
    {
        rasterFormatOut = RASTER_1555;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A4R4G4B4)
    {
        rasterFormatOut = RASTER_4444;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A8B8G8R8)
    {
        rasterFormatOut = RASTER_8888;

        colorOrderOut = COLOR_RGBA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_X8B8G8R8)
    {
        rasterFormatOut = RASTER_888;

        colorOrderOut = COLOR_RGBA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_L8)
    {
        rasterFormatOut = RASTER_LUM;

        // Actually, there is no such thing as a color order for luminance textures.
        // We set this field so we make things happy.
        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_A4L4 ||
             d3dFormat == D3DFMT_A8L8)
    {
        rasterFormatOut = RASTER_LUM_ALPHA;

        // See above.
        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT1)
    {
        if ( canHaveAlpha )
        {
            rasterFormatOut = RASTER_1555;
        }
        else
        {
            rasterFormatOut = RASTER_565;
        }

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT2 || d3dFormat == D3DFMT_DXT3)
    {
        rasterFormatOut = RASTER_4444;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_DXT4 || d3dFormat == D3DFMT_DXT5)
    {
        rasterFormatOut = RASTER_4444;

        colorOrderOut = COLOR_BGRA;

        isValidFormat = true;

        isVirtualRasterFormat = true;
    }
    else if (d3dFormat == D3DFMT_P8)
    {
        // We cannot be a palette texture without having actual palette data.
        isValidFormat = false;
    }
    
    if ( isValidFormat )
    {
        isVirtualRasterFormatOut = isVirtualRasterFormat;
    }

    return isValidFormat;
}

inline uint32 getCompressionFromD3DFormat( D3DFORMAT d3dFormat )
{
    uint32 compressionIndex = 0;

    if ( d3dFormat == D3DFMT_DXT1 )
    {
        compressionIndex = 1;
    }
    else if ( d3dFormat == D3DFMT_DXT2 )
    {
        compressionIndex = 2;
    }
    else if ( d3dFormat == D3DFMT_DXT3 )
    {
        compressionIndex = 3;
    }
    else if ( d3dFormat == D3DFMT_DXT4 )
    {
        compressionIndex = 4;
    }
    else if ( d3dFormat == D3DFMT_DXT5 )
    {
        compressionIndex = 5;
    }

    return compressionIndex;
}

struct NativeTextureD3D9 : public d3dpublic::d3dNativeTextureInterface
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureD3D9( Interface *engineInterface )
    {
        // Initialize the texture object.
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->palette = NULL;
        this->paletteSize = 0;
        this->paletteType = PALETTE_NONE;
        this->rasterFormat = RASTER_8888;
        this->depth = 0;
        this->isCubeTexture = false;
        this->autoMipmaps = false;
        this->d3dFormat = D3DFMT_A8R8G8B8;
        this->d3dRasterFormatLink = false;
        this->isOriginalRWCompatible = true;
        this->anonymousFormatLink = NULL;
        this->dxtCompression = 0;
        this->rasterType = 4;
        this->hasAlpha = true;
        this->colorOrdering = COLOR_BGRA;
    }

    inline NativeTextureD3D9( const NativeTextureD3D9& right )
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

        this->isCubeTexture =           right.isCubeTexture;
        this->autoMipmaps =             right.autoMipmaps;
        this->d3dFormat =               right.d3dFormat;
        this->d3dRasterFormatLink =     right.d3dRasterFormatLink;
        this->isOriginalRWCompatible =  right.isOriginalRWCompatible;
        this->anonymousFormatLink =     right.anonymousFormatLink;
        this->dxtCompression =          right.dxtCompression;
        this->rasterType =              right.rasterType;
        this->hasAlpha =                right.hasAlpha;
        this->colorOrdering =           right.colorOrdering;
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

    inline ~NativeTextureD3D9( void )
    {
        this->clearTexelData();
    }

    inline static void getSizeRules( uint32 dxtType, nativeTextureSizeRules& rulesOut )
    {
        bool isCompressed = ( dxtType != 0 );

        rulesOut.powerOfTwo = ( isCompressed == true );
        rulesOut.squared = false;
    }

    // Implement the public API.

    void GetD3DFormat( DWORD& d3dFormat ) const override
    {
        d3dFormat = (DWORD)this->d3dFormat;
    }

    // PUBLIC API END

public:
    typedef genmip::mipmapLayer mipmapLayer;

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

    bool d3dRasterFormatLink;
    bool isOriginalRWCompatible;

    d3dpublic::nativeTextureFormatHandler *anonymousFormatLink;

    inline bool IsRWCompatible( void ) const
    {
        // This function returns whether we can push our data to the RW implementation.
        // We cannot push anything to RW that we have no idea about how it actually looks like.
        return ( this->d3dRasterFormatLink == true || this->dxtCompression != 0 );
    }

    bool hasAlpha;

    eColorOrdering colorOrdering;
};

inline eCompressionType getD3DCompressionType( const NativeTextureD3D9 *nativeTex )
{
    eCompressionType rwCompressionType = RWCOMPRESS_NONE;

    uint32 dxtType = nativeTex->dxtCompression;

    if ( dxtType != 0 )
    {
        if ( dxtType == 1 )
        {
            rwCompressionType = RWCOMPRESS_DXT1;
        }
        else if ( dxtType == 2 )
        {
            rwCompressionType = RWCOMPRESS_DXT2;
        }
        else if ( dxtType == 3 )
        {
            rwCompressionType = RWCOMPRESS_DXT3;
        }
        else if ( dxtType == 4 )
        {
            rwCompressionType = RWCOMPRESS_DXT4;
        }
        else if ( dxtType == 5 )
        {
            rwCompressionType = RWCOMPRESS_DXT5;
        }
        else
        {
            throw RwException( "unsupported DXT compression" );
        }
    }

    return rwCompressionType;
}

struct d3d9NativeTextureTypeProvider : public texNativeTypeProvider, d3dpublic::d3dNativeTextureDriverInterface
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D9( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureD3D9( *(const NativeTextureD3D9*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureD3D9*)objMem ).~NativeTextureD3D9();
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
        NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void* GetNativeInterface( void *objMem ) override
    {
        NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

        // The native interface is part of the texture.
        d3dpublic::d3dNativeTextureInterface *nativeAPI = nativeTex;

        return (void*)nativeAPI;
    }

    void* GetDriverNativeInterface( void ) const override
    {
        d3dpublic::d3dNativeTextureDriverInterface *nativeDriver = (d3dpublic::d3dNativeTextureDriverInterface*)this;

        // We do export a public driver API.
        // It is the most direct way to address the D3D9 native texture environment.
        return nativeDriver;
    }

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->rasterFormat;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->paletteType;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return ( nativeTex->dxtCompression != 0 );
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return getD3DCompressionType( nativeTex );
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Direct3D 8 and 9 work with DWORD aligned texture data rows.
        // We found this out when looking at the return values of GetLevelDesc.
        // By the way, the way RenderWare reads texture data into memory is optimized and flawed.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb206357%28v=vs.85%29.aspx
        // According to the docs, the alignment may always change, the driver has liberty to do anything.
        // We just conform to what Rockstar and Criterion have thought about here, anyway.
        // I believe that ATI and nVidia have recogized this issue and made sure the alignment is constant!
        return getD3DTextureDataRowAlignment();
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The rules here are very similar to Direct3D 8.
        // When we add support for cubemaps, we will have to respect them aswell...
        // But that is an issue for another day!
        const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

        NativeTextureD3D9::getSizeRules( nativeTex->dxtCompression, rulesOut );
    }

    // We want to support output as DirectDraw Surface files, as they are pretty handy.
    const char* GetNativeImageFormatExtension( void ) const override
    {
        return "DDS";
    }

    bool IsNativeImageFormat( Interface *engineInterface, Stream *inputStream ) const override;
    void SerializeNativeImage( Interface *engineInterface, Stream *inputStream, void *objMem ) const override;
    void DeserializeNativeImage( Interface *engineInterface, Stream *outputStream, void *objMem ) const override;

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // We are the Direct3D 9 driver.
        return 2;
    }

    // PUBLIC API begin

    d3dpublic::nativeTextureFormatHandler* GetFormatHandler( D3DFORMAT format ) const
    {
        for ( nativeFormatExtensions_t::const_iterator iter = this->formatExtensions.cbegin(); iter != this->formatExtensions.cend(); iter++ )
        {
            const nativeFormatExtension& ext = *iter;

            if ( ext.theFormat == format )
            {
                return ext.handler;
            }
        }

        return NULL;
    }

    bool RegisterFormatHandler( DWORD format, d3dpublic::nativeTextureFormatHandler *handler );
    bool UnregisterFormatHandler( DWORD format );

    struct nativeFormatExtension
    {
        D3DFORMAT theFormat;
        d3dpublic::nativeTextureFormatHandler *handler;
    };

    typedef std::list <nativeFormatExtension> nativeFormatExtensions_t;

    nativeFormatExtensions_t formatExtensions;

    // PUBLIC API end

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "Direct3D9", this, sizeof( NativeTextureD3D9 ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "Direct3D9" );
    }
};

namespace d3d9
{

#pragma pack(1)
struct textureMetaHeaderStructGeneric
{
    endian::little_endian <uint32> platformDescriptor;

    rw::texFormatInfo_serialized <rw::endian::little_endian <uint32>> texFormat;
    
    char name[32];
    char maskName[32];

    endian::little_endian <uint32> rasterFormat;
    endian::little_endian <D3DFORMAT> d3dFormat;

    endian::little_endian <uint16> width;
    endian::little_endian <uint16> height;
    uint8 depth;
    uint8 mipmapCount;
    uint8 rasterType : 3;
    uint8 pad1 : 5;

    uint8 hasAlpha : 1;
    uint8 isCubeTexture : 1;
    uint8 autoMipMaps : 1;
    uint8 isNotRwCompatible : 1;
    uint8 pad2 : 4;
};
#pragma pack()

};

}