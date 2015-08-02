// Mipmap utilities for textures.
#include "StdInc.h"

#include "pixelformat.hxx"

#include "txdread.palette.hxx"

#include "txdread.common.hxx"

namespace rw
{

uint32 Raster::getMipmapCount( void ) const
{
    uint32 mipmapCount = 0;

    if ( PlatformTexture *platformTex = this->platformData )
    {
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        if ( texProvider )
        {
            mipmapCount = GetNativeTextureMipmapCount( engineInterface, platformTex, texProvider );
        }
    }

    return mipmapCount;
}

void Raster::clearMipmaps( void )
{
    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;
        
    if ( platformTex )
    {
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        // Clear mipmaps. This is image data starting from level index 1.
        nativeTextureBatchedInfo info;

        texProvider->GetTextureInfo( engineInterface, platformTex, info );

        if ( info.mipmapCount > 1 )
        {
            // Call into the native type provider.
            texProvider->ClearMipmaps( engineInterface, platformTex );
        }

        // Now we can have automatically generated mipmaps!
    }
}

inline uint32 pow2( uint32 val )
{
    uint32 n = 1;

    while ( val )
    {
        n *= 2;

        val--;
    }

    return n;
}

inline bool performMipmapFiltering(
    eMipmapGenerationMode mipGenMode,
    Bitmap& srcBitmap, eColorModel model, uint32 srcPosX, uint32 srcPosY, uint32 mipProcessWidth, uint32 mipProcessHeight,
    abstractColorItem& colorItem
)
{
    bool success = false;

    if ( mipGenMode == MIPMAPGEN_DEFAULT )
    {
        if ( model == COLORMODEL_RGBA )
        {
            uint32 redSumm = 0;
            uint32 greenSumm = 0;
            uint32 blueSumm = 0;
            uint32 alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < mipProcessHeight; y++ )
            {
                for ( uint32 x = 0; x < mipProcessWidth; x++ )
                {
                    uint8 r, g, b, a;

                    bool hasColor = srcBitmap.browsecolor(
                        x + srcPosX, y + srcPosY,
                        r, g, b, a
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        redSumm += r;
                        greenSumm += g;
                        blueSumm += b;
                        alphaSumm += a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.rgbaColor.r = std::min( redSumm / addCount, 255u );
                colorItem.rgbaColor.g = std::min( greenSumm / addCount, 255u );
                colorItem.rgbaColor.b = std::min( blueSumm / addCount, 255u );
                colorItem.rgbaColor.a = std::min( alphaSumm / addCount, 255u );

                colorItem.model = COLORMODEL_RGBA;

                success = true;
            }
        }
        else if ( model == COLORMODEL_LUMINANCE )
        {
            uint32 lumSumm = 0;
            uint32 alphaSumm = 0;

            // Loop through the texels and calculate a blur.
            uint32 addCount = 0;

            for ( uint32 y = 0; y < mipProcessHeight; y++ )
            {
                for ( uint32 x = 0; x < mipProcessWidth; x++ )
                {
                    uint8 lumm, a;

                    bool hasColor = srcBitmap.browselum(
                        x + srcPosX, y + srcPosY,
                        lumm, a
                    );

                    if ( hasColor )
                    {
                        // Add colors together.
                        lumSumm += lumm;
                        alphaSumm += a;

                        addCount++;
                    }
                }
            }

            if ( addCount != 0 )
            {
                // Calculate the real color.
                colorItem.luminance.lum = std::min( lumSumm / addCount, 255u );
                colorItem.luminance.alpha = std::min( alphaSumm / addCount, 255u );

                colorItem.model = COLORMODEL_LUMINANCE;

                success = true;
            }
        }
    }
    else
    {
        //assert( 0 );
    }

    return success;
}

template <typename dataType, typename containerType>
inline void ensurePutIntoArray( dataType dataToPut, containerType& container, uint32 putIndex )
{
    if ( container.size() <= putIndex )
    {
        container.resize( putIndex + 1 );
    }

    container[ putIndex ] = dataToPut;
}

void Raster::generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode )
{
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

    // We cannot do anything if we do not have a first level (base texture).
    nativeTextureBatchedInfo nativeInfo;

    texProvider->GetTextureInfo( engineInterface, platformTex, nativeInfo );

    uint32 oldMipmapCount = nativeInfo.mipmapCount;

    if ( oldMipmapCount == 0 )
        return;

    // Grab the bitmap of this texture, so we can generate mipmaps.
    Bitmap textureBitmap = this->getBitmap();

    // Do the generation.
    // We process the image in 2x2 blocks for the level index 1, 4x4 for level index 2, ...
    uint32 firstLevelWidth, firstLevelHeight;
    textureBitmap.getSize( firstLevelWidth, firstLevelHeight );

    uint32 firstLevelDepth = textureBitmap.getDepth();
    uint32 firstLevelRowAlignment = textureBitmap.getRowAlignment();

    eRasterFormat tmpRasterFormat = textureBitmap.getFormat();
    eColorOrdering tmpColorOrder = textureBitmap.getColorOrder();

    mipGenLevelGenerator mipLevelGen( firstLevelWidth, firstLevelHeight );

    if ( !mipLevelGen.isValidLevel() )
    {
        throw RwException( "invalid raster dimensions in mipmap generation" );
    }

    uint32 curMipProcessWidth = 1;
    uint32 curMipProcessHeight = 1;

    uint32 curMipIndex = 0;

    uint32 actualNewMipmapCount = oldMipmapCount;

    while ( true )
    {
        bool canProcess = false;

        if ( !canProcess )
        {
            if ( curMipIndex >= oldMipmapCount )
            {
                canProcess = true;
            }
        }

        if ( canProcess )
        {
            // Check whether the current gen index does not overshoot the max gen count.
            if ( curMipIndex >= maxMipmapCount )
            {
                break;
            }

            // Get the processing dimensions.
            uint32 mipWidth = mipLevelGen.getLevelWidth();
            uint32 mipHeight = mipLevelGen.getLevelHeight();

            // Allocate the new bitmap.
            uint32 texRowSize = getRasterDataRowSize( mipWidth, firstLevelDepth, firstLevelRowAlignment );

            uint32 texDataSize = getRasterDataSizeByRowSize( texRowSize, mipHeight );

            void *newtexels = engineInterface->PixelAllocate( texDataSize );

            texNativeTypeProvider::acquireFeedback_t acquireFeedback;

            bool couldAdd = false;

            try
            {
                // Process the pixels.
                bool hasAlpha = false;

                colorModelDispatcher <void> putDispatch( tmpRasterFormat, tmpColorOrder, firstLevelDepth, NULL, 0, PALETTE_NONE );

                eColorModel srcColorModel = textureBitmap.getColorModel();
                   
                for ( uint32 mip_y = 0; mip_y < mipHeight; mip_y++ )
                {
                    void *dstRow = getTexelDataRow( newtexels, texRowSize, mip_y );

                    for ( uint32 mip_x = 0; mip_x < mipWidth; mip_x++ )
                    {
                        // Get the color for this pixel.
                        abstractColorItem colorItem;

                        // Perform a filter operation on the currently selected texture block.
                        uint32 mipProcessWidth = curMipProcessWidth;
                        uint32 mipProcessHeight = curMipProcessHeight;

                        bool couldPerform = performMipmapFiltering(
                            mipGenMode,
                            textureBitmap, srcColorModel,
                            mip_x * mipProcessWidth, mip_y * mipProcessHeight,
                            mipProcessWidth, mipProcessHeight,
                            colorItem
                        );

                        // Put the color.
                        if ( couldPerform == true )
                        {
                            // Decide if we have alpha.
                            if ( srcColorModel == COLORMODEL_RGBA )
                            {
                                if ( colorItem.rgbaColor.a != 255 )
                                {
                                    hasAlpha = true;
                                }
                            }

                            putDispatch.setColor( dstRow, mip_x, colorItem );
                        }
                        else
                        {
                            // We do have alpha.
                            hasAlpha = true;

                            putDispatch.clearColor( dstRow, mip_x );
                        }
                    }
                }

                // Push the texels into the texture.
                rawMipmapLayer rawMipLayer;

                rawMipLayer.mipData.width = mipWidth;
                rawMipLayer.mipData.height = mipHeight;

                rawMipLayer.mipData.mipWidth = mipWidth;   // layer dimensions.
                rawMipLayer.mipData.mipHeight = mipHeight;

                rawMipLayer.mipData.texels = newtexels;
                rawMipLayer.mipData.dataSize = texDataSize;

                rawMipLayer.rasterFormat = tmpRasterFormat;
                rawMipLayer.depth = firstLevelDepth;
                rawMipLayer.rowAlignment = firstLevelRowAlignment;
                rawMipLayer.colorOrder = tmpColorOrder;
                rawMipLayer.paletteType = PALETTE_NONE;
                rawMipLayer.paletteData = NULL;
                rawMipLayer.paletteSize = 0;
                rawMipLayer.compressionType = RWCOMPRESS_NONE;
                    
                rawMipLayer.hasAlpha = hasAlpha;

                rawMipLayer.isNewlyAllocated = true;

                couldAdd = texProvider->AddMipmapLayer(
                    engineInterface, platformTex, rawMipLayer, acquireFeedback
                );
            }
            catch( ... )
            {
                // We have not successfully pushed the texels, so deallocate us.
                engineInterface->PixelFree( newtexels );

                throw;
            }

            if ( couldAdd == false || acquireFeedback.hasDirectlyAcquired == false )
            {
                // If the texture has not directly acquired the texels, we must free our copy.
                engineInterface->PixelFree( newtexels );
            }

            if ( couldAdd == false )
            {
                // If we failed to add any mipmap, we abort operation.
                break;
            }

            // We have a new mipmap.
            actualNewMipmapCount++;
        }

        // Increment mip index.
        curMipIndex++;

        // Process parameters.
        bool shouldContinue = mipLevelGen.incrementLevel();

        if ( shouldContinue )
        {
            if ( mipLevelGen.didIncrementWidth() )
            {
                curMipProcessWidth *= 2;
            }

            if ( mipLevelGen.didIncrementHeight() )
            {
                curMipProcessHeight *= 2;
            }
        }
        else
        {
            break;
        }
    }
}

}