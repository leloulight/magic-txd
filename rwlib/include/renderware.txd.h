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

    void writeToBlock(BlockProvider& outputProvider) const;
    void readFromBlock(BlockProvider& inputProvider);
};

template <typename userType = endian::little_endian <uint32>>
struct texFormatInfo_serialized
{
    userType info;

    inline operator texFormatInfo ( void ) const
    {
        texFormatInfo formatOut;

        *(uint32*)&formatOut = info;

        return formatOut;
    }

    inline void operator = ( const texFormatInfo& right )
    {
        info = *(uint32*)&right;
    }
};

struct wardrumFormatInfo
{
private:
    endian::little_endian <uint32> filterMode;
    endian::little_endian <uint32> uAddressing;
    endian::little_endian <uint32> vAddressing;

public:
    void parse(TextureBase& theTexture) const;
    void set(const TextureBase& inTex );
};

struct rasterSizeRules
{
    inline rasterSizeRules( void )
    {
        this->powerOfTwo = false;
        this->squared = false;
        this->multipleOf = false;
        this->multipleOfValue = 0;
        this->maximum = false;
        this->maxVal = 0;
    }

    bool powerOfTwo;
    bool squared;
    bool multipleOf;
    uint32 multipleOfValue;
    bool maximum;
    uint32 maxVal;

    bool verifyDimensions( uint32 width, uint32 height ) const;
    void adjustDimensions( uint32 width, uint32 height, uint32& newWidth, uint32& newHeight ) const;
};

#include "renderware.txd.pixelformat.h"

// This is our library-wide supported sample format enumeration.
// Feel free to extend this. You must not serialize this anywhere,
// since this type is version-agnostic.
enum eRasterFormat
{
    RASTER_DEFAULT,
    RASTER_1555,
    RASTER_565,
    RASTER_4444,
    RASTER_LUM,         // D3DFMT_L8 (can be 4 or 8 bit)
    RASTER_8888,
    RASTER_888,
    RASTER_16,          // D3DFMT_D16
    RASTER_24,          // D3DFMT_D24X8
    RASTER_32,          // D3DFMT_D32
    RASTER_555,         // D3DFMT_X1R5G5B5

    // New types. :)
    RASTER_LUM_ALPHA
};

inline bool canRasterFormatHaveAlpha(eRasterFormat rasterFormat)
{
    if (rasterFormat == RASTER_1555 ||
        rasterFormat == RASTER_4444 ||
        rasterFormat == RASTER_8888 ||
        // NEW FORMATS.
        rasterFormat == RASTER_LUM_ALPHA )
    {
        return true;
    }
    
    return false;
}

enum ePaletteType
{
    PALETTE_NONE,
    PALETTE_4BIT,
    PALETTE_8BIT,
    PALETTE_4BIT_LSB    // same as 4BIT, but different addressing order
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

    if ( paletteType == PALETTE_4BIT ||
         paletteType == PALETTE_4BIT_LSB )
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

// Native GTA:SA feature map:
// no RASTER_PAL4 support at all.
// if RASTER_PAL8, then only RASTER_8888 and RASTER_888
// else 

// Those are all sample types from RenderWare version 3.
enum rwSerializedRasterFormat_3
{
    RWFORMAT3_UNKNOWN,
    RWFORMAT3_1555,
    RWFORMAT3_565,
    RWFORMAT3_4444,
    RWFORMAT3_LUM8,
    RWFORMAT3_8888,
    RWFORMAT3_888,
    RWFORMAT3_16,
    RWFORMAT3_24,
    RWFORMAT3_32,
    RWFORMAT3_555
};

// Useful routine to generate generic raster format flags.
inline uint32 generateRasterFormatFlags( eRasterFormat rasterFormat, ePaletteType paletteType, bool hasMipmaps, bool autoMipmaps )
{
    uint32 rasterFlags = 0;

    // bits 0..3 can be (alternatively) used for the raster type
    // bits 4..8 are stored in the raster private flags.

    // Map the raster format or RenderWare3.
    rwSerializedRasterFormat_3 serFormat = RWFORMAT3_UNKNOWN;

    if ( rasterFormat != RASTER_DEFAULT )
    {
        if ( rasterFormat == RASTER_1555 )
        {
            serFormat = RWFORMAT3_1555;
        }
        else if ( rasterFormat == RASTER_565 )
        {
            serFormat = RWFORMAT3_565;
        }
        else if ( rasterFormat == RASTER_4444 )
        {
            serFormat = RWFORMAT3_4444;
        }
        else if ( rasterFormat == RASTER_LUM )
        {
            serFormat = RWFORMAT3_LUM8;
        }
        else if ( rasterFormat == RASTER_8888 )
        {
            serFormat = RWFORMAT3_8888;
        }
        else if ( rasterFormat == RASTER_888 )
        {
            serFormat = RWFORMAT3_888;
        }
        else if ( rasterFormat == RASTER_16 )
        {
            serFormat = RWFORMAT3_16;
        }
        else if ( rasterFormat == RASTER_24 )
        {
            serFormat = RWFORMAT3_24;
        }
        else if ( rasterFormat == RASTER_32 )
        {
            serFormat = RWFORMAT3_32;
        }
        else if ( rasterFormat == RASTER_555 )
        {
            serFormat = RWFORMAT3_555;
        }
        // otherwise, well we failed.
        // snap, we dont have a format!
        // hopefully the implementation knows what it is doing!
    }

    rasterFlags |= ( (uint32)serFormat << 8 );

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
    rwSerializedRasterFormat_3 serFormat = (rwSerializedRasterFormat_3)( ( rasterFormatFlags & 0xF00 ) >> 8 );
    
    // Map the serialized format to our raster format.
    // We should be backwards compatible to every RenderWare3 format.
    eRasterFormat formatOut = RASTER_DEFAULT;

    if ( serFormat == RWFORMAT3_1555 )
    {
        formatOut = RASTER_1555;
    }
    else if ( serFormat == RWFORMAT3_565 )
    {
        formatOut = RASTER_565;
    }
    else if ( serFormat == RWFORMAT3_4444 )
    {
        formatOut = RASTER_4444;
    }
    else if ( serFormat == RWFORMAT3_LUM8 )
    {
        formatOut = RASTER_LUM;
    }
    else if ( serFormat == RWFORMAT3_8888 )
    {
        formatOut = RASTER_8888;
    }
    else if ( serFormat == RWFORMAT3_888 )
    {
        formatOut = RASTER_888;
    }
    else if ( serFormat == RWFORMAT3_16 )
    {
        formatOut = RASTER_16;
    }
    else if ( serFormat == RWFORMAT3_24 )
    {
        formatOut = RASTER_24;
    }
    else if ( serFormat == RWFORMAT3_32 )
    {
        formatOut = RASTER_32;
    }
    else if ( serFormat == RWFORMAT3_555 )
    {
        formatOut = RASTER_555;
    }
    // anything else will be an unknown raster mapping.

    rasterFormat = formatOut;

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

    bool supportsImageMethod(const char *method) const;
    void writeImage(rw::Stream *outputStream, const char *method);
    void readImage(rw::Stream *inputStream);

    Bitmap getBitmap(void) const;
    void setImageData(const Bitmap& srcImage);

    void resize(uint32 width, uint32 height, const char *downsampleMode = NULL, const char *upscaleMode = NULL);

    void getSize(uint32& width, uint32& height) const;

    void newNativeData( const char *typeName );
    void clearNativeData( void );

    bool hasNativeDataOfType( const char *typeName ) const;
    const char* getNativeDataTypeName( void ) const;

    void* getNativeInterface( void );
    void* getDriverNativeInterface( void );

    void getSizeRules( rasterSizeRules& rulesOut ) const;

    void getFormatString( char *buf, size_t bufSize, size_t& lengthOut ) const;

	void convertToFormat(eRasterFormat format);
    void convertToPalette(ePaletteType paletteType, eRasterFormat newRasterFormat = RASTER_DEFAULT);

    eRasterFormat getRasterFormat( void ) const;
    ePaletteType getPaletteType( void ) const;

    // Optimization routines.
    void optimizeForLowEnd(float quality);
    void compress(float quality);
    void compressCustom(eCompressionType format);

    bool isCompressed( void ) const;
    eCompressionType getCompressionFormat( void ) const;

    // Mipmap utilities.
    uint32 getMipmapCount( void ) const;

    void clearMipmaps( void );
    void generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode = MIPMAPGEN_DEFAULT );

    // Data members.
    Interface *engineInterface;

    PlatformTexture *platformData;

    std::atomic <uint32> refCount;
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
    void fixFiltering(void);

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

    bool isCompressedFormat;    // if true then this texture does not store raw texel data.
};

struct pixelFormat
{
    inline pixelFormat( void )
    {
        this->rasterFormat = RASTER_DEFAULT;
        this->depth = 0;
        this->rowAlignment = 0;
        this->colorOrder = COLOR_RGBA;
        this->paletteType = PALETTE_NONE;
        this->compressionType = RWCOMPRESS_NONE;
    }

    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
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
        this->rowAlignment = 0;
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

    static void FreeMipmap( Interface *engineInterface, mipmapResource& mipData );

    typedef std::vector <mipmapResource> mipmaps_t;

    mipmaps_t mipmaps;

    bool isNewlyAllocated;          // counts for all mipmap layers and palette data.
    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
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

// Plugin methods.
TexDictionary* CreateTexDictionary( Interface *engineInterface );
TexDictionary* ToTexDictionary( Interface *engineInterface, RwObject *rwObj );

TextureBase* CreateTexture( Interface *engineInterface, Raster *theRaster );
TextureBase* ToTexture( Interface *engineInterface, RwObject *rwObj );

typedef std::list <std::string> platformTypeNameList_t;

struct rawMipmapLayer
{
    pixelDataTraversal::mipmapResource mipData;

    eRasterFormat rasterFormat;
    uint32 depth;
    uint32 rowAlignment;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;

    bool hasAlpha;

    bool isNewlyAllocated;
};

// Complex native texture API.
bool ConvertRasterTo( Raster *theRaster, const char *nativeName );

void* GetNativeTextureDriverInterface( Interface *engineInterface, const char *nativeName );

const char* GetNativeTextureImageFormatExtension( Interface *engineInterface, const char *nativeName );

platformTypeNameList_t GetAvailableNativeTextureTypes( Interface *engineInterface );

struct nativeRasterFormatInfo
{
    // Basic information.
    bool isCompressedFormat;
    
    // Support flags.
    bool supportsDXT1;
    bool supportsDXT2;
    bool supportsDXT3;
    bool supportsDXT4;
    bool supportsDXT5;
    bool supportsPalette;
};

bool GetNativeTextureFormatInfo( Interface *engineInterface, const char *nativeName, nativeRasterFormatInfo& infoOut );

// Format info helper API.
const char* GetRasterFormatStandardName( eRasterFormat theFormat );
eRasterFormat FindRasterFormatByName( const char *name );

// Raster API.
Raster* CreateRaster( Interface *engineInterface );
Raster* CloneRaster( const Raster *rasterToClone );
Raster* AcquireRaster( Raster *theRaster );
void DeleteRaster( Raster *theRaster );

// Pixel manipulation API, exported for good compatibility.
// Use this API if you are not sure how to map the raster format stuff properly.
bool BrowseTexelRGBA(
    const void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut
);
bool BrowseTexelLuminance(
    const void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, ePaletteType paletteType, const void *paletteData, uint32 paletteSize,
    uint8& lumOut, uint8& alphaOut
);

// Use this function to decide the functions you should use for color manipulation.
eColorModel GetRasterFormatColorModel( eRasterFormat rasterFormat );

bool PutTexelRGBA(
    void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth, eColorOrdering colorOrder,
    uint8 red, uint8 green, uint8 blue, uint8 alpha
);
bool PutTexelLuminance(
    void *texelSource, uint32 texelIndex,
    eRasterFormat rasterFormat, uint32 depth,
    uint8 lum, uint8 alpha
);

// Debug API.
bool DebugDrawMipmaps( Interface *engineInterface, Raster *debugRaster, Bitmap& drawSurface );