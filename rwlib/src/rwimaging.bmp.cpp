#include "StdInc.h"

#include "rwimaging.hxx"

#include "pixelformat.hxx"

#include <PluginHelpers.h>

namespace rw
{

#ifdef RWLIB_INCLUDE_BMP_IMAGING
// .bmp file imaging extension.

#pragma pack(push,1)
struct bmpFileHeader
{
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
};

struct bmpInfoHeader
{
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
};
#pragma pack(pop)

struct bmpImagingEnv : public imagingFormatExtension
{
    inline void Initialize( Interface *engineInterface )
    {
        // Register ourselves.
        RegisterImagingFormat( engineInterface, "Raw Bitmap", "BMP", this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterImagingFormat( engineInterface, this );
    }

    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        int64 bmpStartOffset = inputStream->tell();

        // This is a pretty straight-forward system.
        bmpFileHeader header;

        size_t headerReadCount = inputStream->read( &header, sizeof( header ) );

        if ( headerReadCount != sizeof( header ) )
        {
            return false;
        }

        if ( header.bfType != 'MB' )
        {
            return false;
        }

        if ( header.bfReserved1 != 0 || header.bfReserved2 != 0 )
        {
            return false;
        }
        
        bmpInfoHeader infoHeader;

        size_t infoReadCount = inputStream->read( &infoHeader, sizeof( infoHeader ) );

        if ( infoReadCount != sizeof( infoHeader ) )
        {
            return false;
        }

        // Alright, now just the raster data has to be valid.
        inputStream->seek( bmpStartOffset + header.bfOffBits, eSeekMode::RWSEEK_BEG );

        uint32 imageDataSize = getRasterDataSize( infoHeader.biWidth * infoHeader.biHeight, infoHeader.biBitCount );

        skipAvailable( inputStream, imageDataSize );

        // I guess we should be a BMP file.
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

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const override
    {
        int64 bitmapStartOffset = inputStream->tell();

        // BMP files are a pretty simple format.
        bmpFileHeader header;

        size_t headerReadCount = inputStream->read( &header, sizeof( header ) );

        if ( headerReadCount != sizeof( header ) )
        {
            throw RwException( "could not read .bmp header" );
        }

        // We only know one bitmap type.
        if ( header.bfType != 'MB' )
        {
            throw RwException( "invalid checksum for .bmp" );
        }

        if ( header.bfReserved1 != 0 ||header.bfReserved2 != 0 )
        {
            throw RwException( "unknown bitmap extension; reserved not zero" );
        }

        // Now we read the info header.
        bmpInfoHeader infoHeader;

        size_t infoHeaderReadCount = inputStream->read( &infoHeader, sizeof( infoHeader ) );

        if ( infoHeaderReadCount != sizeof( infoHeader ) )
        {
            throw RwException( "could not read .bmp info header" );
        }

        if ( infoHeader.biSize != sizeof( infoHeader ) )
        {
            throw RwException( "unknown .bmp revision: invalid info header size" );
        }

        if ( infoHeader.biPlanes != 1 )
        {
            throw RwException( "invalid amount of .bmp planes (must be 1)" );
        }

        if ( infoHeader.biCompression != 0 )
        {
            throw RwException( "no support for compressed .bmp files (sorry)" );
        }

        // We ignore the size field.

        // Decide about the raster format we decode to.
        eRasterFormat rasterFormat;
        uint32 depth;

        ePaletteType paletteType = PALETTE_NONE;

        bool hasRasterFormat = false;

        uint32 itemDepth;

        if ( infoHeader.biBitCount == 4 || infoHeader.biBitCount == 8 )
        {
            rasterFormat = RASTER_8888;
            depth = 32;

            if ( infoHeader.biBitCount == 4 )
            {
                paletteType = PALETTE_4BIT;

                itemDepth = 4;
            }
            else if ( infoHeader.biBitCount == 8 )
            {
                paletteType = PALETTE_8BIT;

                itemDepth = 8;
            }

            hasRasterFormat = true;
        }
        else if ( infoHeader.biBitCount == 16 )
        {
            rasterFormat = RASTER_565;
            depth = 16;

            itemDepth = depth;

            hasRasterFormat = true;
        }
        else if ( infoHeader.biBitCount == 24 )
        {
            rasterFormat = RASTER_888;
            depth = 24;

            itemDepth = depth;
            
            hasRasterFormat = true;
        }
        else if ( infoHeader.biBitCount == 32 )
        {
            rasterFormat = RASTER_888;
            depth = 32;

            itemDepth = depth;

            hasRasterFormat = true;
        }

        if ( hasRasterFormat == false )
        {
            throw RwException( "unsupported .bmp raster format" );
        }

        // If we have a palette, read it.
        void *paletteData = NULL;
        uint32 paletteSize = 0;

        if ( paletteType != PALETTE_NONE )
        {
            paletteSize = infoHeader.biClrUsed;

            if ( paletteSize == 0 )
            {
                throw RwException( "malformed .bmp; invalid palette item count" );
            }

            uint32 paletteDataSize = getRasterDataSize( paletteSize, depth );

            checkAhead( inputStream, paletteDataSize );

            paletteData = engineInterface->PixelAllocate( paletteDataSize );

            try
            {
                size_t palReadCount = inputStream->read( paletteData, paletteDataSize );

                if ( palReadCount != paletteDataSize )
                {
                    throw RwException( "failed to read .bmp palette data" );
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( paletteData );

                throw;
            }
        }

        try
        {
            // We skip to the raster data to read it.
            inputStream->seek( bitmapStartOffset + header.bfOffBits, eSeekMode::RWSEEK_BEG );

            // Now read the actual image data.
            uint32 width = infoHeader.biWidth;
            uint32 height = infoHeader.biHeight;

            uint32 itemCount = ( width * height );

            uint32 imageDataSize = getRasterDataSize( itemCount, itemDepth );

            checkAhead( inputStream, imageDataSize );

            void *texelData = engineInterface->PixelAllocate( imageDataSize );

            try
            {
                size_t texelReadCount = inputStream->read( texelData, imageDataSize );

                if ( texelReadCount != imageDataSize )
                {
                    throw RwException( "failed to read .bmp image data" );
                }
            }
            catch( ... )
            {
                engineInterface->PixelFree( texelData );

                throw;
            }

            // Alright. We are successful.
            outputPixels.layerWidth = width;
            outputPixels.layerHeight = height;
            outputPixels.mipWidth = width;
            outputPixels.mipHeight = height;
            outputPixels.texelSource = texelData;
            outputPixels.dataSize = imageDataSize;

            outputPixels.rasterFormat = rasterFormat;
            outputPixels.depth = depth;
            outputPixels.colorOrder = COLOR_RGBA;
            outputPixels.paletteType = paletteType;
            outputPixels.paletteData = paletteData;
            outputPixels.paletteSize = paletteSize;
            outputPixels.compressionType = RWCOMPRESS_NONE;
        }
        catch( ... )
        {
            if ( paletteData != NULL )
            {
                engineInterface->PixelFree( paletteData );
                
                paletteData = NULL;
            }

            throw;
        }
    }
    
    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const override
    {
        // Calculate the file size.
        int64 actualFileSize = sizeof( bmpFileHeader );

        // After the file header is an info header.
        actualFileSize += sizeof( bmpInfoHeader );

        // Now we should have a palette, if we are palettized.
        ePaletteType srcPaletteType = inputPixels.paletteType;
        uint32 srcPaletteSize = inputPixels.paletteSize;

        uint32 srcDepth = inputPixels.depth;

        if ( srcPaletteType != PALETTE_NONE )
        {
            actualFileSize += getRasterDataSize( srcPaletteSize, srcDepth );
        }

        DWORD rasterOffBits = actualFileSize;

        // The last part is our image data.
        uint32 srcDataSize = inputPixels.dataSize;

        actualFileSize += srcDataSize;

        // Lets push some data onto the disk.
        bmpFileHeader header;
        header.bfType = 'MB';
        header.bfSize = actualFileSize;
        header.bfReserved1 = 0;
        header.bfReserved2 = 0;
        header.bfOffBits = rasterOffBits;
        
        outputStream->write( &header, sizeof( header ) );

        uint32 mipWidth = inputPixels.mipWidth;
        uint32 mipHeight = inputPixels.mipHeight;

        // Now write the info header.
        bmpInfoHeader infoHeader;
        infoHeader.biSize = sizeof( infoHeader );
        infoHeader.biWidth = mipWidth;
        infoHeader.biHeight = mipHeight;
        infoHeader.biPlanes = 1;

        // TODO.

        throw RwException( "cannot serialize .bmp yet" );
    }
};

static PluginDependantStructRegister <bmpImagingEnv, RwInterfaceFactory_t> bmpEnvRegister;

#endif //RWLIB_INCLUDE_BMP_IMAGING

void registerBMPImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_BMP_IMAGING
    bmpEnvRegister.RegisterPlugin( engineFactory );
#endif //RWLIB_INCLUDE_BMP_IMAGING
}

};