#include "StdInc.h"

#include <bitset>
#include <map>
#include <cmath>

#include "pixelformat.hxx"

#include "txdread.d3d.hxx"

#include "txdread.palette.hxx"

namespace rw
{

struct _fetch_texel_libquant_traverse
{
    pixelDataTraversal *pixelData;

    uint32 mipIndex;
};

static void _fetch_image_data_libquant(liq_color row_out[], int row_index, int width, void *user_info)
{
    _fetch_texel_libquant_traverse *info = (_fetch_texel_libquant_traverse*)user_info;

    pixelDataTraversal *pixelData = info->pixelData;

    // Fetch the color row.
    pixelDataTraversal::mipmapResource& mipLayer = pixelData->mipmaps[ info->mipIndex ];

    const void *texelSource = mipLayer.texels;

    eRasterFormat rasterFormat = pixelData->rasterFormat;
    ePaletteType paletteType = pixelData->paletteType;

    uint32 itemDepth = pixelData->depth;

    eColorOrdering colorOrder = pixelData->colorOrder;

    void *palColors = pixelData->paletteData;
    uint32 palColorCount = pixelData->paletteSize;

    for (int n = 0; n < width; n++)
    {
        uint32 colorIndex = PixelFormat::coord2index(n, row_index, width);

        // Fetch the color.
        uint8 r, g, b, a;

        browsetexelcolor(texelSource, paletteType, palColors, palColorCount, colorIndex, rasterFormat, colorOrder, itemDepth, r, g, b, a);

        // Store the color.
        liq_color& outColor = row_out[ n ];
        outColor.r = r;
        outColor.g = g;
        outColor.b = b;
        outColor.a = a;
    }
}

// Custom algorithm for palettizing image data.
// This routine is called by ConvertPixelData. It should not be called from anywhere else.
void PalettizePixelData( Interface *engineInterface, pixelDataTraversal& pixelData, const pixelFormat& dstPixelFormat )
{
    // Make sure the pixelData is not compressed.
    assert( pixelData.compressionType == RWCOMPRESS_NONE );
    assert( dstPixelFormat.compressionType == RWCOMPRESS_NONE );

    ePaletteType convPaletteFormat = dstPixelFormat.paletteType;

    if (convPaletteFormat != PALETTE_8BIT && convPaletteFormat != PALETTE_4BIT)
    {
        throw RwException( "unknown palette type target in palettization routine" );
    }

    ePaletteType srcPaletteType = pixelData.paletteType;

    // The reason for this shortcut is because the purpose of this algorithm is palettization.
    // If you want to change the raster format or anything, use ConvertPixelData!
    if (srcPaletteType == convPaletteFormat)
        return;

    // Get the source format.
    eRasterFormat srcRasterFormat = pixelData.rasterFormat;
    eColorOrdering srcColorOrder = pixelData.colorOrder;
    uint32 srcDepth = pixelData.depth;

    // Get the format that we want to output in.
    eRasterFormat dstRasterFormat = dstPixelFormat.rasterFormat;
    uint32 dstDepth = dstPixelFormat.depth;
    eColorOrdering dstColorOrder = dstPixelFormat.colorOrder;

    void *srcPaletteData = pixelData.paletteData;
    uint32 srcPaletteCount = pixelData.paletteSize;

    // Get palette maximums.
    uint32 maxPaletteEntries = 0;
    uint8 newDepth = 0;

    bool hasValidPaletteTarget = false;

    if (dstDepth == 8)
    {
        if (convPaletteFormat == PALETTE_8BIT)
        {
            maxPaletteEntries = 256;

            hasValidPaletteTarget = true;
        }
    }
    else if (dstDepth == 4)
    {
        if (convPaletteFormat == PALETTE_4BIT || convPaletteFormat == PALETTE_8BIT)
        {
            maxPaletteEntries = 16;

            hasValidPaletteTarget = true;
        }
    }
   
    if ( hasValidPaletteTarget == false )
    {
        throw RwException( "invalid palette depth in palettization routine" );
    }

    // Do the palettization.
    bool palettizeSuccess = false;
    {
        uint32 mipmapCount = pixelData.mipmaps.size();

        // Decide what palette system to use.
        ePaletteRuntimeType useRuntime = engineInterface->GetPaletteRuntime();

        if (useRuntime == PALRUNTIME_NATIVE)
        {
            palettizer conv;

            // Linear eliminate unique texels.
            // Use only the first texture.
            if ( mipmapCount > 0 )
            {
                pixelDataTraversal::mipmapResource& mainLayer = pixelData.mipmaps[ 0 ];

                uint32 srcWidth = mainLayer.mipWidth;
                uint32 srcHeight = mainLayer.mipHeight;
                uint32 srcStride = mainLayer.width;
                void *texelSource = mainLayer.texels;

#if 0
                // First define properties to use for linear elimination.
                for (uint32 y = 0; y < srcHeight; y++)
                {
                    for (uint32 x = 0; x < srcWidth; x++)
                    {
                        uint32 colorIndex = PixelFormat::coord2index(x, y, srcWidth);

                        uint8 red, green, blue, alpha;
                        bool hasColor = browsetexelcolor(texelSource, paletteType, paletteData, maxpalette, colorIndex, rasterFormat, red, green, blue, alpha);

                        if ( hasColor )
                        {
                            conv.characterize(red, green, blue, alpha);
                        }
                    }
                }

                // Prepare the linear elimination.
                conv.after_characterize();
#endif

                // Linear eliminate.
                for (uint32 y = 0; y < srcHeight; y++)
                {
                    for (uint32 x = 0; x < srcWidth; x++)
                    {
                        uint32 colorIndex = PixelFormat::coord2index(x, y, srcStride);

                        uint8 red, green, blue, alpha;
                        bool hasColor = browsetexelcolor(texelSource, srcPaletteType, srcPaletteData, srcPaletteCount, colorIndex, srcRasterFormat, srcColorOrder, srcDepth, red, green, blue, alpha);

                        if ( hasColor )
                        {
                            conv.feedcolor(red, green, blue, alpha);
                        }
                    }
                }
            }

            // Construct a palette out of the remaining colors.
            conv.constructpalette(maxPaletteEntries);

            // Point each color from the original texture to the palette.
            for (uint32 n = 0; n < mipmapCount; n++)
            {
                // Create palette index memory for each mipmap.
                pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                uint32 srcWidth = mipLayer.width;
                uint32 srcHeight = mipLayer.height;
                void *texelSource = mipLayer.texels;

                uint32 itemCount = ( srcWidth * srcHeight );
                
                uint32 dataSize = 0;
                void *newTexelData = NULL;

                // Remap the texels.
                nativePaletteRemap(
                    engineInterface,
                    conv, convPaletteFormat, dstDepth,
                    texelSource, itemCount,
                    srcPaletteType, srcPaletteData, srcPaletteCount, srcRasterFormat, srcColorOrder, srcDepth,
                    newTexelData, dataSize
                );

                // Replace texture data.
                if ( newTexelData != texelSource )
                {
                    if ( texelSource )
                    {
                        engineInterface->PixelFree( texelSource );
                    }

                    mipLayer.texels = newTexelData;
                }

                mipLayer.dataSize = dataSize;
            }

            // Delete the old palette data (if available).
            if (srcPaletteData != NULL)
            {
                engineInterface->PixelFree( srcPaletteData );
            }

            // Store the new palette texels.
            pixelData.paletteData = conv.makepalette(engineInterface, dstRasterFormat, dstColorOrder);
            pixelData.paletteSize = conv.texelElimData.size();

            palettizeSuccess = true;
        }
        else if (useRuntime == PALRUNTIME_PNGQUANT)
        {
            liq_attr *quant_attr = liq_attr_create();

            assert( quant_attr != NULL );

            liq_set_max_colors(quant_attr, maxPaletteEntries);

            _fetch_texel_libquant_traverse main_traverse;

            main_traverse.pixelData = &pixelData;
            main_traverse.mipIndex = 0;

            pixelDataTraversal::mipmapResource& mainLayer = pixelData.mipmaps[ 0 ];

            liq_image *quant_image = liq_image_create_custom(
                quant_attr, _fetch_image_data_libquant, &main_traverse,
                mainLayer.width, mainLayer.height,
                1.0
            );

            assert( quant_image != NULL );

            // Quant it!
            liq_result *quant_result = liq_quantize_image(quant_attr, quant_image);

            if (quant_result != NULL)
            {
                // Get the palette and remap all mipmaps.
                for (uint32 n = 0; n < mipmapCount; n++)
                {
                    pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                    uint32 mipWidth = mipLayer.width;
                    uint32 mipHeight = mipLayer.height;

                    uint32 palItemCount = ( mipWidth * mipHeight );

                    unsigned char *newPalItems = (unsigned char*)engineInterface->PixelAllocate( palItemCount );

                    assert( newPalItems != NULL );

                    liq_image *srcImage = NULL;
                    bool newImage = false;

                    _fetch_texel_libquant_traverse thisTraverse;

                    if ( n == 0 )
                    {
                        srcImage = quant_image;
                    }
                    else
                    {
                        // Create a new image.
                        thisTraverse.pixelData = &pixelData;
                        thisTraverse.mipIndex = n;

                        srcImage = liq_image_create_custom(
                            quant_attr, _fetch_image_data_libquant, &thisTraverse,
                            mipWidth, mipHeight,
                            1.0
                        );

                        newImage = true;
                    }

                    // Map it.
                    liq_write_remapped_image( quant_result, srcImage, newPalItems, palItemCount );

                    // Delete image (if newly allocated)
                    if (newImage)
                    {
                        liq_image_destroy( srcImage );
                    }

                    // Update the texels.
                    engineInterface->PixelFree( mipLayer.texels );

                    bool hasUsedArray = false;

                    {
                        uint32 dataSize = 0;
                        void *newTexelArray = NULL;

                        if (dstDepth == 4)
                        {
                            dataSize = PixelFormat::palette4bit::sizeitems( palItemCount );
                        }
                        else if (dstDepth == 8)
                        {
                            dataSize = palItemCount;

                            newTexelArray = newPalItems;

                            hasUsedArray = true;
                        }

                        if ( !hasUsedArray )
                        {
                            newTexelArray = engineInterface->PixelAllocate( dataSize );

                            // Copy over the items.
                            for ( uint32 n = 0; n < palItemCount; n++ )
                            {
                                uint32 resVal = newPalItems[ n ];

                                if (dstDepth == 4)
                                {
                                    ( (PixelFormat::palette4bit*)newTexelArray )->setvalue(n, resVal);
                                }
                                else if (dstDepth == 8)
                                {
                                    ( (PixelFormat::palette8bit*)newTexelArray)->setvalue(n, resVal);
                                }
                            }
                        }

                        // Set the texels.
                        mipLayer.texels = newTexelArray;
                        mipLayer.dataSize = dataSize;
                    }

                    if ( !hasUsedArray )
                    {
                        engineInterface->PixelFree( newPalItems );
                    }
                }

                // Delete the old palette data.
                if (srcPaletteData != NULL)
                {
                    engineInterface->PixelFree( srcPaletteData );
                }

                // Update the texture palette data.
                {
                    const liq_palette *palData = liq_get_palette(quant_result);

                    uint32 newPalItemCount = palData->count;

                    uint32 palDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

                    uint32 palDataSize = getRasterDataSize( newPalItemCount, palDepth );

                    void *newPalArray = engineInterface->PixelAllocate( palDataSize );

                    for ( unsigned int n = 0; n < newPalItemCount; n++ )
                    {
                        const liq_color& srcColor = palData->entries[ n ];

                        puttexelcolor(newPalArray, n, dstRasterFormat, dstColorOrder, palDepth, srcColor.r, srcColor.g, srcColor.b, srcColor.a);
                    }

                    // Update texture properties.
                    pixelData.paletteData = newPalArray;
                    pixelData.paletteSize = newPalItemCount;
                }
            }
            else
            {
                assert( 0 );
            }

            // Release resources.
            liq_image_destroy( quant_image );

            liq_attr_destroy( quant_attr );

            liq_result_destroy( quant_result );

            palettizeSuccess = true;
        }
        else
        {
            assert( 0 );
        }
    }

    // If the palettization was a success, we update the pixelData raster format fields.
    if ( palettizeSuccess )
    {
        if ( srcRasterFormat != dstRasterFormat )
        {
            pixelData.rasterFormat = dstRasterFormat;
        }

        if ( srcColorOrder != dstColorOrder )
        {
            pixelData.colorOrder = dstColorOrder;
        }

        // Set the new depth of the texture.
        if ( srcDepth != dstDepth )
        {
            pixelData.depth = dstDepth;
        }

        // Notify the raster about its new format.
        pixelData.paletteType = convPaletteFormat;
    }
}

void Raster::convertToPalette( ePaletteType paletteType )
{
    // NULL operation.
    if ( paletteType == PALETTE_NONE )
        return;

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    // If the raster has the same palettization as we request, we can terminate early.
    ePaletteType currentPaletteType = texProvider->GetTexturePaletteType( platformTex );

    if ( currentPaletteType == paletteType )
        return;

    // Get palette default capabilities.
    uint32 dstDepth;

    if ( paletteType == PALETTE_4BIT )
    {
        dstDepth = 4;
    }
    else if ( paletteType == PALETTE_8BIT )
    {
        dstDepth = 8;
    }
    else
    {
        throw RwException( "unknown palette type in raster palettization routine" );
    }

    // Decide whether the target raster even supports palette.
    pixelCapabilities inputTransferCaps;

    texProvider->GetPixelCapabilities( inputTransferCaps );

    if ( inputTransferCaps.supportsPalette == false )
    {
        throw RwException( "target raster does not support palette input" );
    }

    storageCapabilities storageCaps;

    texProvider->GetStorageCapabilities( storageCaps );

    if ( storageCaps.pixelCaps.supportsPalette == false )
    {
        throw RwException( "target raster cannot store palette data" );
    }

    // Alright, our native data does support palette data.
    // We now want to fetch the rasters pixel data, make it private and palettize it.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    bool hasDirectlyAcquired = false;

    try
    {
        // Unset it from the original texture.
        texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

        // Pixel data is now safely stand-alone.
        pixelData.SetStandalone();

        // We always want to palettize to 32bit quality.
        eRasterFormat targetRasterFormat = pixelData.rasterFormat;

        if ( pixelData.hasAlpha )
        {
            targetRasterFormat = RASTER_8888;
        }
        else
        {
            targetRasterFormat = RASTER_888;
        }

        // Convert the pixel data to palette.
        pixelFormat targetPixelFormat;
        targetPixelFormat.rasterFormat = targetRasterFormat;
        targetPixelFormat.depth = dstDepth;
        targetPixelFormat.colorOrder = pixelData.colorOrder;
        targetPixelFormat.paletteType = paletteType;
        targetPixelFormat.compressionType = RWCOMPRESS_NONE;

        bool hasConverted = ConvertPixelData( engineInterface, pixelData, targetPixelFormat );

        if ( !hasConverted )
        {
            throw RwException( "pixel conversion failed in palettization routine" );
        }

        // Now set the pixels to the texture again.
        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

        hasDirectlyAcquired = acquireFeedback.hasDirectlyAcquired;
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( hasDirectlyAcquired == false )
    {
        // This should never happen.
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

};