#include "StdInc.h"

#include "rwimaging.hxx"

#include <PluginHelpers.h>

#include "pixelformat.hxx"

#include "streamutil.hxx"

#ifdef RWLIB_INCLUDE_PNG_IMAGING
#include <png.h>
#endif //RWLIB_INCLUDE_PNG_IMAGING

namespace rw
{

#ifdef RWLIB_INCLUDE_PNG_IMAGING
// PNG RenderWare image format extension.

inline uint32 getPNGTexelDataRowAlignment( void )
{
    return 1;
}

inline uint32 getPNGRasterDataRowSize( uint32 width, uint32 depth )
{
    return getRasterDataRowSize( width, depth, getPNGTexelDataRowAlignment() );
}

static const imaging_filename_ext png_ext[] =
{
    { "PNG", true }
};

struct pngImagingExtension : public imagingFormatExtension
{
    struct png_chunk_header
    {
        endian::big_endian <uint32> length;
        endian::big_endian <uint32> type;
    };

    struct png_chunk_footer
    {
        endian::big_endian <uint32> crc32;
    };

    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        // Check that it really is a PNG file.
        // Note that we have to check by structure, not by signature only.
        unsigned char sigbytes[8];

        size_t sigbytesRead = inputStream->read( sigbytes, sizeof( sigbytes ) );

        if ( sigbytesRead != sizeof( sigbytes ) )
        {
            return false;
        }

        // Check signature.
        bool signatureCorrect = ( png_sig_cmp( sigbytes, 0, sizeof( sigbytes ) ) == 0 );

        if ( !signatureCorrect )
        {
            return false;
        }

        // Verify the PNG structure.
        while ( true )
        {
            png_chunk_header header;

            size_t chunkHeaderReadCount = inputStream->read( &header, sizeof( header ) );

            if ( chunkHeaderReadCount == 0 )
            {
                // This happens when we reach the end of the file.
                break;
            }
            else if ( chunkHeaderReadCount != sizeof( header ) ) 
            {
                return false;
            }

            // We need to read by the amount of the chunk length.
            uint32 chunkLength = header.length;

            // So lets just skip those bytes, assuming they are available.
            skipAvailable( inputStream, chunkLength );

            // Now let us read the chunk end.
            // We do not need to verify that the CRC is correct.
            // But we could add that in the future to add absolute certainty!
            png_chunk_footer footer;

            size_t chunkFooterReadCount = inputStream->read( &footer, sizeof( footer ) );

            if ( chunkFooterReadCount != sizeof( footer ) )
            {
                return false;
            }

            // I guess that chunk is alright. Try the next one.
        }

        // The PNG file is an endless stream of chunks.
        // So if there are no more chunks at the end, then the file is done.

        return true;
    }

    void GetStorageCapabilities( pixelCapabilities& storageCaps ) const override
    {
        storageCaps.supportsDXT1 = false;
        storageCaps.supportsDXT2 = false;
        storageCaps.supportsDXT3 = false;
        storageCaps.supportsDXT4 = false;
        storageCaps.supportsDXT5 = false;
        storageCaps.supportsPalette = true;
    }

    struct png_meta_info
    {
        Interface *engineInterface;
    };

    struct png_stream_info : public png_meta_info
    {
        Stream *usedStream;
    };

    static void png_error_routine( png_structp png_info, png_const_charp message )
    {
        throw RwException( "PNG error: " + std::string( message ) );
    }

    static void png_warning_routine( png_structp png_info, png_const_charp message )
    {
        png_meta_info *meta_info = (png_meta_info*)png_get_error_ptr( png_info );

        meta_info->engineInterface->PushWarning( "PNG warning: " + std::string( message ) );
    }

    static void* png_malloc_routine( png_structp png_info, png_size_t memsize )
    {
        png_meta_info *meta_info = (png_meta_info*)png_get_mem_ptr( png_info );

        return meta_info->engineInterface->MemAllocate( memsize );
    }

    static void png_memfree_routine( png_structp png_info, png_voidp memptr )
    {
        png_meta_info *meta_info = (png_meta_info*)png_get_mem_ptr( png_info );

        meta_info->engineInterface->MemFree( memptr );
    }

    static void png_read_routine( png_structp png_info, png_bytep outBuff, png_size_t readCount )
    {
        png_stream_info *streamInfo = (png_stream_info*)png_get_io_ptr( png_info );

        size_t actualReadCount = streamInfo->usedStream->read( outBuff, readCount );

        if ( actualReadCount != readCount )
        {
            throw RwException( "PNG error: unfinished read exception" );
        }
    }

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const override
    {
        // We use this meta information to interface back with the RW core.
        png_stream_info meta_info;
        meta_info.engineInterface = engineInterface;
        meta_info.usedStream = inputStream;

        // Initialize our read structure.
        png_structp read_info = png_create_read_struct_2(
            PNG_LIBPNG_VER_STRING, &meta_info, png_error_routine, png_warning_routine,
            &meta_info, png_malloc_routine, png_memfree_routine
        );

        if ( read_info == NULL )
        {
            throw RwException( "failed to allocate PNG read struct" );
        }

        try
        {
            // Now initialize our info struct.
            png_infop img_info = png_create_info_struct( read_info );

            if ( img_info == NULL )
            {
                throw RwException( "failed to allocate PNG info struct" );
            }

            try
            {
                // Prepare the PNG environment for reading.
                png_set_read_fn( read_info, &meta_info, png_read_routine );

                png_read_info( read_info, img_info );

                // Read image meta information.
                png_uint_32 width, height;
                int png_depth, color_type, interlace_method,
                    compression_method, filter_method;

                png_uint_32 ihdrResult = png_get_IHDR(
                    read_info, img_info, &width, &height, &png_depth, &color_type,
                    &interlace_method, &compression_method, &filter_method
                );

                // We must have image information.
                if ( ihdrResult == 0 )
                {
                    throw RwException( "PNG error: missing image meta information" );
                }
                
                if ( color_type & ~0x07 )
                {
                    throw RwException( "PNG error: unknown color type" );
                }

                // Determine how we can map this PNG to RW original types.
                eRasterFormat rasterFormat;
                uint32 depth;
                eColorOrdering colorOrder = COLOR_RGBA;

                ePaletteType paletteType = PALETTE_NONE;
                uint32 paletteSize = 0;

                uint32 itemDepth;

                bool directAcquireByPixelFormat = false;

                if ( color_type == 0 )
                {
                    if ( png_depth == 1 || png_depth == 2 || png_depth == 4 || png_depth == 8 || png_depth == 16 )
                    {
                        // We are a grayscale image without palette and no alpha.
                        rasterFormat = RASTER_LUM;  // obviously 8bit LUM.
                        depth = 8;
                        itemDepth = 8;

                        // We always directly acquire this.
                        directAcquireByPixelFormat = true;

                        if ( png_depth < 8 )
                        {
                            // Expand to full depth, please.
                            png_set_expand_gray_1_2_4_to_8( read_info );
                        }
                        else if ( png_depth == 16 )
                        {
                            // Warn the user of lossy PNG conversion.
                            engineInterface->PushWarning( "lossy 16bit grayscale conversion to 8bit LUM8" );

                            png_set_scale_16( read_info );
                        }
                    }
                    else
                    {
                        throw RwException( "unknown .png grayscale format" );
                    }
                }
                else if ( color_type == 2 )
                {
                    if ( png_depth == 8 || png_depth == 16 )
                    {
                        // We are a color image without palette and no alpha.
                        rasterFormat = RASTER_888;
                        depth = 24;
                        itemDepth = 24;

                        // Always directly acquire.
                        directAcquireByPixelFormat = true;

                        if ( png_depth == 16 )
                        {
                            engineInterface->PushWarning( "lossy 16bit color channel to 8bit color channel conversion" );

                            png_set_scale_16( read_info );
                        }
                    }
                    else
                    {
                        throw RwException( "unknown .png truecolor format" );
                    }
                }
                else if ( color_type == 3 )
                {
                    if ( png_depth == 4 || png_depth == 8 )
                    {
                        // We are a color image with palette.
                        // There can but there does not have to be alpha included.
                        rasterFormat = RASTER_888;
                        depth = 32;

                        // We can just take over the row as is.
                        itemDepth = png_depth;

                        if ( png_depth == 4 )
                        {
                            // Make sure we order the 4bit chunks properly.
                            png_set_packswap( read_info );

                            paletteType = PALETTE_4BIT;
                        }
                        else if ( png_depth == 8 )
                        {
                            paletteType = PALETTE_8BIT;
                        }
                        else
                        {
                            assert( 0 );
                        }

                        paletteSize = (uint32)std::pow( 2, png_depth );

                        // This format can also be directly acquired.
                        directAcquireByPixelFormat = true;
                    }
                    else
                    {
                        throw RwException( "unknown .png palette depth format" );
                    }
                }
                else if ( color_type == 4 )
                {
                    if ( png_depth == 8 || png_depth == 16 )
                    {
                        // We are luminance with alpha.
                        rasterFormat = RASTER_LUM_ALPHA;
                        depth = 16;
                        itemDepth = 16;

                        directAcquireByPixelFormat = true;

                        if ( png_depth == 16 )
                        {
                            engineInterface->PushWarning( "lossy 16bit luminance alpha to 8bit conversion" );

                            png_set_scale_16( read_info );
                        }
                    }
                    else
                    {
                        throw RwException( "unknown .png luminance alpha format" );
                    }
                }
                else if ( color_type == 6 )
                {
                    if ( png_depth == 8 || png_depth == 16 )
                    {
                        // We are a color image without palette but with alpha channel.
                        rasterFormat = RASTER_8888;
                        depth = 32;
                        itemDepth = 32;

                        // Direct acquisition ftw.
                        directAcquireByPixelFormat = true;

                        if ( png_depth == 16 )
                        {
                            engineInterface->PushWarning( "lossy 16bit truecolor+alpha to 8bit conversion" );

                            png_set_scale_16( read_info );
                        }
                    }
                    else
                    {
                        throw RwException( "unknown .png truecolor+alpha format" );
                    }
                }
                else
                {
                    throw RwException( "PNG error: unknown color type" );
                }

                // TODO: allow conversion to supported format later, when we get better at stuff.
                if ( directAcquireByPixelFormat == false )
                {
                    throw RwException( "fatal error: could not apply pixels from .png directly" );
                }

                // If we are a format with palette, get the palette chunk.
                // It must be there.
                void *paletteData = NULL;

                if ( paletteType != PALETTE_NONE )
                {
                    int pngPalCount = 0;
                    png_colorp pngPaletteColors = NULL;

                    // Get the palette chunk and do things with it.
                    png_uint_32 palRes = png_get_PLTE( read_info, img_info, &pngPaletteColors, &pngPalCount );

                    if ( palRes == 0 )
                    {
                        throw RwException( "could not find PLTE chunk in .png" );
                    }

                    // Check whether we have an alpha channel.
                    png_bytep alphaValues = NULL;
                    int numAlphaValues = 0;

                    png_uint_32 alphaChunkExists = png_get_tRNS( read_info, img_info, &alphaValues, &numAlphaValues, NULL );

                    if ( alphaChunkExists != 0 )
                    {
                        // We want to store things as RASTER_8888 instead.
                        rasterFormat = RASTER_8888;
                        depth = 32;
                    }

                    // Transform the PNG palette spec into a palette we understand.
                    size_t paletteDataSize = getPaletteDataSize( paletteSize, depth );

                    paletteData = engineInterface->PixelAllocate( paletteDataSize );

                    try
                    {
                        colorModelDispatcher <void> putDispatch( rasterFormat, colorOrder, depth, NULL, 0, PALETTE_NONE );

                        // Transform!
                        for ( uint32 n = 0; n < paletteSize; n++ )
                        {
                            uint8 r = 0;
                            uint8 g = 0;
                            uint8 b = 0;
                            uint8 a = 255;

                            // Get the RGB components.
                            if ( n < (uint32)pngPalCount )
                            {
                                png_colorp pngCurColor = pngPaletteColors + n;

                                r = pngCurColor->red;
                                g = pngCurColor->green;
                                b = pngCurColor->blue;
                            }

                            // Get the alpha component, if present.
                            if ( alphaChunkExists != 0 && n < (uint32)numAlphaValues )
                            {
                                a = alphaValues[ n ];
                            }

                            // Store this color.
                            putDispatch.setRGBA( paletteData, n, r, g, b, a );
                        }
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( paletteData );

                        throw;
                    }
                }

                // Update parameters.
                png_read_update_info( read_info, img_info );

                try
                {
                    // After we have set up everything, we can start the reading.
                    // For that we first allocate a big buffer with row pointers.
                    // We do not want to handle interlacing ourselves.
                    png_bytepp row_pointers =
                        (png_bytepp)engineInterface->MemAllocate( sizeof( png_bytep ) * height );

                    if ( row_pointers == NULL )
                    {
                        throw RwException( "failed to allocate large enough buffer for PNG row pointers" );
                    }

                    try
                    {
                        // Calculate the size we need for a pixel buffer.
                        size_t rowLength = png_get_rowbytes( read_info, img_info );

                        uint32 dstRowSize = getPNGRasterDataRowSize( width, itemDepth );

                        assert( rowLength == dstRowSize );

                        uint32 texelBufferSize = (uint32)( rowLength * height );

                        // Allocate a pixel buffer. We may want to reuse this buffer directly if we can.
                        void *texelBuffer = engineInterface->PixelAllocate( texelBufferSize );

                        if ( texelBuffer == NULL )
                        {
                            throw RwException( "failed to allocate large enough memory buffer for PNG texel data" );
                        }

                        // Set the row pointers, so that they point into the texel buffer properly.
                        {
                            size_t texelBufferOffset = 0;

                            for ( uint32 n = 0; n < height; n++ )
                            {
                                row_pointers[ n ] = (png_bytep)texelBuffer + texelBufferOffset;

                                texelBufferOffset += rowLength;
                            }
                        }

                        // We made sure that the resulting buffer is always a contiguous array of pixels
                        // So there is no problem of directly acquiring it.

                        try
                        {
                            // Alright, we can read the image.
                            png_read_image( read_info, row_pointers );

                            // Lets handle this thing now!
                            {
                                outputPixels.layerWidth = width;
                                outputPixels.layerHeight = height;
                                outputPixels.mipWidth = width;
                                outputPixels.mipHeight = height;
                                outputPixels.texelSource = texelBuffer;
                                outputPixels.dataSize = texelBufferSize;
                                outputPixels.rasterFormat = rasterFormat;
                                outputPixels.depth = itemDepth;
                                outputPixels.rowAlignment = getPNGTexelDataRowAlignment();
                                outputPixels.colorOrder = colorOrder;
                                outputPixels.paletteType = paletteType;
                                outputPixels.paletteData = paletteData;
                                outputPixels.paletteSize = paletteSize;
                                outputPixels.compressionType = RWCOMPRESS_NONE;

                                // We took the palette.
                                paletteData = NULL;

                                // We took the texel buffer.
                                texelBuffer = NULL;
                            }

                            // Read some shit at the end.
                            // Example says it is required..?
                            png_read_end( read_info, img_info );
                        }
                        catch( ... )
                        {
                            if ( texelBuffer != NULL )
                            {
                                engineInterface->PixelFree( texelBuffer );

                                texelBuffer = NULL;
                            }

                            throw;
                        }

                        // If we do not directly acquire, we do not need the texel buffer anymore.
                        if ( texelBuffer != NULL )
                        {
                            engineInterface->PixelFree( texelBuffer );

                            texelBuffer = NULL;
                        }
                    }
                    catch( ... )
                    {
                        engineInterface->MemFree( row_pointers );

                        throw;
                    }

                    // Free the buffer again.
                    engineInterface->MemFree( row_pointers );
                }
                catch( ... )
                {
                    // If we have not taken the palette data yet, we free it.
                    if ( paletteData != NULL )
                    {
                        engineInterface->PixelFree( paletteData );

                        paletteData = NULL;
                    }

                    throw;
                }

                // Clean up palette if we did not end up taking it.
                if ( paletteData != NULL )
                {
                    engineInterface->PixelFree( paletteData );

                    paletteData = NULL;
                }
            }
            catch( ... )
            {
                // An error happened, so destroy ourselves.
                png_destroy_info_struct( read_info, &img_info );

                throw;
            }

            // Free the info struct.
            png_destroy_info_struct( read_info, &img_info );
        }
        catch( ... )
        {
            // Basically, we failed, so free resources.
            png_destroy_read_struct( &read_info, NULL, NULL );

            throw;
        }

        // Free the main struct.
        png_destroy_read_struct( &read_info, NULL, NULL );

        // Done!
    }

    static void png_write_routine( png_structp write_info, png_bytep buffer, png_size_t count )
    {
        png_stream_info *stream_info = (png_stream_info*)png_get_io_ptr( write_info );

        size_t realWriteCount = stream_info->usedStream->write( buffer, count );

        if ( realWriteCount != count )
        {
            throw RwException( "PNG error: unfinished write exception" );
        }
    }

    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const override
    {
        if ( inputPixels.compressionType != RWCOMPRESS_NONE )
        {
            throw RwException( "cannot serialize .png using compressed data" );
        }

        png_stream_info meta_info;
        meta_info.engineInterface = engineInterface;
        meta_info.usedStream = outputStream;

        // We want to write this PNG while preserving the given pixel format as much as possible.
        png_structp write_info = png_create_write_struct_2(
            PNG_LIBPNG_VER_STRING, &meta_info, png_error_routine, png_warning_routine,
            &meta_info, png_malloc_routine, png_memfree_routine
        );

        if ( write_info == NULL )
        {
            throw RwException( "failed to allocate .png write struct" );
        }

        try
        {
            png_infop img_info = png_create_info_struct( write_info );

            if ( img_info == NULL )
            {
                throw RwException( "failed to create .png info struct for writing" );
            }

            try
            {
                // Setup the writing process.
                png_set_write_fn( write_info, &meta_info, png_write_routine, NULL );

                // Set us up the bomb.
                uint32 mipWidth = inputPixels.mipWidth;
                uint32 mipHeight = inputPixels.mipHeight;

                eRasterFormat rasterFormat = inputPixels.rasterFormat;
                uint32 depth = inputPixels.depth;
                uint32 rowAlignment = inputPixels.rowAlignment;
                eColorOrdering colorOrder = inputPixels.colorOrder;
                ePaletteType paletteType = inputPixels.paletteType;
                const void *paletteData = inputPixels.paletteData;
                uint32 paletteSize = inputPixels.paletteSize;

                const void *texelSource = inputPixels.texelSource;
                uint32 texelDataSize = inputPixels.dataSize;

                // We will need this to fetch pixels.
                colorModelDispatcher <const void> fetchDispatch( rasterFormat, colorOrder, depth, paletteData, paletteSize, paletteType );

                eColorModel colorModel = fetchDispatch.getColorModel();

                // Determine how to map the RW original types to PNG types.
                int png_depth, color_type, interlace_method, compression_method, filter_method;

                // Basic things.
                interlace_method = PNG_INTERLACE_NONE;  // we never interlace things.
                compression_method = PNG_COMPRESSION_TYPE_DEFAULT;  // we dont care.
                filter_method = PNG_FILTER_TYPE_BASE;   // no filtering pls.

                bool canRasterHaveAlpha = canRasterFormatHaveAlpha( rasterFormat );

                bool isPalette = ( paletteType != PALETTE_NONE );

                eRasterFormat wantedRasterFormat;
                uint32 wantedItemDepth;
                eColorOrdering wantedColorOrder = COLOR_RGBA;

                uint32 wantedDepth;

                if ( isPalette )
                {
                    if ( colorModel == COLORMODEL_RGBA || colorModel == COLORMODEL_LUMINANCE )
                    {
                        // We have to write a palette PNG.
                        png_depth = depth;
                        color_type = 3;

                        wantedRasterFormat = RASTER_888;
                        wantedItemDepth = depth;
                        wantedDepth = 24;
                    }
                    else
                    {
                        throw RwException( "failed to write palette .png with unknown color model" );
                    }
                }
                else
                {
                    if ( colorModel == COLORMODEL_RGBA )
                    {
                        png_depth = 8;

                        if ( canRasterHaveAlpha )
                        {
                            color_type = 6;

                            wantedRasterFormat = RASTER_8888;
                            wantedItemDepth = 32;
                            wantedDepth = 32;
                        }
                        else
                        {
                            color_type = 2;

                            wantedRasterFormat = RASTER_888;
                            wantedItemDepth = 24;
                            wantedDepth = 24;
                        }
                    }
                    else if ( colorModel == COLORMODEL_LUMINANCE )
                    {
                        // We always write 8bit grayscale samples.
                        png_depth = 8;

                        if ( canRasterHaveAlpha )
                        {
                            color_type = 4;     // LUM with ALPHA

                            wantedRasterFormat = RASTER_LUM_ALPHA;
                            wantedItemDepth = 16;
                            wantedDepth = 16;
                        }
                        else
                        {
                            color_type = 0;     // just LUM

                            wantedRasterFormat = RASTER_LUM;
                            wantedItemDepth = 8;
                            wantedDepth = 8;
                        }
                    }
                    else
                    {
                        throw RwException( "cannot serialize .png with an unknown color model" );
                    }
                }

                // Cache the row size.
                size_t rowSizeSrc = getRasterDataRowSize( mipWidth, depth, rowAlignment );

                // Check whether we need transformation of pixels before writing.
                // If we do not, then writing can happen very fast.
                bool isAlreadyTransformed = true;

                if ( paletteType == PALETTE_NONE )
                {
                    isAlreadyTransformed = ( rasterFormat == wantedRasterFormat && depth == wantedItemDepth && colorOrder == wantedColorOrder );
                }

                // Store that information into the PNG stream.
                png_set_IHDR(
                    write_info, img_info, mipWidth, mipHeight,
                    png_depth, color_type, interlace_method, compression_method,
                    filter_method
                );

                // If we are a palette image, lets save palette information.
                void *pngPaletteData = NULL;
                void *pngAlphaValues = NULL;

                if ( isPalette )
                {
                    // Calculate a subset of the palette that we need, anyway.
                    uint32 reqPalSubset = std::min( paletteSize, getPaletteItemCount( paletteType ) );

                    uint32 palRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

                    eRasterFormat requiredPalRasterFormat = wantedRasterFormat;
                    uint32 requiredPalDepth = wantedDepth;
                    eColorOrdering requiredPalColorOrder = wantedColorOrder;

                    // We need to create a palette that fits into PNG.
                    size_t palDataSize = getPaletteDataSize( reqPalSubset, requiredPalDepth );

                    void *pngPaletteDataMutable = engineInterface->PixelAllocate( palDataSize );

                    pngPaletteData = pngPaletteDataMutable;
                    
                    try
                    {
                        colorModelDispatcher <void> putPalDispatch(
                            requiredPalRasterFormat, requiredPalColorOrder, requiredPalDepth,
                            NULL, 0, PALETTE_NONE
                        );

                        colorModelDispatcher <const void> fetchPalDispatch(
                            rasterFormat, colorOrder, palRasterDepth,
                            NULL, 0, PALETTE_NONE
                        );

                        for ( uint32 n = 0; n < reqPalSubset; n++ )
                        {
                            abstractColorItem color;

                            fetchPalDispatch.getColor( paletteData, n, color );

                            putPalDispatch.setColor( pngPaletteDataMutable, n, color );
                        }

                        png_set_PLTE( write_info, img_info, (png_const_colorp)pngPaletteData, reqPalSubset );

                        // If we have alpha information, save it aswell in another chunk.
                        if ( canRasterHaveAlpha )
                        {
                            pngAlphaValues = engineInterface->PixelAllocate( reqPalSubset * sizeof( uint8 ) );

                            try
                            {
                                for ( uint32 n = 0; n < reqPalSubset; n++ )
                                {
                                    uint8 r, g, b, a;

                                    fetchPalDispatch.getRGBA( paletteData, n, r, g, b, a );

                                    // Store it.
                                    unsigned char *alphaValue = (unsigned char*)pngAlphaValues + n;

                                    *alphaValue = a;
                                }

                                // Give it to the PNG writer.
                                png_set_tRNS( write_info, img_info, (png_const_bytep)pngAlphaValues, reqPalSubset, NULL );
                            }
                            catch( ... )
                            {
                                engineInterface->PixelFree( pngAlphaValues );
                                
                                throw;
                            }
                        }
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( pngPaletteDataMutable );

                        throw;
                    }
                }

                try
                {
                    // Write some amazing meta-data text!
                    {
                        png_text meta_text[1];

                        png_text& firstText = meta_text[0];
                        firstText.key = "Software";
                        firstText.text = "RenderWare";
                        firstText.compression = PNG_TEXT_COMPRESSION_NONE;
                        firstText.itxt_length = 0;
                        firstText.lang = NULL;
                        firstText.lang_key = NULL;

                        png_set_text( write_info, img_info, meta_text, sizeof( meta_text ) / sizeof( *meta_text ) );
                    }

                    // Now that everything is set up properly... write it.
                    png_write_info( write_info, img_info );

                    // Make sure we swap pixels if they are packed.
                    png_set_packswap( write_info );

                    // Alright, lets start writing the pixels.
                    if ( isAlreadyTransformed )
                    {
                        // Optimized write.
                        // We do not want to allocate row pointers, because we will never interlace the image.
                        const void *current_row_pointer = texelSource;

                        for ( uint32 row = 0; row < mipHeight; row++ )
                        {
                            // Write it.
                            png_write_row( write_info, (png_const_bytep)current_row_pointer );

                            // Increment the row pointer.
                            current_row_pointer = (const char*)current_row_pointer + rowSizeSrc;
                        }
                    }
                    else
                    {
                        // We need to allocate a row where we transform pixels to.
                        size_t rowSizeTransformed = getPNGRasterDataRowSize( mipWidth, wantedItemDepth );

                        // We have to extract special row-bits and write them.
                        png_voidp alloc_row = (png_voidp)engineInterface->PixelAllocate( rowSizeTransformed );

                        if ( alloc_row == NULL )
                        {
                            throw RwException( "failed to allocate special row buffer for .png writing" );
                        }

                        try
                        {
                            for ( uint32 row = 0; row < mipHeight; row++ )
                            {
                                // Call the generic transformation routine.
                                moveTexels(
                                    texelSource, alloc_row,
                                    0, row,
                                    0, 0,
                                    mipWidth, 1,
                                    mipWidth, mipHeight,
                                    rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteSize,
                                    wantedRasterFormat, wantedItemDepth, getPNGTexelDataRowAlignment(), wantedColorOrder, paletteType, paletteSize
                                );

                                // Write our row.
                                png_write_row( write_info, (png_const_bytep)alloc_row );
                            }
                        }
                        catch( ... )
                        {
                            // Free the row again.
                            engineInterface->PixelFree( alloc_row );

                            throw;
                        }

                        engineInterface->PixelFree( alloc_row );
                    }

                    // Write the end of the PNG.
                    png_write_end( write_info, img_info );
                }
                catch( ... )
                {
                    // Free palette information, if there.
                    if ( pngPaletteData )
                    {
                        engineInterface->PixelFree( pngPaletteData );

                        pngPaletteData = NULL;
                    }

                    if ( pngAlphaValues )
                    {
                        engineInterface->PixelFree( pngAlphaValues );

                        pngAlphaValues = NULL;
                    }

                    throw;
                }

                // Clean up the temporary palette stuff.
                if ( pngPaletteData )
                {
                    engineInterface->PixelFree( pngPaletteData );
                }

                if ( pngAlphaValues )
                {
                    engineInterface->PixelFree( pngAlphaValues );
                }
            }
            catch( ... )
            {
                png_destroy_info_struct( write_info, &img_info );

                throw;
            }

            // Clean up.
            png_destroy_info_struct( write_info, &img_info );
        }
        catch( ... )
        {
            png_destroy_write_struct( &write_info, NULL );

            throw;
        }

        // Clean up memory.
        png_destroy_write_struct( &write_info, NULL );
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterImagingFormat( engineInterface, "Portable Network Graphics", IMAGING_COUNT_EXT(png_ext), png_ext, this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterImagingFormat( engineInterface, this );
    }
};

static PluginDependantStructRegister <pngImagingExtension, RwInterfaceFactory_t> pngImagingStore;

#endif //RWLIB_INCLUDE_PNG_IMAGING

void registerPNGImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_PNG_IMAGING
    pngImagingStore.RegisterPlugin( engineFactory );
#endif //RWLIB_INCLUDE_PNG_IMAGING
}

};