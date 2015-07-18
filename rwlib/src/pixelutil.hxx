// Utilities for working with mipmap and pixel data.

#include "pixelformat.hxx"

#include "txdread.d3d.dxt.hxx"

namespace rw
{

// Use this format to make a raster format compliant to a pixelCapabilities configuration.
// Returns whether the given raster format needs conversion into the new.
inline bool TransformDestinationRasterFormat(
    Interface *engineInterface,
    eRasterFormat srcRasterFormat, uint32 srcDepth, uint32 srcRowAlignment, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, void *srcPaletteData, uint32 srcPaletteSize, eCompressionType srcCompressionType,
    eRasterFormat& dstRasterFormatOut, uint32& dstDepthOut, uint32& dstRowAlignmentOut, eColorOrdering& dstColorOrderOut, ePaletteType& dstPaletteTypeOut, void*& dstPaletteDataOut, uint32& dstPaletteSizeOut, eCompressionType& dstCompressionTypeOut,
    const pixelCapabilities& pixelCaps, bool hasAlpha
)
{
    // Check whether those are correct.
    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    uint32 dstRowAlignment = srcRowAlignment;
    eColorOrdering dstColorOrder = srcColorOrder;
    ePaletteType dstPaletteType = srcPaletteType;
    void *dstPaletteData = srcPaletteData;
    uint32 dstPaletteSize = srcPaletteSize;
    eCompressionType dstCompressionType = srcCompressionType;

    bool hasBeenModded = false;

    if ( dstCompressionType == RWCOMPRESS_DXT1 && pixelCaps.supportsDXT1 == false ||
         dstCompressionType == RWCOMPRESS_DXT2 && pixelCaps.supportsDXT2 == false ||
         dstCompressionType == RWCOMPRESS_DXT3 && pixelCaps.supportsDXT3 == false ||
         dstCompressionType == RWCOMPRESS_DXT4 && pixelCaps.supportsDXT4 == false ||
         dstCompressionType == RWCOMPRESS_DXT5 && pixelCaps.supportsDXT5 == false )
    {
        // Set proper decompression parameters.
        uint32 dxtType;

        bool isDXT = IsDXTCompressionType( dstCompressionType, dxtType );

        assert( isDXT == true );

        eRasterFormat targetRasterFormat = getDXTDecompressionRasterFormat( engineInterface, dxtType, hasAlpha );

        dstRasterFormat = targetRasterFormat;
        dstDepth = Bitmap::getRasterFormatDepth( targetRasterFormat );
        dstRowAlignment = 4;    // for good measure.
        dstColorOrder = COLOR_BGRA;
        dstPaletteType = PALETTE_NONE;
        dstPaletteData = NULL;
        dstPaletteSize = 0;

        // We decompress stuff.
        dstCompressionType = RWCOMPRESS_NONE;

        hasBeenModded = true;
    }

    if ( hasBeenModded == false )
    {
        if ( dstPaletteType != PALETTE_NONE && pixelCaps.supportsPalette == false )
        {
            // We want to do things without a palette.
            dstPaletteType = PALETTE_NONE;
            dstPaletteSize = 0;
            dstPaletteData = NULL;

            dstDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

            hasBeenModded = true;
        }
    }

    // Check whether we even want an update.
    bool wantsUpdate = false;

    if ( srcRasterFormat != dstRasterFormat || dstDepth != srcDepth || dstColorOrder != srcColorOrder ||
         dstPaletteType != srcPaletteType || dstPaletteData != srcPaletteData || dstPaletteSize != srcPaletteSize ||
         dstCompressionType != srcCompressionType )
    {
        wantsUpdate = true;
    }

    // We give the result to the runtime.
    dstRasterFormatOut = dstRasterFormat;
    dstDepthOut = dstDepth;
    dstRowAlignmentOut = dstRowAlignment;
    dstColorOrderOut = dstColorOrder;
    dstPaletteTypeOut = dstPaletteType;
    dstPaletteDataOut = dstPaletteData;
    dstPaletteSizeOut = dstPaletteSize;
    dstCompressionTypeOut = dstCompressionType;

    return wantsUpdate;
}

}