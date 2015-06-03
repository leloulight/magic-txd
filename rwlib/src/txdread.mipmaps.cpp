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

void TextureBase::clearMipmaps( void )
{
    Raster *texRaster = this->GetRaster();

    if ( texRaster )
    {
        Interface *engineInterface = this->engineInterface;

        PlatformTexture *platformTex = texRaster->platformData;
        
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
                
                // Adjust the filtering.
                {
                    eRasterStageFilterMode currentFilterMode = this->GetFilterMode();

                    if ( currentFilterMode == RWFILTER_POINT_POINT ||
                         currentFilterMode == RWFILTER_POINT_LINEAR )
                    {
                        this->SetFilterMode( RWFILTER_POINT );
                    }
                    else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                              currentFilterMode == RWFILTER_LINEAR_LINEAR )
                    {
                        this->SetFilterMode( RWFILTER_LINEAR );
                    }
                }
            }

            // Now we can have automatically generated mipmaps!
        }
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

inline void performMipmapFiltering(
    eMipmapGenerationMode mipGenMode,
    Bitmap& srcBitmap, uint32 srcPosX, uint32 srcPosY, uint32 mipProcessWidth, uint32 mipProcessHeight,
    uint8& redOut, uint8& greenOut, uint8& blueOut, uint8& alphaOut
)
{
    uint8 red = 0;
    uint8 green = 0;
    uint8 blue = 0;
    uint8 alpha = 0;

    if ( mipGenMode == MIPMAPGEN_DEFAULT )
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
            red = std::min( redSumm / addCount, 255u );
            green = std::min( greenSumm / addCount, 255u );
            blue = std::min( blueSumm / addCount, 255u );
            alpha = std::min( alphaSumm / addCount, 255u );
        }
    }
    else
    {
        //assert( 0 );
    }

    redOut = red;
    greenOut = green;
    blueOut = blue;
    alphaOut = alpha;
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

void TextureBase::generateMipmaps( uint32 maxMipmapCount, eMipmapGenerationMode mipGenMode )
{
    Raster *texRaster = this->GetRaster();

    if ( texRaster )
    {
        PlatformTexture *platformTex = texRaster->platformData;

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
        Bitmap textureBitmap = texRaster->getBitmap();

        // Do the generation.
        // We process the image in 2x2 blocks for the level index 1, 4x4 for level index 2, ...
        uint32 firstLevelWidth, firstLevelHeight;
        textureBitmap.getSize( firstLevelWidth, firstLevelHeight );

        uint32 firstLevelDepth = textureBitmap.getDepth();

        eRasterFormat tmpRasterFormat = textureBitmap.getFormat();
        eColorOrdering tmpColorOrder = textureBitmap.getColorOrder();

        mipGenLevelGenerator mipLevelGen( firstLevelWidth, firstLevelHeight );

        if ( !mipLevelGen.isValidLevel() )
        {
            throw RwException( "invalid texture dimensions in mipmap generation (" + this->name + ")" );
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
                uint32 texItemCount = ( mipWidth * mipHeight );

                uint32 texDataSize = getRasterDataSize( texItemCount, firstLevelDepth );

                void *newtexels = engineInterface->PixelAllocate( texDataSize );

                texNativeTypeProvider::acquireFeedback_t acquireFeedback;

                bool couldAdd = false;

                try
                {
                    // Process the pixels.
                    bool hasAlpha = false;
                   
                    for ( uint32 mip_y = 0; mip_y < mipHeight; mip_y++ )
                    {
                        for ( uint32 mip_x = 0; mip_x < mipWidth; mip_x++ )
                        {
                            // Get the color for this pixel.
                            uint8 r, g, b, a;

                            r = 0;
                            g = 0;
                            b = 0;
                            a = 0;

                            // Perform a filter operation on the currently selected texture block.
                            uint32 mipProcessWidth = curMipProcessWidth;
                            uint32 mipProcessHeight = curMipProcessHeight;

                            performMipmapFiltering(
                                mipGenMode,
                                textureBitmap,
                                mip_x * mipProcessWidth, mip_y * mipProcessHeight,
                                mipProcessWidth, mipProcessHeight,
                                r, g, b, a
                            );

                            if ( a != 255 )
                            {
                                hasAlpha = true;
                            }

                            // Put the color.
                            uint32 colorIndex = PixelFormat::coord2index( mip_x, mip_y, mipWidth );

                            puttexelcolor(
                                newtexels, colorIndex, tmpRasterFormat, tmpColorOrder, firstLevelDepth,
                                r, g, b, a
                            );
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

        // Adjust filtering mode.
        {
            eRasterStageFilterMode currentFilterMode = this->GetFilterMode();

            if ( actualNewMipmapCount > 1 )
            {
                if ( currentFilterMode == RWFILTER_POINT )
                {
                    this->SetFilterMode( RWFILTER_POINT_POINT );
                }
                else if ( currentFilterMode == RWFILTER_LINEAR )
                {
                    this->SetFilterMode( RWFILTER_LINEAR_LINEAR );
                }
            }
            else
            {
                if ( currentFilterMode == RWFILTER_POINT_POINT ||
                     currentFilterMode == RWFILTER_POINT_LINEAR )
                {
                    this->SetFilterMode( RWFILTER_POINT );
                }
                else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                          currentFilterMode == RWFILTER_LINEAR_LINEAR )
                {
                    this->SetFilterMode( RWFILTER_LINEAR );
                }
            }
        }
    }
}

}