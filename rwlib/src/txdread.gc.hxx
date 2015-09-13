#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.nativetex.hxx"

// The Gamecube native texture is stored in big-endian format.
#define PLATFORMDESC_GAMECUBE   6

#include "txdread.d3d.genmip.hxx"

#include "pixelformat.hxx"

namespace rw
{

inline uint32 getGCTextureDataRowAlignment( void )
{
    // I guess it is aligned by 4? I am not sure yet.
    return 4;
}

inline uint32 getGCRasterDataRowSize( uint32 planeWidth, uint32 depth )
{
    return getRasterDataRowSize( planeWidth, depth, getGCTextureDataRowAlignment() );
}

inline uint32 getGCPaletteSize( ePaletteType paletteType )
{
    uint32 paletteSize = 0;

    if ( paletteType == PALETTE_4BIT )
    {
        paletteSize = 16;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        paletteSize = 256;
    }

    return paletteSize;
}

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

enum eGCPixelFormat : char
{
    GVRPIX_NO_PALETTE = -1,
    GVRPIX_LUM_ALPHA,
    GVRPIX_RGB565,
    GVRPIX_RGB5A3
};

inline bool getGCNativeTextureRasterFormat(
    eGCNativeTextureFormat format, eGCPixelFormat paletteFormat,
    eRasterFormat& rasterFormatOut, uint32& depthOut, eColorOrdering& colorOrderOut,
    ePaletteType& paletteTypeOut
)
{
    bool hasFormat = true;

    ePaletteType paletteType = PALETTE_NONE;

    if ( format == GVRFMT_LUM_4BIT )
    {
        rasterFormatOut = RASTER_LUM;
        depthOut = 4;
        colorOrderOut = COLOR_RGBA;
    }
    else if ( format == GVRFMT_LUM_8BIT )
    {
        rasterFormatOut = RASTER_LUM;
        depthOut = 8;
        colorOrderOut = COLOR_RGBA;
    }
    else if ( format == GVRFMT_LUM_4BIT_ALPHA )
    {
        rasterFormatOut = RASTER_LUM_ALPHA;
        depthOut = 8;
        colorOrderOut = COLOR_RGBA;
    }
    else if ( format == GVRFMT_LUM_8BIT_ALPHA )
    {
        rasterFormatOut = RASTER_LUM_ALPHA;
        depthOut = 16;
        colorOrderOut = COLOR_RGBA;
    }
    else if ( format == GVRFMT_RGB565 )
    {
        rasterFormatOut = RASTER_565;
        depthOut = 16;
        colorOrderOut = COLOR_RGBA;
    }
    else if ( format == GVRFMT_RGB5A3 )
    {
        // This format will never have a RW representation, because it is too specific to GC hardware.
        hasFormat = false;
    }
    else if ( format == GVRFMT_PAL_4BIT ||
              format == GVRFMT_PAL_8BIT )
    {
        bool hasPaletteFormat = false;

        if ( paletteFormat == GVRPIX_LUM_ALPHA )
        {
            rasterFormatOut = RASTER_LUM_ALPHA;
            colorOrderOut = COLOR_RGBA;

            hasPaletteFormat = true;
        }
        else if ( paletteFormat == GVRPIX_RGB565 )
        {
            rasterFormatOut = RASTER_565;
            colorOrderOut = COLOR_RGBA;

            hasPaletteFormat = true;
        }

        if ( hasPaletteFormat )
        {
            if ( format == GVRFMT_PAL_4BIT )
            {
                paletteType = PALETTE_4BIT;
                depthOut = 4;
            }
            else if ( format == GVRFMT_PAL_8BIT )
            {
                paletteType = PALETTE_8BIT;
                depthOut = 8;
            }
        }

        hasFormat = hasPaletteFormat;
    }
    else
    {
        // Should never happen.
        hasFormat = false;
    }

    if ( hasFormat )
    {
        paletteTypeOut = paletteType;
    }

    return hasFormat;
}

inline uint32 getGCInternalFormatDepth( eGCNativeTextureFormat format )
{
    uint32 depth = 0;

    if ( format == GVRFMT_LUM_4BIT ||
         format == GVRFMT_PAL_4BIT )
    {
        depth = 4;
    }
    else if ( format == GVRFMT_LUM_4BIT_ALPHA ||
              format == GVRFMT_LUM_8BIT ||
              format == GVRFMT_PAL_8BIT )
    {
        depth = 8;
    }
    else if ( format == GVRFMT_RGB565 ||
              format == GVRFMT_RGB5A3 )
    {
        depth = 16;
    }
    else if ( format == GVRFMT_RGBA8888 )
    {
        depth = 32;
    }

    return depth;
}

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

struct gcColorDispatch
{
private:
    eGCNativeTextureFormat internalFormat;
    eGCPixelFormat palettePixelFormat;
    uint32 itemDepth;
    ePaletteType paletteType;
    const void *paletteData;
    uint32 maxpalette;

    eColorModel usedColorModel;

public:
    AINLINE gcColorDispatch(
        eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette
    )
    {
        this->internalFormat = internalFormat;
        this->palettePixelFormat = palettePixelFormat;
        this->itemDepth = itemDepth;
        this->paletteType = paletteType;
        this->paletteData = paletteData;
        this->maxpalette = maxpalette;

        // Decide about color model.
        if ( internalFormat == GVRFMT_PAL_4BIT ||
             internalFormat == GVRFMT_PAL_8BIT )
        {
            if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
            {
                this->usedColorModel = COLORMODEL_LUMINANCE;
            }
            else if ( palettePixelFormat == GVRPIX_RGB565 ||
                      palettePixelFormat == GVRPIX_RGB5A3 )
            {
                this->usedColorModel = COLORMODEL_RGBA;
            }
            else
            {
                throw RwException( "failed to map Gamecube palette format to color model" );
            }
        }
        else if ( internalFormat == GVRFMT_LUM_4BIT ||
                  internalFormat == GVRFMT_LUM_8BIT ||
                  internalFormat == GVRFMT_LUM_4BIT_ALPHA ||
                  internalFormat == GVRFMT_LUM_8BIT_ALPHA )
        {
            this->usedColorModel = COLORMODEL_LUMINANCE;
        }
        else if ( internalFormat == GVRFMT_RGB565 ||
                  internalFormat == GVRFMT_RGB5A3 ||
                  internalFormat == GVRFMT_RGBA8888 )
        {
            this->usedColorModel = COLORMODEL_RGBA;
        }
        else
        {
            throw RwException( "failed to map Gamecube internalFormat to color model" );
        }
    }

private:
    AINLINE static bool gcbrowsetexelrgba(
        const void *texelSource, uint32 colorIndex, eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette,
        uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut
    )
    {
        bool hasColor = false;

        const void *realTexelSource = NULL;
        uint32 realColorIndex = 0;
        eGCNativeTextureFormat realInternalFormat;

        bool couldResolveSource = false;

        uint8 prered, pregreen, preblue, prealpha;

        if (paletteType != PALETTE_NONE)
        {
            realTexelSource = paletteData;

            uint8 paletteIndex;

            bool couldResolvePalIndex = getpaletteindex(texelSource, paletteType, maxpalette, itemDepth, colorIndex, paletteIndex);

            if (couldResolvePalIndex)
            {
                realColorIndex = paletteIndex;

                // Get the internal format from the palette pixel type.
                if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
                {
                    realInternalFormat = GVRFMT_LUM_8BIT_ALPHA;
                }
                else if ( palettePixelFormat == GVRPIX_RGB565 )
                {
                    realInternalFormat = GVRFMT_RGB565;
                }
                else if ( palettePixelFormat == GVRPIX_RGB5A3 )
                {
                    realInternalFormat = GVRFMT_RGB5A3;
                }
                else
                {
                    assert( 0 );

                    return false;
                }

                couldResolveSource = true;
            }
        }
        else
        {
            realTexelSource = texelSource;
            realColorIndex = colorIndex;
            realInternalFormat = internalFormat;

            couldResolveSource = true;
        }

        if ( !couldResolveSource )
            return false;

        if ( realInternalFormat == GVRFMT_RGB565 )
        {
            struct pixel_t
            {
                uint16 r : 5;
                uint16 g : 6;
                uint16 b : 5;
            };

            pixel_t srcData = *( (const endian::big_endian <pixel_t>*)realTexelSource + realColorIndex );

            prered = scalecolor( srcData.r, 31, 255 );
            pregreen = scalecolor( srcData.g, 63, 255 );
            preblue = scalecolor( srcData.b, 31, 255 );
            prealpha = 0xFF;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_RGB5A3 )
        {
            struct pixel_t
            {
                union
                {
                    struct
                    {
                        uint16 pad : 15;
                        uint16 hasAlpha : 1;
                    };
                    struct
                    {
                        uint16 b : 5;
                        uint16 g : 5;
                        uint16 r : 5;
                    } no_alpha;
                    struct
                    {
                        uint16 b : 4;
                        uint16 g : 4;
                        uint16 r : 4;
                        uint16 a : 3;
                    } with_alpha;
                };
            };

            pixel_t srcData = *( (const endian::little_endian <pixel_t>*)realTexelSource + realColorIndex );

            if ( !srcData.hasAlpha )
            {
                prered = scalecolor( srcData.with_alpha.r, 15, 255 );
                pregreen = scalecolor( srcData.with_alpha.g, 15, 255 );
                preblue = scalecolor( srcData.with_alpha.b, 15, 255 );
                prealpha = scalecolor( srcData.with_alpha.a, 7, 255 );
            }
            else
            {
                prered = scalecolor( srcData.no_alpha.r, 31, 255 );
                pregreen = scalecolor( srcData.no_alpha.g, 31, 255 );
                preblue = scalecolor( srcData.no_alpha.b, 31, 255 );
                prealpha = 255;
            }

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_RGBA8888 )
        {
            struct pixel_t
            {
                uint8 r;
                uint8 g;
                uint8 b;
                uint8 a;
            };

            const pixel_t *srcData = (const pixel_t*)realTexelSource + realColorIndex;

            prered = srcData->r;
            pregreen = srcData->g;
            preblue = srcData->b;
            prealpha = srcData->a;

            hasColor = true;
        }

        if ( hasColor )
        {
            redOut = prered;
            greenOut = pregreen;
            blueOut = preblue;
            alphaOut = prealpha;
        }

        return hasColor;
    }

public:
    bool getRGBA( const void *texelSource, uint32 colorIndex, uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut ) const
    {
        eColorModel ourModel = this->usedColorModel;

        bool hasColor = false;

        if ( ourModel == COLORMODEL_RGBA )
        {
            hasColor =
                gcbrowsetexelrgba(
                    texelSource, colorIndex, this->internalFormat, this->palettePixelFormat,
                    this->itemDepth, this->paletteType, this->paletteData, this->maxpalette,
                    redOut, greenOut, blueOut, alphaOut
                );
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            uint8 lum;

            hasColor =
                this->getLuminance( texelSource, colorIndex, lum, alphaOut );

            if ( hasColor )
            {
                redOut = lum;
                greenOut = lum;
                blueOut = lum;
            }
        }

        return hasColor;
    }

private:
    AINLINE static bool gcbrowsetexellum( 
        const void *texelSource, uint32 colorIndex, eGCNativeTextureFormat internalFormat, eGCPixelFormat palettePixelFormat,
        uint32 itemDepth, ePaletteType paletteType, const void *paletteData, uint32 maxpalette,
        uint8& lumOut, uint8& alphaOut
    )
    {
        bool hasColor = false;

        const void *realTexelSource = NULL;
        uint32 realColorIndex = 0;
        eGCNativeTextureFormat realInternalFormat;

        bool couldResolveSource = false;

        uint8 prelum, prealpha;

        if (paletteType != PALETTE_NONE)
        {
            realTexelSource = paletteData;

            uint8 paletteIndex;

            bool couldResolvePalIndex = getpaletteindex(texelSource, paletteType, maxpalette, itemDepth, colorIndex, paletteIndex);

            if (couldResolvePalIndex)
            {
                realColorIndex = paletteIndex;

                // Get the internal format from the palette pixel type.
                if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
                {
                    realInternalFormat = GVRFMT_LUM_8BIT_ALPHA;
                }
                else if ( palettePixelFormat == GVRPIX_RGB565 )
                {
                    realInternalFormat = GVRFMT_RGB565;
                }
                else if ( palettePixelFormat == GVRPIX_RGB5A3 )
                {
                    realInternalFormat = GVRFMT_RGB5A3;
                }
                else
                {
                    assert( 0 );

                    return false;
                }

                couldResolveSource = true;
            }
        }
        else
        {
            realTexelSource = texelSource;
            realColorIndex = colorIndex;
            realInternalFormat = internalFormat;

            couldResolveSource = true;
        }

        if ( !couldResolveSource )
            return false;

        if ( realInternalFormat == GVRFMT_LUM_4BIT )
        {
            const PixelFormat::palette4bit *srcData = (const PixelFormat::palette4bit*)realTexelSource;

            uint8 lum_unscaled;
            srcData->getvalue( realColorIndex, lum_unscaled );

            prelum = scalecolor( lum_unscaled, 15, 255 );
            prealpha = 0xFF;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_8BIT )
        {
            struct pixel_t
            {
                uint8 lum;
            };

            const pixel_t *srcData = (const pixel_t*)realTexelSource + realColorIndex;

            prelum = srcData->lum;
            prealpha = 0xFF;

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_4BIT_ALPHA )
        {
            struct pixel_t
            {
                uint8 lum : 4;
                uint8 alpha : 4;
            };

            const pixel_t *srcData = (const pixel_t*)realTexelSource + realColorIndex;

            prelum = scalecolor( srcData->lum, 15, 255 );
            prealpha = scalecolor( srcData->alpha, 15, 255 );

            hasColor = true;
        }
        else if ( realInternalFormat == GVRFMT_LUM_8BIT_ALPHA )
        {
            struct pixel_t
            {
                uint8 lum;
                uint8 alpha;
            };

            pixel_t srcData = *( (const endian::little_endian <pixel_t>*)realTexelSource + realColorIndex );

            prelum = srcData.lum;
            prealpha = srcData.alpha;

            hasColor = true;
        }

        if ( hasColor )
        {
            lumOut = prelum;
            alphaOut = prealpha;
        }

        return hasColor;
    }

public:
    bool getLuminance( const void *texelSource, uint32 colorIndex, uint8& lumOut, uint8& alphaOut ) const
    {
        eColorModel ourModel = this->usedColorModel;

        bool hasColor = false;

        if ( ourModel == COLORMODEL_RGBA )
        {
            uint8 red, green, blue;

            hasColor =
                this->getRGBA( texelSource, colorIndex, red, green, blue, alphaOut );

            if ( hasColor )
            {
                lumOut = rgb2lum( red, green, blue );
            }
        }
        else if ( ourModel == COLORMODEL_LUMINANCE )
        {
            hasColor =
                gcbrowsetexellum(
                    texelSource, colorIndex, this->internalFormat, this->palettePixelFormat,
                    this->itemDepth, this->paletteType, this->paletteData, this->maxpalette,
                    lumOut, alphaOut
                );
        }

        return hasColor;
    }

    void getColor( const void *texelSource, uint32 colorIndex, abstractColorItem& colorOut ) const
    {
        eColorModel colorModel = this->usedColorModel;

        colorOut.model = colorModel;

        if ( colorModel == COLORMODEL_RGBA )
        {
            bool hasColor = this->getRGBA(
                texelSource, colorIndex,
                colorOut.rgbaColor.r,
                colorOut.rgbaColor.g,
                colorOut.rgbaColor.b,
                colorOut.rgbaColor.a
            );

            if ( !hasColor )
            {
                colorOut.rgbaColor.r = 0;
                colorOut.rgbaColor.g = 0;
                colorOut.rgbaColor.b = 0;
                colorOut.rgbaColor.a = 0;
            }
        }
        else if ( colorModel == COLORMODEL_LUMINANCE )
        {
            bool hasColor = this->getLuminance(
                texelSource, colorIndex,
                colorOut.luminance.lum,
                colorOut.luminance.alpha
            );

            if ( !hasColor )
            {
                colorOut.luminance.lum = 0;
                colorOut.luminance.alpha = 0;
            }
        }
        else
        {
            throw RwException( "unknown color model in Gamecube abstract color fetch" );
        }
    }
};

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
        this->palettePixelFormat = GVRPIX_NO_PALETTE;
        this->autoMipmaps = false;
        this->rasterType = 4;
        this->hasAlpha = false;
        this->unk1 = 0;
        this->unk2 = 1;
        this->unk3 = 1;
        this->unk4 = 0;
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
        this->unk1 = right.unk1;
        this->unk2 = right.unk2;
        this->unk3 = right.unk3;
        this->unk4 = right.unk4;
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

    static inline void getSizeRules( eGCNativeTextureFormat fmt, nativeTextureSizeRules& rulesOut )
    {
        bool isDXT = ( fmt == GVRFMT_CMP );

        rulesOut.powerOfTwo = true;
        rulesOut.squared = false;
        rulesOut.multipleOf = ( isDXT == true );
        rulesOut.multipleOfValue = ( isDXT ? 4 : 0 );
        rulesOut.maximum = true;
        rulesOut.maxVal = 1024;
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

    // TODO: find out what these are.
    uint32 unk1;
    uint32 unk2;
    uint32 unk3;
    uint32 unk4;
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

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        // TODO.
        return RASTER_DEFAULT;
    }

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

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        if ( nativeTex->internalFormat == GVRFMT_CMP )
        {
            return RWCOMPRESS_DXT1;
        }

        return RWCOMPRESS_NONE;
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

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        // TODO.
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // I think a Gamecube native texture works very similar to PS2 and XBOX, since it is
        // hardware of the same generation.
        const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

        NativeTextureGC::getSizeRules( nativeTex->internalFormat, rulesOut );
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
    eGCPixelFormat palettePixelFormat;

    endian::big_endian <uint32> hasAlpha;
};
#pragma pack()

};

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE