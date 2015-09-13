#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE

#include "txdread.gc.hxx"

#include "pluginutil.hxx"

#include "streamutil.hxx"

namespace rw
{

void gamecubeNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    // Let's write a gamecube native texture!
    Interface *engineInterface = theTexture->engineInterface;

    {
        BlockProvider gcNativeBlock( &outputProvider );

        gcNativeBlock.EnterContext();

        try
        {
            const NativeTextureGC *platformTex = (const NativeTextureGC*)nativeTex;
            
            eGCNativeTextureFormat internalFormat = platformTex->internalFormat;
            eGCPixelFormat palettePixelFormat = platformTex->palettePixelFormat;

            size_t mipmapCount = platformTex->mipmaps.size();

            if ( mipmapCount == 0 )
            {
                throw RwException( "attempt to write an empty texture (" + theTexture->GetName() + ")" );
            }

            const NativeTextureGC::mipmapLayer& baseLayer = platformTex->mipmaps[ 0 ];

            // We first have to write the header.
            ePaletteType paletteType;
            {
                gamecube::textureMetaHeaderStructGeneric metaHeader;
                metaHeader.platformDescriptor = PLATFORMDESC_GAMECUBE;
            
                // Write the format info.
                texFormatInfo formatInfo;
                formatInfo.set( *theTexture );

                metaHeader.texFormat = formatInfo;

                // First write the unknowns.
                metaHeader.unk1 = platformTex->unk1;
                metaHeader.unk2 = platformTex->unk2;
                metaHeader.unk3 = platformTex->unk3;
                metaHeader.unk4 = platformTex->unk4;

                // Write texture names.
                // These need to be written securely.
                writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaHeader.name, sizeof( metaHeader.name ), theTexture->GetName(), "name" );
                writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaHeader.maskName, sizeof( metaHeader.maskName ), theTexture->GetName(), "mask name" );

                bool hasAlpha = platformTex->hasAlpha;

                // Generate the raster format flags.
                eRasterFormat virtualRasterFormat;
                uint32 depth;
                eColorOrdering colorOrder;  // not needed.

                bool hasFormat =
                    getGCNativeTextureRasterFormat(
                        internalFormat, palettePixelFormat,
                        virtualRasterFormat, depth, colorOrder, paletteType
                    );

                // If we have no valid mapping, we have to write some fake mapping that could make sense.
                if ( !hasFormat )
                {
                    if ( internalFormat == GVRFMT_RGB5A3 )
                    {
                        virtualRasterFormat = RASTER_4444;
                        depth = 16;
   
                        hasFormat = true;
                    }
                    else if ( internalFormat == GVRFMT_PAL_4BIT ||
                              internalFormat == GVRFMT_PAL_8BIT )
                    {
                        if ( palettePixelFormat == GVRPIX_RGB5A3 )
                        {
                            virtualRasterFormat = RASTER_4444;
                            depth = 16;

                            hasFormat = true;
                        }
                    }
                    else if ( internalFormat == GVRFMT_CMP )
                    {
                        virtualRasterFormat = RASTER_DEFAULT;
                        depth = 4;

                        hasFormat = true;
                    }

                    // If we still have no format, just say that we do not know.
                    if ( !hasFormat )
                    {
                        virtualRasterFormat = RASTER_DEFAULT;
                        depth = 0;
                    }

                    if ( internalFormat == GVRFMT_PAL_4BIT )
                    {
                        paletteType = PALETTE_4BIT;
                    }
                    else if ( internalFormat == GVRFMT_PAL_8BIT )
                    {
                        paletteType = PALETTE_8BIT;
                    }
                    else
                    {
                        paletteType = PALETTE_NONE;
                    }
                }

                uint32 rasterFormatFlags =
                    generateRasterFormatFlags( virtualRasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps );

                // Set some advanced stuff.
                rasterFormatFlags |= platformTex->rasterType;

                metaHeader.rasterFormat = rasterFormatFlags;

                metaHeader.width = baseLayer.layerWidth;
                metaHeader.height = baseLayer.layerHeight;
                metaHeader.depth = depth;
                metaHeader.mipmapCount = (uint8)mipmapCount;
                metaHeader.internalFormat = internalFormat;
                metaHeader.palettePixelFormat = palettePixelFormat;
                
                metaHeader.hasAlpha = ( hasAlpha ? 1 : 0 );

                // Write it.
                gcNativeBlock.writeStruct( metaHeader );
            }

            // Now write palette data.
            if ( paletteType != PALETTE_NONE )
            {
                uint32 palRasterDepth = getGVRPixelFormatDepth( palettePixelFormat );

                uint32 reqPalCount = getGCPaletteSize( paletteType );

                uint32 paletteCount = platformTex->paletteSize;

                uint32 palDataSize = getPaletteDataSize( paletteCount, palRasterDepth );

                writePartialBlockSafe(gcNativeBlock, platformTex->palette, palDataSize, getPaletteDataSize(reqPalCount, palRasterDepth));
            }

            // We need to write the total size of image data.
            // For that we have to calculate it.
            uint32 imageDataSectionSize = 0;

            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                const NativeTextureGC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

                imageDataSectionSize += mipLayer.dataSize;
            }

            endian::big_endian <uint32> sectionSizeBE = imageDataSectionSize;

            gcNativeBlock.writeStruct( sectionSizeBE );

            // Write all the mipmap layers now.
            for ( size_t n = 0; n < mipmapCount; n++ )
            {
                const NativeTextureGC::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

                uint32 texDataSize = mipLayer.dataSize;

                const void *texels = mipLayer.texels;

                gcNativeBlock.write( texels, texDataSize );
            }

            // Finito.
        }
        catch( ... )
        {
            gcNativeBlock.LeaveContext();

            throw;
        }

        gcNativeBlock.LeaveContext();
    }

    // Also write it's extensions.
    engineInterface->SerializeExtensions( theTexture, outputProvider );
}

void gamecubeNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    
}

void gamecubeNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, texNativeTypeProvider::acquireFeedback_t& acquireFeedback )
{

}

void gamecubeNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{

}

static PluginDependantStructRegister <gamecubeNativeTextureTypeProvider, RwInterfaceFactory_t> gcNativeTypeRegister;

void registerGCNativePlugin( void )
{
    gcNativeTypeRegister.RegisterPlugin( engineFactory );
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE