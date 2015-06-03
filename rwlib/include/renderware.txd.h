/*
 * TXDs
 */

struct Texture : public RwObject
{
    inline Texture( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->filterFlags = 0;
        this->hasSkyMipmap = false;
    }

	uint32 filterFlags;
	std::string name;
	std::string maskName;

	/* Extensions */

	/* sky mipmap */
	bool hasSkyMipmap;

	/* functions */
	void read(std::istream &dff);
	uint32 write(std::ostream &dff);
	void readExtension(std::istream &dff);
	void dump(std::string ind = "");
};

enum eRasterStageFilterMode
{
    RWFILTER_DISABLE,
    RWFILTER_POINT,
    RWFILTER_LINEAR,
    RWFILTER_POINT_POINT,
    RWFILTER_LINEAR_POINT,
    RWFILTER_POINT_LINEAR,
    RWFILTER_LINEAR_LINEAR,
    RWFILTER_ANISOTROPY
};
enum eRasterStageAddressMode
{
    RWTEXADDRESS_WRAP = 1,
    RWTEXADDRESS_MIRROR,
    RWTEXADDRESS_CLAMP,
    RWTEXADDRESS_BORDER
};

struct TextureBase;

struct texFormatInfo
{
private:
    uint32 filterMode : 8;
    uint32 uAddressing : 4;
    uint32 vAddressing : 4;
    uint32 pad1 : 16;

public:
    void parse(TextureBase& theTexture) const;
    void set(const TextureBase& inTex);
};

struct wardrumFormatInfo
{
private:
    uint32 filterMode;
    uint32 uAddressing;
    uint32 vAddressing;

public:
    void parse(TextureBase& theTexture) const;
    void set(const TextureBase& inTex );
};

#include "renderware.txd.pixelformat.h"

struct ps2MipmapTransmissionData
{
    uint16 destX, destY;
};

// Native GTA:SA feature map:
// no RASTER_PAL4 support at all.
// if RASTER_PAL8, then only RASTER_8888 and RASTER_888
// else 

enum eRasterFormat
{
    RASTER_DEFAULT,
    RASTER_1555,
    RASTER_565,
    RASTER_4444,
    RASTER_LUM8,        // D3DFMT_A4L4
    RASTER_8888,
    RASTER_888,
    RASTER_16,          // D3DFMT_D16
    RASTER_24,          // D3DFMT_D24X8
    RASTER_32,          // D3DFMT_D32
    RASTER_555          // D3DFMT_X1R5G5B5
};

inline bool isValidRasterFormat(eRasterFormat rasterFormat)
{
    bool isValidRaster = false;

    if (rasterFormat == RASTER_1555 ||
        rasterFormat == RASTER_565 ||
        rasterFormat == RASTER_4444 ||
        rasterFormat == RASTER_LUM8 ||
        rasterFormat == RASTER_8888 ||
        rasterFormat == RASTER_888 ||
        rasterFormat == RASTER_555)
    {
        isValidRaster = true;
    }

    return isValidRaster;
}

enum ePaletteType
{
    PALETTE_NONE,
    PALETTE_4BIT,
    PALETTE_8BIT
};

enum eColorOrdering
{
    COLOR_RGBA,
    COLOR_BGRA,
    COLOR_ABGR
};

// utility to calculate palette item count.
inline uint32 getPaletteItemCount( ePaletteType paletteType )
{
    uint32 count = 0;

    if ( paletteType == PALETTE_4BIT )
    {
        count = 16;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        count = 256;
    }
    else if ( paletteType == PALETTE_NONE )
    {
        count = 0;
    }
    else
    {
        assert( 0 );
    }

    return count;
}

#include "renderware.bmp.h"

// Texture container per platform for specialized color data.
typedef void* PlatformTexture;

// Useful routine to generate generic raster format flags.
inline uint32 generateRasterFormatFlags( eRasterFormat rasterFormat, ePaletteType paletteType, bool hasMipmaps, bool autoMipmaps )
{
    uint32 rasterFlags = 0;

    // bits 0..3 can be (alternatively) used for the raster type
    // bits 4..8 are stored in the raster private flags.

    rasterFlags |= ( (uint32)rasterFormat << 8 );

    if ( paletteType == PALETTE_4BIT )
    {
        rasterFlags |= RASTER_PAL4;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        rasterFlags |= RASTER_PAL8;
    }

    if ( hasMipmaps )
    {
        rasterFlags |= RASTER_MIPMAP;
    }

    if ( autoMipmaps )
    {
        rasterFlags |= RASTER_AUTOMIPMAP;
    }

    return rasterFlags;
}

// Useful routine to read generic raster format flags.
inline void readRasterFormatFlags( uint32 rasterFormatFlags, eRasterFormat& rasterFormat, ePaletteType& paletteType, bool& hasMipmaps, bool& autoMipmaps )
{
    rasterFormat = (eRasterFormat)( ( rasterFormatFlags & 0xF00 ) >> 8 );
    
    if ( ( rasterFormatFlags & RASTER_PAL4 ) != 0 )
    {
        paletteType = PALETTE_4BIT;
    }
    else if ( ( rasterFormatFlags & RASTER_PAL8 ) != 0 )
    {
        paletteType = PALETTE_8BIT;
    }
    else
    {
        paletteType = PALETTE_NONE;
    }

    hasMipmaps = ( rasterFormatFlags & RASTER_MIPMAP ) != 0;
    autoMipmaps = ( rasterFormatFlags & RASTER_AUTOMIPMAP ) != 0;
}

enum eMipmapGenerationMode
{
    MIPMAPGEN_DEFAULT,
    MIPMAPGEN_CONTRAST,
    MIPMAPGEN_BRIGHTEN,
    MIPMAPGEN_DARKEN,
    MIPMAPGEN_SELECTCLOSE
};

enum eRasterType
{
    RASTERTYPE_DEFAULT,     // same as 4
    RASTERTYPE_BITMAP = 4
};

struct Raster
{
    inline Raster( Interface *engineInterface, void *construction_params )
    {
        this->engineInterface = engineInterface;
        this->platformData = NULL;
        this->refCount = 1;
    }

    Raster( const Raster& right );
    ~Raster( void );

    void SetEngineVersion( LibraryVersion version );
    LibraryVersion GetEngineVersion( void ) const;

    void writeTGA(const char *path, bool optimized = false);
    void writeTGAStream(Stream *tga, bool optimized = false);

    Bitmap getBitmap(void) const;
    void setImageData(const Bitmap& srcImage);

    void resize(uint32 width, uint32 height);

    void getSize(uint32& width, uint32& height) const;

    void newNativeData( const char *typeName );
    void clearNativeData( void );

    bool hasNativeDataOfType( const char *typeName ) const;

    void* getNativeInterface( void );

	void convertToFormat(eRasterFormat format);
    void convertToPalette(ePaletteType paletteType);

    // Optimization routines.
    void optimizeForLowEnd(float quality);
    void compress(float quality);

    // Mipmap utilities.
    uint32 getMipmapCount( void ) const;

    // Data members.
    Interface *engineInterface;

    PlatformTexture *platformData;

    uint32 refCount;
};

struct TexDictionary;

struct TextureBase : public RwObject
{
    friend struct TexDictionary;

    inline TextureBase( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->texRaster = NULL;
        this->filterMode = RWFILTER_DISABLE;
        this->uAddressing = RWTEXADDRESS_WRAP;
        this->vAddressing = RWTEXADDRESS_WRAP;
        this->texDict = NULL;
    }

    TextureBase( const TextureBase& right );
    ~TextureBase( void );

    void OnVersionChange( const LibraryVersion& oldVer, const LibraryVersion& newVer )
    {
        // If we have a raster, set its version aswell.
        if ( Raster *texRaster = this->texRaster )
        {
            texRaster->SetEngineVersion( newVer );
        }
    }

    void SetName( const char *nameString )                      { this->name = nameString; }
    void SetMaskName( const char *nameString )                  { this->maskName = nameString; }

    const std::string& GetName( void ) const                    { return this->name; }
    const std::string& GetMaskName( void ) const                { return this->maskName; }

    void AddToDictionary( TexDictionary *dict );
    void RemoveFromDictionary( void );
    TexDictionary* GetTexDictionary( void ) const;

    void SetFilterMode( eRasterStageFilterMode filterMode )     { this->filterMode = filterMode; }
    void SetUAddressing( eRasterStageAddressMode mode )         { this->uAddressing = mode; }
    void SetVAddressing( eRasterStageAddressMode mode )         { this->vAddressing = mode; }

    eRasterStageFilterMode GetFilterMode( void ) const          { return this->filterMode; }
    eRasterStageAddressMode GetUAddressing( void ) const        { return this->uAddressing; }
    eRasterStageAddressMode GetVAddressing( void ) const        { return this->vAddressing; }

    void improveFiltering(void);

    // Mipmap utilities.
    void clearMipmaps( void );
    void generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode = MIPMAPGEN_DEFAULT );

    void SetRaster( Raster *texRaster );

    Raster* GetRaster( void ) const
    {
        return this->texRaster;
    }

private:
    // Pointer to the pixel data storage.
    Raster *texRaster;

	std::string name;
	std::string maskName;
	eRasterStageFilterMode filterMode;

    eRasterStageAddressMode uAddressing;
    eRasterStageAddressMode vAddressing;

    TexDictionary *texDict;

public:
    RwListEntry <TextureBase> texDictNode;
};

template <typename structType, size_t nodeOffset>
struct rwListIterator
{
    inline rwListIterator( RwList <structType>& rootNode )
    {
        this->rootNode = &rootNode.root;

        this->list_iterator = this->rootNode->next;
    }

    inline rwListIterator( const rwListIterator& right )
    {
        this->rootNode = right.rootNode;

        this->list_iterator = right.list_iterator;
    }

    inline void operator = ( const rwListIterator& right )
    {
        this->rootNode = right.rootNode;

        this->list_iterator = right.list_iterator;
    }

    inline bool IsEnd( void ) const
    {
        return ( this->rootNode == this->list_iterator );
    }

    inline void Increment( void )
    {
        this->list_iterator = this->list_iterator->next;
    }

    inline structType* Resolve( void ) const
    {
        return (structType*)( (char*)this->list_iterator - nodeOffset );
    }

    RwListEntry <structType> *rootNode;
    RwListEntry <structType> *list_iterator;
};

#define DEF_LIST_ITER( newName, structType, nodeName )   typedef rwListIterator <structType, offsetof(structType, nodeName)> newName;

struct TexDictionary : public RwObject
{
    inline TexDictionary( Interface *engineInterface, void *construction_params ) : RwObject( engineInterface, construction_params )
    {
        this->hasRecommendedPlatform = false;

        this->numTextures = 0;

        LIST_CLEAR( textures.root );
    }

    TexDictionary( const TexDictionary& right );
	~TexDictionary( void );

    bool hasRecommendedPlatform;

    uint32 numTextures;

    RwList <TextureBase> textures;

    DEF_LIST_ITER( texIter_t, TextureBase, texDictNode );

	/* functions */
	void clear( void );

    inline texIter_t GetTextureIterator( void )
    {
        return texIter_t( this->textures );
    }

    inline uint32 GetTextureCount( void ) const
    {
        return this->numTextures;
    }
};

enum eCompressionType
{
    RWCOMPRESS_NONE,
    RWCOMPRESS_DXT1,
    RWCOMPRESS_DXT2,
    RWCOMPRESS_DXT3,
    RWCOMPRESS_DXT4,
    RWCOMPRESS_DXT5,
    RWCOMPRESS_NUM
};

// Pixel capabilities are required for transporting data properly.
struct pixelCapabilities
{
    inline pixelCapabilities( void )
    {
        this->supportsDXT1 = false;
        this->supportsDXT2 = false;
        this->supportsDXT3 = false;
        this->supportsDXT4 = false;
        this->supportsDXT5 = false;
        this->supportsPalette = false;
    }

    bool supportsDXT1;
    bool supportsDXT2;
    bool supportsDXT3;
    bool supportsDXT4;
    bool supportsDXT5;
    bool supportsPalette;
};

struct storageCapabilities
{
    inline storageCapabilities( void )
    {
        this->isCompressedFormat = false;
    }

    pixelCapabilities pixelCaps;

    bool isCompressedFormat;
};

struct pixelFormat
{
    inline pixelFormat( void )
    {
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->colorOrder = COLOR_RGBA;
        this->paletteType = PALETTE_NONE;
        this->compressionType = RWCOMPRESS_NONE;
    }

    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    eCompressionType compressionType;
};

struct pixelDataTraversal
{
    inline pixelDataTraversal( void )
    {
        this->isNewlyAllocated = false;
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->colorOrder = COLOR_RGBA;
        this->paletteType = PALETTE_NONE;
        this->paletteData = NULL;
        this->paletteSize = 0;
        this->compressionType = RWCOMPRESS_NONE;
        this->hasAlpha = false;
        this->autoMipmaps = false;
        this->cubeTexture = false;
        this->rasterType = 4;
    }

    void CloneFrom( Interface *engineInterface, const pixelDataTraversal& right );

    void FreePixels( Interface *engineInterface );

    inline void SetStandalone( void )
    {
        // Standalone pixels mean that they do not belong to any texture container anymore.
        // If we are the only owner, we must make sure that we free them.
        // This function was introduced to defeat a memory leak.
        this->isNewlyAllocated = true;
    }

    inline void DetachPixels( void )
    {
        if ( this->isNewlyAllocated )
        {
            this->mipmaps.clear();
            this->paletteData = NULL;

            this->isNewlyAllocated = false;
        }
    }

    // Mipmaps.
    struct mipmapResource
    {
        inline mipmapResource( void )
        {
            // Debug fill the mipmap struct.
            this->texels = NULL;
            this->width = 0;
            this->height = 0;
            this->mipWidth = 0;
            this->mipHeight = 0;
            this->dataSize = 0;
        }

        void Free( Interface *engineInterface );

        void *texels;
        uint32 width, height;
        uint32 mipWidth, mipHeight; // do not update these fields.

        uint32 dataSize;
    };

    typedef std::vector <mipmapResource> mipmaps_t;

    mipmaps_t mipmaps;

    bool isNewlyAllocated;          // counts for all mipmap layers and palette data.
    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;

    // More advanced properties.
    bool hasAlpha;
    bool autoMipmaps;
    bool cubeTexture;
    uint8 rasterType;
};

enum eTexNativeCompatibility
{
    RWTEXCOMPAT_NONE,
    RWTEXCOMPAT_MAYBE,
    RWTEXCOMPAT_ABSOLUTE
};

struct RwUnsupportedOperationException : public RwException
{
    inline RwUnsupportedOperationException( const char *msg ) : RwException( msg )
    {
        return;
    }
};

// Include native texture extension public headers.
#include "renderware.txd.d3d.h"

struct rawMipmapLayer
{
    pixelDataTraversal::mipmapResource mipData;

    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;

    bool hasAlpha;

    bool isNewlyAllocated;
};

struct nativeTextureBatchedInfo
{
    uint32 mipmapCount;
    uint32 baseWidth, baseHeight;
};

struct texNativeTypeProvider abstract
{
    inline texNativeTypeProvider( void )
    {
        this->managerData.rwTexType = NULL;
        this->managerData.isRegistered = false;
    }

    inline ~texNativeTypeProvider( void )
    {
        if ( this->managerData.isRegistered )
        {
            LIST_REMOVE( this->managerData.managerNode );

            this->managerData.isRegistered = false;
        }
    }

    // Type management.
    virtual void            ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) = 0;
    virtual void            CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) = 0;
    virtual void            DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) = 0;

    // Serialization functions.
    virtual eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const throw( ... ) = 0;

    virtual void            SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const throw( ... ) = 0;
    virtual void            DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const throw( ... ) = 0;

    // Conversion parameters.
    virtual void            GetPixelCapabilities( pixelCapabilities& capsOut ) const = 0;
    virtual void            GetStorageCapabilities( storageCapabilities& storeCaps ) const = 0;

    struct acquireFeedback_t
    {
        inline acquireFeedback_t( void )
        {
            this->hasDirectlyAcquired = true;
        }

        bool hasDirectlyAcquired;
    };

    virtual void            GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut ) throw( ... ) = 0;
    virtual void            SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut ) throw( ... ) = 0;
    virtual void            UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate ) = 0;

    // Native Texture version information API.
    virtual void            SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) = 0;
    virtual LibraryVersion  GetTextureVersion( const void *objMem ) = 0;

    // TODO: add meta-data traversal API using dynamic extendable object.

    // Mipmap manipulation API.
    virtual bool            GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut ) = 0;
    virtual bool            AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut ) = 0;
    virtual void            ClearMipmaps( Interface *engineInterface, void *objMem ) = 0;

    // Information API.
    virtual void            GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut ) = 0;

    virtual ePaletteType    GetTexturePaletteType( const void *objMem ) = 0;
    virtual bool            IsTextureCompressed( const void *objMem ) = 0;

    virtual bool            DoesTextureHaveAlpha( const void *objMem ) = 0;

    // If you extend this method, your native texture can export a public API interface to the application.
    // This will be an optimized junction point between native internals and high level API, so use it with caution.
    // Note that if internals change the application must take that into account.
    virtual void*           GetNativeInterface( void *objMem )
    {
        return NULL;
    }

    virtual bool            GetDebugBitmap( Interface *engineInterface, void *objMem, Bitmap& drawTarget ) const
    {
        // Native textures type providers can override this method to provide visual insight into their structure.
        // Returning true means that anything has been drawn to drawTarget.
        return false;
    }

    // Driver identification functions.
    virtual uint32          GetDriverIdentifier( void *objMem ) const
    {
        // By returning 0, this texture type does not recommend a particular driver.
        return 0;
    }

    struct
    {
        RwTypeSystem::typeInfoBase *rwTexType;

        RwListEntry <texNativeTypeProvider> managerNode;

        bool isRegistered;
    } managerData;
};

// Plugin methods.
TexDictionary* CreateTexDictionary( Interface *engineInterface );
TexDictionary* ToTexDictionary( Interface *engineInterface, RwObject *rwObj );

TextureBase* CreateTexture( Interface *engineInterface, Raster *theRaster );
TextureBase* ToTexture( Interface *engineInterface, RwObject *rwObj );

// Complex native texture API.
bool RegisterNativeTextureType( Interface *engineInterface, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize );
bool UnregisterNativeTextureType( Interface *engineInterface, const char *nativeName );
bool ConvertRasterTo( Raster *theRaster, const char *nativeName );

// Raster API.
Raster* CreateRaster( Interface *engineInterface );
Raster* AcquireRaster( Raster *theRaster );
void DeleteRaster( Raster *theRaster );

// Debug API.
bool DebugDrawMipmaps( Interface *engineInterface, Raster *debugRaster, Bitmap& drawSurface );