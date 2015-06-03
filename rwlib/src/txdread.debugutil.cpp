#include "StdInc.h"

#include "pixelformat.hxx"

namespace rw
{

// Draws all mipmap layers onto a mipmap.
bool DebugDrawMipmaps( Interface *engineInterface, Raster *debugRaster, Bitmap& bmpOut )
{
    // Only proceed if we have native data.
    PlatformTexture *platformTex = debugRaster->platformData;

    if ( !platformTex )
        return false;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
        return false;

    // Fetch pixel data from the texture and convert it to uncompressed data.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    // Return a debug bitmap which contains all mipmap layers.
    uint32 requiredBitmapWidth = 0;
    uint32 requiredBitmapHeight = 0;

    uint32 mipmapCount = pixelData.mipmaps.size();

    bool gotStuff = false;

    if ( mipmapCount > 0 )
    {
        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

            uint32 mipLayerWidth = mipLayer.mipWidth;
            uint32 mipLayerHeight = mipLayer.mipHeight;

            // We allocate all mipmaps from top left to top right.
            requiredBitmapWidth += mipLayerWidth;

            if ( requiredBitmapHeight < mipLayerHeight )
            {
                requiredBitmapHeight = mipLayerHeight;
            }
        }

        if ( requiredBitmapWidth != 0 && requiredBitmapHeight != 0 )
        {
            // Allocate bitmap space.
            bmpOut.setSize( requiredBitmapWidth, requiredBitmapHeight );

            // Cursor for the drawing operation.
            uint32 cursor_x = 0;
            uint32 cursor_y = 0;

            // Establish whether we have to convert the mipmaps.
            eRasterFormat srcRasterFormat = pixelData.rasterFormat;
            uint32 srcDepth = pixelData.depth;
            eColorOrdering srcColorOrder = pixelData.colorOrder;
            ePaletteType srcPaletteType = pixelData.paletteType;
            void *srcPaletteData = pixelData.paletteData;
            uint32 srcPaletteSize = pixelData.paletteSize;
            eCompressionType srcCompressionType = pixelData.compressionType;

            eRasterFormat reqRasterFormat = srcRasterFormat;
            uint32 reqDepth = srcDepth;
            eColorOrdering reqColorOrder = srcColorOrder;
            eCompressionType reqCompressionType = RWCOMPRESS_NONE;

            bool requiresConversion = false;

            if ( srcCompressionType != reqCompressionType )
            {
                requiresConversion = true;

                if ( srcCompressionType == RWCOMPRESS_DXT1 && pixelData.hasAlpha == false )
                {
                    reqRasterFormat = RASTER_888;
                    reqDepth = 32;
                }
                else
                {
                    reqRasterFormat = RASTER_8888;
                    reqDepth = 32;
                }
                
                reqColorOrder = COLOR_BGRA;
            }

            // Draw them.
            for ( uint32 n = 0; n < mipmapCount; n++ )
            {
                const pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ n ];

                uint32 mipWidth = mipLayer.width;
                uint32 mipHeight = mipLayer.height;

                uint32 layerWidth = mipLayer.mipWidth;
                uint32 layerHeight = mipLayer.mipHeight;

                void *srcTexels = mipLayer.texels;
                uint32 srcDataSize = mipLayer.dataSize;

                bool isNewlyAllocated = false;

                if ( requiresConversion )
                {
                    bool hasConverted =
                        ConvertMipmapLayerNative(
                            engineInterface,
                            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
                            srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                            reqRasterFormat, reqDepth, reqColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, reqCompressionType,
                            false,
                            mipWidth, mipHeight,
                            srcTexels, srcDataSize
                        );

                    assert( hasConverted == true );

                    isNewlyAllocated = true;
                }

                // Fetch colors from this mipmap layer.
                struct mipmapColorSourcePipeline : public Bitmap::sourceColorPipeline
                {
                    uint32 mipWidth, mipHeight;
                    uint32 depth;
                    const void *texelSource;
                    eRasterFormat rasterFormat;
                    eColorOrdering colorOrder;
                    const void *paletteData;
                    uint32 paletteSize;
                    ePaletteType paletteType;

                    inline mipmapColorSourcePipeline(
                        uint32 mipWidth, uint32 mipHeight, uint32 depth,
                        const void *texelSource,
                        eRasterFormat rasterFormat, eColorOrdering colorOrder,
                        const void *paletteData, uint32 paletteSize, ePaletteType paletteType
                    )
                    {
                        this->mipWidth = mipWidth;
                        this->mipHeight = mipHeight;
                        this->depth = depth;
                        this->texelSource = texelSource;
                        this->rasterFormat = rasterFormat;
                        this->colorOrder = colorOrder;
                        this->paletteData = paletteData;
                        this->paletteSize = paletteSize;
                        this->paletteType = paletteType;
                    }

                    uint32 getWidth( void ) const
                    {
                        return this->mipWidth;
                    }

                    uint32 getHeight( void ) const
                    {
                        return this->mipHeight;
                    }

                    void fetchcolor( uint32 colorIndex, double& red, double& green, double& blue, double& alpha )
                    {
                        uint8 r, g, b, a;

                        bool hasColor = browsetexelcolor(
                            this->texelSource, this->paletteType, this->paletteData, this->paletteSize,
                            colorIndex, this->rasterFormat, this->colorOrder, this->depth,
                            r, g, b, a
                        );

                        if ( !hasColor )
                        {
                            r = 0;
                            g = 0;
                            b = 0;
                            a = 0;
                        }

                        red = unpackcolor( r );
                        green = unpackcolor( g );
                        blue = unpackcolor( b );
                        alpha = unpackcolor( a );
                    }
                };

                mipmapColorSourcePipeline colorPipe(
                    mipWidth, mipHeight, reqDepth,
                    srcTexels,
                    reqRasterFormat, reqColorOrder,
                    srcPaletteData, srcPaletteSize, srcPaletteType
                );

                // Draw it at its position.
                bmpOut.draw(
                    colorPipe, cursor_x, cursor_y,
                    mipWidth, mipHeight,
                    Bitmap::SHADE_ZERO, Bitmap::SHADE_ONE, Bitmap::BLEND_ADDITIVE
                );

                // Delete if necessary.
                if ( isNewlyAllocated )
                {
                    engineInterface->PixelFree( srcTexels );
                }

                // Increase cursor.
                cursor_x += mipWidth;
            }

            gotStuff = true;
        }
    }

    pixelData.FreePixels( engineInterface );

    return gotStuff;
}

}