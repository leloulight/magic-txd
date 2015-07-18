#ifndef AINLINE
#ifdef _MSC_VER
#define AINLINE __forceinline
#elif __linux__
#define AINLINE __attribute__((always_inline))
#else
#define AINLINE
#endif
#endif

#include <MemoryUtils.h>

#include "renderware.h"

// Include the RenderWare configuration file.
// This one should be private to the rwtools project, hence we reject including it in "renderware.h"
#include "../rwconf.h"

namespace rw
{

struct EngineInterface : public Interface
{
    inline EngineInterface( void )
    {
        // We default to the San Andreas engine.
        this->version = KnownVersions::getGameVersion( KnownVersions::SA );
    }

    inline ~EngineInterface( void )
    {
        // We are done.
        return;
    }
};

// Factory for global RenderWare interfaces.
typedef StaticPluginClassFactory <EngineInterface> RwInterfaceFactory_t;

typedef RwInterfaceFactory_t::pluginOffset_t RwInterfacePluginOffset_t;

extern RwInterfaceFactory_t engineFactory;

struct rawBitmapFetchResult
{
    void *texelData;
    uint32 dataSize;
    uint32 width, height;
    bool isNewlyAllocated;
    uint32 depth;
    uint32 rowAlignment;
    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    void *paletteData;
    uint32 paletteSize;
    ePaletteType paletteType;

    void FreePixels( Interface *engineInterface )
    {
        engineInterface->PixelFree( this->texelData );

        this->texelData = NULL;

        if ( void *paletteData = this->paletteData )
        {
            engineInterface->PixelFree( paletteData );

            this->paletteData = NULL;
        }
    }
};

// Private native texture API.
texNativeTypeProvider* GetNativeTextureTypeProvider( Interface *engineInterface, void *platformData );
uint32 GetNativeTextureMipmapCount( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider );

// Quick palette depth remapper.
void ConvertPaletteDepthEx(
    const void *srcTexels, void *dstTexels,
    uint32 srcTexelOffX, uint32 srcTexelOffY,
    uint32 dstTexelOffX, uint32 dstTexelOffY,
    uint32 texWidth, uint32 texHeight,
    uint32 texProcessWidth, uint32 texProcessHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
);

void ConvertPaletteDepth(
    const void *srcTexels, void *dstTexels,
    uint32 texWidth, uint32 texHeight,
    ePaletteType srcPaletteType, ePaletteType dstPaletteType,
    uint32 paletteSize,
    uint32 srcDepth, uint32 dstDepth,
    uint32 srcRowAlignment, uint32 dstRowAlignment
);

// Private pixel manipulation API.
void ConvertMipmapLayer(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType,
    bool forceAllocation,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

bool ConvertMipmapLayerNative(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 srcDataSize,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

bool ConvertMipmapLayerEx(
    Interface *engineInterface,
    const pixelDataTraversal::mipmapResource& mipLayer,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat dstRasterFormat, uint32 dstDepth, uint32 dstRowAlignment, eColorOrdering dstColorOrder, ePaletteType dstPaletteType, const void *dstPaletteData, uint32 dstPaletteSize, eCompressionType dstCompressionType,
    bool copyAnyway,
    uint32& dstPlaneWidthOut, uint32& dstPlaneHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
);

// Automatic optimal compression decision algorithm for RenderWare extensions.
bool DecideBestDXTCompressionFormat(
    Interface *engineInterface,
    bool srcHasAlpha,
    bool supportsDXT1, bool supportsDXT2, bool supportsDXT3, bool supportsDXT4, bool supportsDXT5,
    float quality,
    eCompressionType& dstCompressionTypeOut
);

// Palette conversion helper function.
void ConvertPaletteData(
    const void *srcPaletteTexels, void *dstPaletteTexels,
    uint32 srcPaletteSize, uint32 dstPaletteSize,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder, uint32 srcPalRasterDepth,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder, uint32 dstPalRasterDepth
);

bool ConvertPixelData( Interface *engineInterface, pixelDataTraversal& pixelsToConvert, const pixelFormat pixFormat );
bool ConvertPixelDataDeferred( Interface *engineInterface, const pixelDataTraversal& srcPixels, pixelDataTraversal& dstPixels, const pixelFormat pixFormat );

// Internal warning dispatcher class.
struct WarningHandler abstract
{
    virtual void OnWarningMessage( const std::string& theMessage ) = 0;
};

void GlobalPushWarningHandler( Interface *engineInterface, WarningHandler *theHandler );
void GlobalPopWarningHandler( Interface *engineInterface );

}

#pragma warning(push)
#pragma warning(disable: 4290)

// Global allocator
extern void* operator new( size_t memSize ) throw(std::bad_alloc);
extern void* operator new( size_t memSize, const std::nothrow_t nothrow ) throw();
extern void* operator new[]( size_t memSize ) throw(std::bad_alloc);
extern void* operator new[]( size_t memSize, const std::nothrow_t nothrow ) throw();
extern void operator delete( void *ptr ) throw();
extern void operator delete[]( void *ptr ) throw();

#pragma warning(pop)

#pragma warning(disable: 4996)

template <typename keyType, typename valueType>
struct uniqueMap_t
{
    struct pair
    {
        keyType key;
        valueType value;
    };

    typedef std::vector <pair> list_t;

    list_t pairs;

    inline valueType& operator [] ( const keyType& checkKey )
    {
        pair *targetIter = NULL;

        for ( list_t::iterator iter = pairs.begin(); iter != pairs.end(); iter++ )
        {
            if ( iter->key == checkKey )
            {
                targetIter = &*iter;
            }
        }

        if ( targetIter == NULL )
        {
            pair pair;
            pair.key = checkKey;

            pairs.push_back( pair );

            targetIter = &pairs.back();
        }

        return targetIter->value;
    }
};

AINLINE void setDataByDepth( void *dstArrayData, rw::uint32 depth, rw::uint32 targetArrayIndex, rw::uint32 value )
{
    using namespace rw;

    // Perform the texel set.
    if (depth == 4)
    {
        // Get the src item.
        PixelFormat::palette4bit::trav_t travItem = (PixelFormat::palette4bit::trav_t)value;

        // Put the dst item.
        PixelFormat::palette4bit *dstData = (PixelFormat::palette4bit*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 8)
    {
        // Get the src item.
        PixelFormat::palette8bit::trav_t travItem = (PixelFormat::palette8bit::trav_t)value;

        // Put the dst item.
        PixelFormat::palette8bit *dstData = (PixelFormat::palette8bit*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 16)
    {
        typedef PixelFormat::typedcolor <uint16> theColor;

        // Get the src item.
        theColor::trav_t travItem = (theColor::trav_t)value;

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 24)
    {
        struct colorStruct
        {
            inline colorStruct( uint32 val )
            {
                x = ( val & 0xFF );
                y = ( val & 0xFF00 ) << 8;
                z = ( val & 0xFF0000 ) << 16;
            }

            uint8 x, y, z;
        };

        typedef PixelFormat::typedcolor <colorStruct> theColor;

        // Get the src item.
        theColor::trav_t travItem = (theColor::trav_t)value;

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 32)
    {
        typedef PixelFormat::typedcolor <uint32> theColor;

        // Get the src item.
        theColor::trav_t travItem = (theColor::trav_t)value;

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else
    {
        throw RwException( "unknown bit depth for setting" );
    }
}

AINLINE void moveDataByDepth( void *dstArrayData, const void *srcArrayData, rw::uint32 depth, rw::uint32 targetArrayIndex, rw::uint32 srcArrayIndex )
{
    using namespace rw;

    // Perform the texel movement.
    if (depth == 4)
    {
        PixelFormat::palette4bit *srcData = (PixelFormat::palette4bit*)srcArrayData;

        // Get the src item.
        PixelFormat::palette4bit::trav_t travItem;

        srcData->getvalue(srcArrayIndex, travItem);

        // Put the dst item.
        PixelFormat::palette4bit *dstData = (PixelFormat::palette4bit*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 8)
    {
        // Get the src item.
        PixelFormat::palette8bit *srcData = (PixelFormat::palette8bit*)srcArrayData;

        PixelFormat::palette8bit::trav_t travItem;

        srcData->getvalue(srcArrayIndex, travItem);

        // Put the dst item.
        PixelFormat::palette8bit *dstData = (PixelFormat::palette8bit*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 16)
    {
        typedef PixelFormat::typedcolor <uint16> theColor;

        // Get the src item.
        theColor *srcData = (theColor*)srcArrayData;

        theColor::trav_t travItem;

        srcData->getvalue(srcArrayIndex, travItem);

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 24)
    {
        struct colorStruct
        {
            uint8 x, y, z;
        };

        typedef PixelFormat::typedcolor <colorStruct> theColor;

        // Get the src item.
        theColor *srcData = (theColor*)srcArrayData;

        theColor::trav_t travItem;

        srcData->getvalue(srcArrayIndex, travItem);

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else if (depth == 32)
    {
        typedef PixelFormat::typedcolor <uint32> theColor;

        // Get the src item.
        theColor *srcData = (theColor*)srcArrayData;

        theColor::trav_t travItem;

        srcData->getvalue(srcArrayIndex, travItem);

        // Put the dst item.
        theColor *dstData = (theColor*)dstArrayData;

        dstData->setvalue(targetArrayIndex, travItem);
    }
    else
    {
        throw RwException( "unknown bit depth for movement" );
    }
}