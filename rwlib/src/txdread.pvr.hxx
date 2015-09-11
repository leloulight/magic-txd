#include "txdread.nativetex.hxx"

#include <PVRTextureUtilities.h>

#include "txdread.d3d.genmip.hxx"

#define PLATFORM_PVR    10

namespace rw
{

inline uint32 getPVRToolTextureDataRowAlignment( void )
{
    // Since PowerVR is a compressed format, there is no real row alignment.
    // This row alignment is supposedly what the PowerVR encoding tool expects.
    return 4;
}

inline uint32 getPVRExportTextureDataRowAlignment( void )
{
    // We return a size here that is preferred by the runtime.
    return 4;
}

enum ePVRInternalFormat
{
    GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG = 0x8C00,
    GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG = 0x8C01,
    GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG = 0x8C02,
    GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG = 0x8C03
};

inline uint32 getDepthByPVRFormat( ePVRInternalFormat theFormat )
{
    uint32 formatDepth = 0;

    if ( theFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG ||
         theFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG )
    {
        formatDepth = 4;
    }
    else if ( theFormat == GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG ||
              theFormat == GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG )
    {
        formatDepth = 2;
    }
    else
    {
        assert( 0 );
    }

    return formatDepth;
}

struct NativeTexturePVR
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTexturePVR( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();

        this->internalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
        this->hasAlpha = false;

        this->unk1 = 0;
        this->unk8 = 0;
    }

    inline NativeTexturePVR( const NativeTexturePVR& right )
    {
        // Copy parameters.
        this->engineInterface = right.engineInterface;
        this->texVersion = right.texVersion;
        this->internalFormat = right.internalFormat;
        this->hasAlpha = right.hasAlpha;
        this->unk1 = right.unk1;
        this->unk8 = right.unk8;

        // Copy mipmaps.
        copyMipmapLayers( this->engineInterface, right.mipmaps, this->mipmaps );
    }

    inline void clearImageData( void )
    {
        // Delete mipmap layers.
        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTexturePVR( void )
    {
        this->clearImageData();
    }

    typedef genmip::mipmapLayer mipmapLayer;

    std::vector <mipmapLayer> mipmaps;

    ePVRInternalFormat internalFormat;

    bool hasAlpha;

    // Unknowns.
    uint8 unk1;
    uint32 unk8;
};

inline void getPVRNativeTextureSizeRules( nativeTextureSizeRules& rulesOut )
{
    rulesOut.powerOfTwo = true;
    rulesOut.squared = false;   // NOT SURE.
    rulesOut.maximum = true;
    rulesOut.maxVal = 2048;
}

struct pvrNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        new (objMem) NativeTexturePVR( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize )
    {
        new (objMem) NativeTexturePVR( *(const NativeTexturePVR*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize )
    {
        ( *(NativeTexturePVR*)objMem ).~NativeTexturePVR();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const
    {
        storeCaps.pixelCaps.supportsDXT1 = false;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = false;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = false;
        storeCaps.pixelCaps.supportsPalette = false;

        storeCaps.isCompressedFormat = true;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version )
    {
        NativeTexturePVR *nativeTex = (NativeTexturePVR*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem )
    {
        const NativeTexturePVR *nativeTex = (const NativeTexturePVR*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        return RASTER_DEFAULT;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        return PALETTE_NONE;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        return true;
    }

    eCompressionType GetTextureCompressionFormat( const void *objMem ) override
    {
        return RWCOMPRESS_NONE;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTexturePVR *nativeTex = (const NativeTexturePVR*)objMem;

        return nativeTex->hasAlpha;
    }

    void GetFormatSizeRules( const pixelFormat& format, nativeTextureSizeRules& rulesOut ) const override
    {
        getPVRNativeTextureSizeRules( rulesOut );
    }

    void GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const override
    {
        // The PowerVR native texture seems to be very optimized, limited and not future proof.
        // I am uncertain about the exact rules (a throwback to the good old days!).
        getPVRNativeTextureSizeRules( rulesOut );
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Once again, we are compressing our contents.
        // Row alignment never plays a role here.
        return 0;
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // Has not been officially defined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "PowerVR", this, sizeof( NativeTexturePVR ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "PowerVR" );
    }
};

namespace pvr
{

#pragma pack(1)
struct textureMetaHeaderGeneric
{
    endian::little_endian <uint32> platformDescriptor;

    wardrumFormatInfo formatInfo;

    uint8 pad1[ 0x10 ];

    char name[ 32 ];
    char maskName[ 32 ];

    uint8 mipmapCount;
    uint8 unk1;
    bool hasAlpha;

    uint8 pad2;

    endian::little_endian <uint16> width, height;

    endian::little_endian <ePVRInternalFormat> internalFormat;
    endian::little_endian <uint32> imageDataStreamSize;
    endian::little_endian <uint32> unk8;
};
#pragma pack()

};

};