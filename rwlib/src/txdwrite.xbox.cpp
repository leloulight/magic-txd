#include <StdInc.h>

#include "txdread.xbox.hxx"

#include "txdread.d3d.hxx"

#include "streamutil.hxx"

namespace rw
{

eTexNativeCompatibility xboxNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility texCompat = RWTEXCOMPAT_NONE;

    BlockProvider texNativeImageBlock( &inputProvider );

    texNativeImageBlock.EnterContext();

    try
    {
        if ( texNativeImageBlock.getBlockID() == CHUNK_STRUCT )
        {
            // Here we can check the platform descriptor, since we know it is unique.
            uint32 platformDescriptor = texNativeImageBlock.readUInt32();

            if ( platformDescriptor == PLATFORM_XBOX )
            {
                texCompat = RWTEXCOMPAT_ABSOLUTE;
            }
        }
    }
    catch( ... )
    {
        texNativeImageBlock.LeaveContext();

        throw;
    }

    texNativeImageBlock.LeaveContext();

    return texCompat;
}

void xboxNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Cast the texture to our native type.
    NativeTextureXBOX *platformTex = (NativeTextureXBOX*)nativeTex;

    // Debug some essentials.
    ePaletteType paletteType = platformTex->paletteType;

    uint32 compressionType = platformTex->dxtCompression;

    // If we are not compressed, then the color order matters.
    if ( compressionType == 0 )
    {
        // XBOX textures are always BGRA.
        eColorOrdering requiredColorOrder = COLOR_BGRA;

        if ( platformTex->colorOrder != requiredColorOrder )
        {
            throw RwException( "texture " + theTexture->GetName() + " has an invalid color ordering for writing" );
        }
    }

    // Write the struct.
    {
        BlockProvider texImageDataBlock( &outputProvider );

        texImageDataBlock.EnterContext();

        try
        {
            // First comes the platform id.
            texImageDataBlock.writeUInt32( PLATFORM_XBOX );

            // Write the header.
            uint32 mipmapCount = platformTex->mipmaps.size();
            {
                textureMetaHeaderStructXbox metaInfo;

                // Write addressing information.
                metaInfo.formatInfo.set( *theTexture );

                // Write texture names.
                // These need to be written securely.
                writeStringIntoBufferSafe( engineInterface, theTexture->GetName(), metaInfo.name, sizeof( metaInfo.name ), theTexture->GetName(), "name" );
                writeStringIntoBufferSafe( engineInterface, theTexture->GetMaskName(), metaInfo.maskName, sizeof( metaInfo.maskName ), theTexture->GetName(), "mask name" );

                // Construct raster flags.
                uint32 rasterFlags = generateRasterFormatFlags(platformTex->rasterFormat, paletteType, mipmapCount > 1, platformTex->autoMipmaps);

                // Store the flags.
                metaInfo.rasterFormat = rasterFlags;

                metaInfo.hasAlpha = platformTex->hasAlpha;

                metaInfo.mipmapCount = mipmapCount;

                metaInfo.rasterType = platformTex->rasterType;

                metaInfo.dxtCompression = compressionType;

                // Write the dimensions.
                metaInfo.width = platformTex->mipmaps[ 0 ].layerWidth;
                metaInfo.height = platformTex->mipmaps[ 0 ].layerHeight;

                metaInfo.depth = platformTex->depth;

                // Calculate the size of all the texture data combined.
                uint32 imageDataSectionSize = 0;

                for (uint32 n = 0; n < mipmapCount; n++)
                {
                    imageDataSectionSize += platformTex->mipmaps[ n ].dataSize;
                }

                metaInfo.imageDataSectionSize = imageDataSectionSize;

                // Write the generic header.
                texImageDataBlock.write( &metaInfo, sizeof( metaInfo ) );
            }

            // Write palette data (if available).
            if (paletteType != PALETTE_NONE)
            {
                // Make sure we write as much data as the system expects.
                uint32 reqPalCount = getD3DPaletteCount(paletteType);

                uint32 palItemCount = platformTex->paletteSize;

                // Get the real data size of the palette.
                uint32 palRasterDepth = Bitmap::getRasterFormatDepth(platformTex->rasterFormat);

                uint32 paletteDataSize = getRasterDataSize( palItemCount, palRasterDepth );

                uint32 palByteWriteCount = writePartialBlockSafe(texImageDataBlock, platformTex->palette, paletteDataSize, getRasterDataSize(reqPalCount, palRasterDepth));
        
                assert( palByteWriteCount * 8 / palRasterDepth == reqPalCount );
            }

            // Write mipmap data.
            for ( uint32 n = 0; n < mipmapCount; n++ )
            {
                const NativeTextureXBOX::mipmapLayer& mipLayer = platformTex->mipmaps[ n ];

			    uint32 texDataSize = mipLayer.dataSize;

                const void *texelData = mipLayer.texels;

			    texImageDataBlock.write( texelData, texDataSize );
            }
        }
        catch( ... )
        {
            texImageDataBlock.LeaveContext();

            throw;
        }

        texImageDataBlock.LeaveContext();
    }

	// Extension
	engineInterface->SerializeExtensions( theTexture, outputProvider );
}

};