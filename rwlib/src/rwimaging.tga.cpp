#include "StdInc.h"

#include "rwimaging.hxx"

#include <PluginHelpers.h>

#include "pixelformat.hxx"

namespace rw
{

#ifdef RWLIB_INCLUDE_TGA_IMAGING
// .tga RenderWare imaging extension.
// this image format is natively implemented, so no additional .lib is required.

#pragma pack(push,1)
struct TgaHeader
{
    BYTE IDLength;        /* 00h  Size of Image ID field */
    BYTE ColorMapType;    /* 01h  Color map type */
    BYTE ImageType;       /* 02h  Image type code */
    WORD CMapStart;       /* 03h  Color map origin */
    WORD CMapLength;      /* 05h  Color map length */
    BYTE CMapDepth;       /* 07h  Depth of color map entries */
    WORD XOffset;         /* 08h  X origin of image */
    WORD YOffset;         /* 0Ah  Y origin of image */
    WORD Width;           /* 0Ch  Width of image */
    WORD Height;          /* 0Eh  Height of image */
    BYTE PixelDepth;      /* 10h  Image pixel size */

    struct
    {
        unsigned char numAttrBits : 4;
        unsigned char imageOrdering : 2;
        unsigned char reserved : 2;
    } ImageDescriptor; /* 11h  Image descriptor byte */
};
#pragma pack(pop)

static void writeTGAPixels(
    Interface *engineInterface,
    const void *texelSource, uint32 colorUnitCount,
    eRasterFormat srcRasterFormat, uint32 srcItemDepth, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcMaxPalette,
    eRasterFormat dstRasterFormat, uint32 dstItemDepth,
    eColorOrdering srcColorOrder,
    Stream *tgaStream
)
{
    // Check whether we need to create a TGA compatible color array.
    if ( srcRasterFormat != dstRasterFormat || srcItemDepth != dstItemDepth || srcPaletteType != PALETTE_NONE || srcColorOrder != COLOR_BGRA )
    {
        // Get the data size.
        uint32 texelDataSize = getRasterDataSize(colorUnitCount, dstItemDepth);

        // Allocate a buffer for the fixed pixel data.
        void *tgaColors = engineInterface->PixelAllocate( texelDataSize );

        for ( uint32 n = 0; n < colorUnitCount; n++ )
        {
            // Grab the color.
            uint8 red, green, blue, alpha;

            browsetexelcolor(texelSource, srcPaletteType, srcPaletteData, srcMaxPalette, n, srcRasterFormat, srcColorOrder, srcItemDepth, red, green, blue, alpha);

            // Write it with correct ordering.
            puttexelcolor(tgaColors, n, dstRasterFormat, COLOR_BGRA, dstItemDepth, red, green, blue, alpha);
        }

        tgaStream->write((const void*)tgaColors, texelDataSize);

        // Free memory.
        engineInterface->PixelFree( tgaColors );
    }
    else
    {
        // Simply write the color source.
        uint32 texelDataSize = getRasterDataSize(colorUnitCount, srcItemDepth);

        tgaStream->write((const void*)texelSource, texelDataSize);
    }
}

inline bool getTGARasterFormat( unsigned int pixelDepth, unsigned int pixelAlphaCount, eRasterFormat& dstRasterFormat, uint32& dstDepth )
{
    if ( pixelDepth == 32 )
    {
        if ( pixelAlphaCount == 8 )
        {
            dstRasterFormat = RASTER_8888;
            dstDepth = 32;
            return true;
        }
        else if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_888;
            dstDepth = 32;
            return true;
        }
    }
    else if ( pixelDepth == 24 )
    {
        if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_888;
            dstDepth = 24;
            return true;
        }
    }
    else if ( pixelDepth == 16 )
    {
        if ( pixelAlphaCount == 1 )
        {
            dstRasterFormat = RASTER_1555;
            dstDepth = 16;
            return true;
        }
        else if ( pixelAlphaCount == 0 )
        {
            dstRasterFormat = RASTER_565;
            dstDepth = 16;
            return true;
        }
        else if ( pixelAlphaCount == 4 )
        {
            dstRasterFormat = RASTER_4444;
            dstDepth = 16;
            return true;
        }
    }

    return false;
}

struct tgaImagingExtension : public imagingFormatExtension
{
    inline void Initialize( Interface *engineInterface )
    {
        // We can now address the imaging environment and register ourselves, quite exciting.
        RegisterImagingFormat( engineInterface, "Truevision Raster Graphics", "TGA", this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        // Remove us again.
        UnregisterImagingFormat( engineInterface, this );
    }

    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        // We do a quick structural check whether it is even possible that we are a TGA.
        TgaHeader possibleHeader;

        size_t readCount = inputStream->read( &possibleHeader, sizeof( possibleHeader ) );

        if ( readCount != sizeof( possibleHeader ) )
        {
            return false;
        }

        // Now skip the ID stuff, if present.
        if ( possibleHeader.IDLength != 0 )
        {
            skipAvailable( inputStream, possibleHeader.IDLength );
        }

        // Read the palette stuff.
        if ( possibleHeader.ColorMapType == 1 )
        {
            uint32 palDataSize = getRasterDataSize( possibleHeader.CMapLength, possibleHeader.CMapDepth );

            skipAvailable( inputStream, palDataSize );
        }

        // Now read the image data.
        uint32 colorDataSize = getRasterDataSize( possibleHeader.Width * possibleHeader.Height, possibleHeader.PixelDepth );

        skipAvailable( inputStream, colorDataSize );

        return true;
    }

    void GetStorageCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputTexels ) const override
    {
        // Read the header and decide about color format stuff.
        TgaHeader headerData;

        size_t headerReadCount = inputStream->read( &headerData, sizeof( headerData ) );

        if ( headerReadCount != sizeof( headerData ) )
        {
            throw RwException( "failed to read .tga header" );
        }

        // Check whether we even support this TGA.
        if ( headerData.ImageDescriptor.imageOrdering != 2 )
        {
            throw RwException( "invalid .tga image data ordering (expected top left)" );
        }

        // Decide about how to map it to RenderWare data.
        eRasterFormat dstRasterFormat;
        uint32 dstDepth;
        eColorOrdering dstColorOrder = COLOR_BGRA;  // TGA is always BGRA

        ePaletteType dstPaletteType = PALETTE_NONE;

        bool hasRasterFormat = false;

        uint32 dstItemDepth;

        if ( headerData.ColorMapType == 0 ) // no palette.
        {
            hasRasterFormat = getTGARasterFormat( headerData.PixelDepth, headerData.ImageDescriptor.numAttrBits, dstRasterFormat, dstDepth );

            if ( hasRasterFormat )
            {
                dstItemDepth = dstDepth;
            }
        }
        else if ( headerData.ColorMapType == 1 ) // has palette.
        {
            bool hasPixelRasterFormat = getTGARasterFormat( headerData.CMapDepth, headerData.ImageDescriptor.numAttrBits, dstRasterFormat, dstDepth );

            if ( hasPixelRasterFormat )
            {
                if ( headerData.PixelDepth == 4 || headerData.PixelDepth == 8 )
                {
                    dstItemDepth = headerData.PixelDepth;

                    hasRasterFormat = true;
                }
            }
        }

        if ( hasRasterFormat == false )
        {
            throw RwException( "unknown raster format mapping for .tga" );
        }

        // Skip the image id, if we have one.
        if ( headerData.IDLength != 0 )
        {
            inputStream->skip( headerData.IDLength );
        }

        // Read the palette, if present.
        void *paletteData = NULL;
        uint32 paletteSize = headerData.CMapLength;

        if ( headerData.ColorMapType == 1 )
        {
            uint32 paletteDataSize = getRasterDataSize( paletteSize, dstDepth );

            checkAhead( inputStream, paletteDataSize );

            paletteData = engineInterface->PixelAllocate( paletteDataSize );

            try
            {
                size_t readCount = inputStream->read( paletteData, paletteDataSize );

                if ( readCount != paletteDataSize )
                {
                    throw RwException( "failed to read .tga palette data" );
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( paletteData );

                paletteData = NULL;

                throw;
            }
        }

        try
        {
            // Now read the color/index data.
            uint32 width = headerData.Width;
            uint32 height = headerData.Height;

            uint32 itemCount = ( width * height );

            uint32 rasterDataSize = getRasterDataSize( itemCount, dstItemDepth );

            checkAhead( inputStream, rasterDataSize );

            void *texelData = engineInterface->PixelAllocate( rasterDataSize );

            try
            {
                size_t rasterReadCount = inputStream->read( texelData, rasterDataSize );

                if ( rasterReadCount != rasterDataSize )
                {
                    throw RwException( "failed to read .tga color/index data" );
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( texelData );

                throw;
            }

            // Nothing can go wrong now.
            outputTexels.layerWidth = width;
            outputTexels.layerHeight = height;
            outputTexels.mipWidth = width;
            outputTexels.mipHeight = height;
            outputTexels.texelSource = texelData;
            outputTexels.dataSize = rasterDataSize;
            
            outputTexels.rasterFormat = dstRasterFormat;
            outputTexels.depth = dstDepth;
            outputTexels.colorOrder = dstColorOrder;
            outputTexels.paletteType = dstPaletteType;
            outputTexels.paletteData = paletteData;
            outputTexels.paletteSize = paletteSize;
            outputTexels.compressionType = RWCOMPRESS_NONE;
        }
        catch( ... )
        {
            if ( paletteData )
            {
                engineInterface->PixelFree( paletteData );

                paletteData = NULL;
            }

            throw;
        }

        // We are done!
    }

    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputTexels ) const override
    {
        // We are using an extended version of the TGA standard that not a lot of editors support.

        if ( inputTexels.compressionType != RWCOMPRESS_NONE )
        {
            throw RwException( "cannot serialize TGA with compressed texels" );
        }

        // TODO: make this an Interface property.
        const bool optimized = true;

        // Decide how to write the raster.
        eRasterFormat srcRasterFormat = inputTexels.rasterFormat;
        ePaletteType srcPaletteType = inputTexels.paletteType;
        uint32 srcItemDepth = inputTexels.depth;

        eRasterFormat dstRasterFormat;
        uint32 dstColorDepth;
        uint32 dstAlphaBits;
        bool hasDstRasterFormat = false;

        ePaletteType dstPaletteType;

        if ( !optimized )
        {
            // We output in a format that every parser should understand.
            dstRasterFormat = RASTER_8888;
            dstColorDepth = 32;
            dstAlphaBits = 8;

            dstPaletteType = PALETTE_NONE;

            hasDstRasterFormat = true;
        }
        else
        {
            if ( srcRasterFormat == RASTER_1555 )
            {
                dstRasterFormat = RASTER_1555;
                dstColorDepth = 16;
                dstAlphaBits = 1;

                hasDstRasterFormat = true;
            }
            else if ( srcRasterFormat == RASTER_565 )
            {
                dstRasterFormat = RASTER_565;
                dstColorDepth = 16;
                dstAlphaBits = 0;

                hasDstRasterFormat = true;
            }
            else if ( srcRasterFormat == RASTER_4444 )
            {
                dstRasterFormat = RASTER_4444;
                dstColorDepth = 16;
                dstAlphaBits = 4;

                hasDstRasterFormat = true;
            }
            else if ( srcRasterFormat == RASTER_8888 ||
                      srcRasterFormat == RASTER_888 )
            {
                dstRasterFormat = RASTER_8888;
                dstColorDepth = 32;
                dstAlphaBits = 8;

                hasDstRasterFormat = true;
            }
            else if ( srcRasterFormat == RASTER_555 )
            {
                dstRasterFormat = RASTER_565;
                dstColorDepth = 16;
                dstAlphaBits = 0;

                hasDstRasterFormat = true;
            }

            // We palettize if present.
            dstPaletteType = srcPaletteType;
        }

        if ( !hasDstRasterFormat )
        {
            throw RwException( "could not find a raster format to write TGA image with" );
        }
        else
        {
            // Prepare the TGA header.
            TgaHeader header;

            uint32 maxpalette = inputTexels.paletteSize;

            bool isPalette = (dstPaletteType != PALETTE_NONE);

            header.IDLength = 0;
            header.ColorMapType = ( isPalette ? 1 : 0 );
            header.ImageType = ( isPalette ? 1 : 2 );

            // The pixel depth is the number of bits a color entry is going to take (real RGBA color).
            uint32 pixelDepth = 0;

            if (isPalette)
            {
                pixelDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);
            }
            else
            {
                pixelDepth = dstColorDepth;
            }
        
            header.CMapStart = 0;
            header.CMapLength = ( isPalette ? maxpalette : 0 );
            header.CMapDepth = ( isPalette ? pixelDepth : 0 );

            header.XOffset = 0;
            header.YOffset = 0;

            uint32 width = inputTexels.mipWidth;
            uint32 height = inputTexels.mipHeight;

            header.Width = width;
            header.Height = height;
            header.PixelDepth = ( isPalette ? srcItemDepth : dstColorDepth );

            header.ImageDescriptor.numAttrBits = dstAlphaBits;
            header.ImageDescriptor.imageOrdering = 2;
            header.ImageDescriptor.reserved = 0;

            // Write the header.
            outputStream->write((const void*)&header, sizeof(header));

            const void *texelSource = inputTexels.texelSource;
            const void *paletteData = inputTexels.paletteData;
            eColorOrdering colorOrder = inputTexels.colorOrder;

            // Write the palette if we require.
            if (isPalette)
            {
                writeTGAPixels(
                    engineInterface,
                    paletteData, maxpalette,
                    srcRasterFormat, pixelDepth, PALETTE_NONE, NULL, 0,
                    dstRasterFormat, pixelDepth,
                    colorOrder, outputStream
                );
            }

            // Now write image information.
            // If we are a palette, we simply write the color indice.
            uint32 colorUnitCount = ( width * height );

            if (isPalette)
            {
                assert( srcPaletteType != PALETTE_NONE );

                // Write a fixed version of the palette indice.
                uint32 texelDataSize = inputTexels.dataSize; // for palette items its the same size.

                void *fixedPalItems = engineInterface->PixelAllocate( texelDataSize );

                ConvertPaletteDepth(
                    texelSource, fixedPalItems,
                    colorUnitCount,
                    srcPaletteType, maxpalette,
                    srcItemDepth, srcItemDepth
                );

                outputStream->write((const void*)fixedPalItems, texelDataSize);

                // Clean up memory.
                engineInterface->PixelFree( fixedPalItems );
            }
            else
            {
                writeTGAPixels(
                    engineInterface,
                    texelSource, colorUnitCount,
                    srcRasterFormat, srcItemDepth, srcPaletteType, paletteData, maxpalette,
                    dstRasterFormat, dstColorDepth,
                    colorOrder, outputStream
                );
            }
        }
    }
};

static PluginDependantStructRegister <tgaImagingExtension, RwInterfaceFactory_t> tgaImagingEnv;

#endif //RWLIB_INCLUDE_TGA_IMAGING

void registerTGAImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_TGA_IMAGING
    tgaImagingEnv.RegisterPlugin( engineFactory );
#endif //RWLIB_INCLUDE_TGA_IMAGING
}

}