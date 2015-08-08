#include "StdInc.h"

#include "txdread.gc.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.dxt.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

eTexNativeCompatibility gamecubeNativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility compat = RWTEXCOMPAT_NONE;

    // Read the native struct.
    BlockProvider gcNativeBlock( &inputProvider );

    gcNativeBlock.EnterContext();

    try
    {
        if ( gcNativeBlock.getBlockID() == CHUNK_STRUCT )
        {
            // We are definately a gamecube native texture if the platform descriptor matches.
            gamecube::textureMetaHeaderStructGeneric metaHeader;

            gcNativeBlock.readStruct( metaHeader );

            if ( metaHeader.platformDescriptor == PLATFORMDESC_GAMECUBE )
            {
                // We should definately be a gamecube native texture.
                // Whatever errors will crop up should be handled as rule violations of the GC native texture format.
                compat = RWTEXCOMPAT_ABSOLUTE;
            }
        }
        else
        {
            throw RwException( "unknown gamecube native block; expected chunk struct" );
        }
    }
    catch( ... )
    {
        gcNativeBlock.LeaveContext();

        throw;
    }

    gcNativeBlock.LeaveContext();

    return compat;
}

void gamecubeNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    {
        // Read the native struct.
        BlockProvider gcNativeBlock( &inputProvider );

        gcNativeBlock.EnterContext();

        try
        {
            if ( gcNativeBlock.getBlockID() == CHUNK_STRUCT )
            {
                // Read the main header.
                gamecube::textureMetaHeaderStructGeneric metaHeader;

                gcNativeBlock.readStruct( metaHeader );

                if ( metaHeader.platformDescriptor != PLATFORMDESC_GAMECUBE )
                {
                    throw RwException( "invalid platform descriptor in gamecube native texture deserialization" );
                }

                NativeTextureGC *platformTex = (NativeTextureGC*)nativeTex;

                int engineWarningLevel = engineInterface->GetWarningLevel();

                bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

                // Parse the header.
                // Read the texture names.
                {
                    char tmpbuf[ sizeof( metaHeader.name ) + 1 ];

                    // Make sure the name buffer is zero terminted.
                    tmpbuf[ sizeof( metaHeader.name ) ] = '\0';

                    // Move over the texture name.
                    memcpy( tmpbuf, metaHeader.name, sizeof( metaHeader.name ) );

                    theTexture->SetName( tmpbuf );

                    // Move over the texture mask name.
                    memcpy( tmpbuf, metaHeader.maskName, sizeof( metaHeader.maskName ) );

                    theTexture->SetMaskName( tmpbuf );
                }

                // Parse the addressing and filtering properties.
                texFormatInfo formatInfo = metaHeader.texFormat;

                formatInfo.parse( *theTexture );

                // Verify that it has correct entries.
                eGCNativeTextureFormat internalFormat = metaHeader.internalFormat;

                if ( internalFormat != GVRFMT_LUM_4BIT &&
                     internalFormat != GVRFMT_LUM_8BIT &&
                     internalFormat != GVRFMT_LUM_4BIT_ALPHA &&
                     internalFormat != GVRFMT_LUM_8BIT_ALPHA &&
                     internalFormat != GVRFMT_RGB565 &&
                     internalFormat != GVRFMT_RGB5A3 &&
                     internalFormat != GVRFMT_RGBA8888 &&
                     internalFormat != GVRFMT_PAL_4BIT &&
                     internalFormat != GVRFMT_PAL_8BIT &&
                     internalFormat != GVRFMT_CMP )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has an unknown gamecube native texture format" );
                }

                // Verify the palette format.
                eGCPixelFormat palettePixelFormat = metaHeader.palettePixelFormat;

                if ( palettePixelFormat != GVRPIX_NO_PALETTE &&
                     palettePixelFormat != GVRPIX_LUM_ALPHA &&
                     palettePixelFormat != GVRPIX_RGB5A3 &&
                     palettePixelFormat != GVRPIX_RGB565 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has an unknown palette pixel format" );
                }

                // Check whether we have a solid palette format description.
                // We cannot have the internal format say we have a palette but the pixel format say otherwise.
                {
                    bool hasPaletteByInternalFormat = ( internalFormat == GVRFMT_PAL_4BIT || internalFormat == GVRFMT_PAL_8BIT );
                    bool hasPaletteByPixelFormat = ( palettePixelFormat != GVRPIX_NO_PALETTE );

                    if ( hasPaletteByInternalFormat != hasPaletteByPixelFormat )
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has ambiguous palette format description" );
                    }
                }

                // Parse the raster format.
                eRasterFormat rasterFormat;     // this raster format is invalid anyway.
                ePaletteType paletteType;
                bool hasMipmaps;
                bool autoMipmaps;

                uint32 rasterFormatFlags = metaHeader.rasterFormat;

                readRasterFormatFlags( rasterFormatFlags, rasterFormat, paletteType, hasMipmaps, autoMipmaps );

                // Read advanced things.
                uint8 rasterType = ( rasterFormatFlags & 0xFF );

                uint32 depth = metaHeader.depth;

                // Verify properties.
                {
                    eRasterFormat gcRasterFormat;
                    uint32 gcDepth;
                    eColorOrdering gcColorOrder;
                    ePaletteType gcPaletteType;

                    bool hasGCFormat = getGCNativeTextureRasterFormat( internalFormat, palettePixelFormat, gcRasterFormat, gcDepth, gcColorOrder, gcPaletteType );

                    if ( hasGCFormat )
                    {
                        // - depth
                        {
                            if ( depth != gcDepth )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid depth (ignoring)" );

                                // Fix it.
                                depth = gcDepth;
                            }
                        }
                        // - raster format.
                        {
                            if ( rasterFormat != gcRasterFormat )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid raster format (ignoring)" );

                                rasterFormat = gcRasterFormat;
                            }
                        }
                        // - palette type.
                        {
                            if ( paletteType != gcPaletteType )
                            {
                                engineInterface->PushWarning( "terxture " + theTexture->GetName() + " has an invalid palette type (ignoring)" );

                                paletteType = gcPaletteType;
                            }
                        }
                    }
                }

                // Store raster properties in the native texture.
                platformTex->internalFormat = internalFormat;
                platformTex->palettePixelFormat = palettePixelFormat;
                platformTex->autoMipmaps = autoMipmaps;
                platformTex->hasAlpha = ( metaHeader.hasAlpha != 0 );
                platformTex->rasterType = rasterType;

                platformTex->paletteType = paletteType;

                // Store some unknowns.
                platformTex->unk1 = metaHeader.unk1;
                platformTex->unk2 = metaHeader.unk2;
                platformTex->unk3 = metaHeader.unk3;
                platformTex->unk4 = metaHeader.unk4;
                
                // Read the palette.
                if ( paletteType != PALETTE_NONE )
                {
                    uint32 palRasterDepth = getGVRPixelFormatDepth( palettePixelFormat );

                    uint32 paletteSize = getGCPaletteSize( paletteType );

                    uint32 palDataSize = getPaletteDataSize( paletteSize, palRasterDepth );

                    gcNativeBlock.check_read_ahead( palDataSize );

                    void *palData = engineInterface->PixelAllocate( palDataSize );

                    if ( !palData )
                    {
                        throw RwException( "failed to allocate palette buffer for texture " + theTexture->GetName() );
                    }

                    try
                    {
                        gcNativeBlock.read( palData, palDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( palData );

                        throw;
                    }

                    platformTex->palette = palData;
                    platformTex->paletteSize = paletteSize;
                }

                // Fetch the image data section size.
                uint32 imageDataSectionSize;
                {
                    endian::big_endian <uint32> secSize;

                    gcNativeBlock.readStruct( secSize );

                    imageDataSectionSize = secSize;
                }

                // Read the image data now.
                uint32 actualImageDataLength = imageDataSectionSize;

                uint32 mipmapCount = 0;

                uint32 maybeMipmapCount = metaHeader.mipmapCount;

                mipGenLevelGenerator mipGen( metaHeader.width, metaHeader.height );

                if ( !mipGen.isValidLevel() )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has invalid texture dimensions" );
                }

                uint32 imageDataLeft = actualImageDataLength;

                bool handledImageDataLengthError = false;

                for ( uint32 n = 0; n < maybeMipmapCount; n++ )
                {
                    bool hasEstablishedLevel = true;

                    if ( n != 0 )
                    {
                        hasEstablishedLevel = mipGen.incrementLevel();
                    }

                    if ( !hasEstablishedLevel )
                    {
                        break;
                    }

                    uint32 layerWidth = mipGen.getLevelWidth();
                    uint32 layerHeight = mipGen.getLevelHeight();

                    // Calculate the surface dimensions of this level.
                    uint32 surfWidth, surfHeight;
                    {
                        if ( internalFormat == GVRFMT_CMP )
                        {
                            // This is DXT1 compression.
                            surfWidth = ALIGN_SIZE( layerWidth, 4u );
                            surfHeight = ALIGN_SIZE( layerHeight, 4u );
                        }
                        else
                        {
                            surfWidth = layerWidth;
                            surfHeight = layerHeight;
                        }
                    }

                    // Calculate the actual mipmap data size.
                    uint32 texDataSize;
                    {
                        if ( internalFormat == GVRFMT_CMP )
                        {
                            texDataSize = getDXTRasterDataSize( 1, surfWidth * surfHeight );
                        }
                        else
                        {
                            uint32 texRowSize = getGCRasterDataRowSize( surfWidth, depth );

                            texDataSize = getRasterDataSizeByRowSize( texRowSize, surfHeight );
                        }
                    }

                    // Check whether we can read this image data.
                    if ( imageDataLeft < texDataSize )
                    {
                        // In this case we just halt reading and output a warning.
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has less image data bytes than the mipmaps require" );

                        handledImageDataLengthError = true;

                        break;
                    }

                    // Read the data.
                    gcNativeBlock.check_read_ahead( texDataSize );

                    void *texels = engineInterface->PixelAllocate( texDataSize );

                    if ( !texels )
                    {
                        throw RwException( "failed to allocate mipmap texel buffer for texture " + theTexture->GetName() );
                    }

                    try
                    {
                        gcNativeBlock.read( texels, texDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( texels );

                        throw;
                    }

                    // Update the read count of the image data section.
                    imageDataLeft -= texDataSize;

                    // Store this layer.
                    NativeTextureGC::mipmapLayer mipLayer;
                    mipLayer.width = surfWidth;
                    mipLayer.height = surfHeight;

                    mipLayer.layerWidth = layerWidth;
                    mipLayer.layerHeight = layerHeight;

                    mipLayer.texels = texels;
                    mipLayer.dataSize = texDataSize;

                    // Store it.
                    platformTex->mipmaps.push_back( mipLayer );

                    mipmapCount++;
                }

                // Make sure we cannot accept empty textures.
                if ( mipmapCount == 0 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has no mipmaps" );
                }

                // If we have not read all the image data, skip to the end.
                if ( imageDataLeft != 0 )
                {
                    if ( handledImageDataLengthError == false )
                    {
                        // We want to warn the user about this.
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has image data section meta data (ignoring)" );
                    }

                    inputProvider.skip( imageDataLeft );
                }

                // Fix filtering modes.
                fixFilteringMode( *theTexture, mipmapCount );

                // Done.
            }
            else
            {
                throw RwException( "unknown gamecube native block; expected chunk struct" );
            }
        }
        catch( ... )
        {
            gcNativeBlock.LeaveContext();

            throw;
        }

        gcNativeBlock.LeaveContext();
    }

    // Read the texture extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

struct gcMipmapManager
{
    NativeTextureGC *nativeTex;

    inline gcMipmapManager( NativeTextureGC *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureGC::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureGC::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        uint32& dstRowAlignment,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        // TODO.
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureGC::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        uint32 rowAlignment,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // TODO.
    }
};

bool gamecubeNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    return false;

    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    gcMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureGC::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool gamecubeNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, texNativeTypeProvider::acquireFeedback_t& feedbackOut )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    gcMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureGC::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void gamecubeNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureGC *nativeTex = (NativeTextureGC*)objMem;

    virtualClearMipmaps <NativeTextureGC::mipmapLayer> (
        engineInterface, nativeTex->mipmaps
    );
}

void gamecubeNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

    uint32 mipmapCount = (uint32)nativeTex->mipmaps.size();

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount != 0 )
    {
        const NativeTextureGC::mipmapLayer& mipLayer = nativeTex->mipmaps[ 0 ];

        baseWidth = mipLayer.layerWidth;
        baseHeight = mipLayer.layerHeight;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
    infoOut.mipmapCount = mipmapCount;
}

void gamecubeNativeTextureTypeProvider::GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const
{
    const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

    std::string formatName;

    // Set information about the color format.
    eGCNativeTextureFormat internalFormat = nativeTex->internalFormat;

    if ( internalFormat == GVRFMT_LUM_4BIT )
    {
        formatName += "LUM4";
    }
    else if ( internalFormat == GVRFMT_LUM_8BIT )
    {
        formatName += "LUM8";
    }
    else if ( internalFormat == GVRFMT_LUM_4BIT_ALPHA )
    {
        formatName += "LUM_A4";
    }
    else if ( internalFormat == GVRFMT_LUM_8BIT_ALPHA )
    {
        formatName += "LUM_A8";
    }
    else if ( internalFormat == GVRFMT_RGB565 )
    {
        formatName += "565";
    }
    else if ( internalFormat == GVRFMT_RGB5A3 )
    {
        formatName += "555A3";
    }
    else if ( internalFormat == GVRFMT_RGBA8888 )
    {
        formatName += "8888";
    }
    else if ( internalFormat == GVRFMT_PAL_4BIT ||
              internalFormat == GVRFMT_PAL_8BIT )
    {
        eGCPixelFormat palettePixelFormat = nativeTex->palettePixelFormat;

        if ( palettePixelFormat == GVRPIX_LUM_ALPHA )
        {
            formatName += "LUM_A8";
        }
        else if ( palettePixelFormat == GVRPIX_RGB565 )
        {
            formatName += "565";
        }
        else if ( palettePixelFormat == GVRPIX_RGB5A3 )
        {
            formatName += "555A3";
        }
        else
        {
            formatName += "unknown";
        }

        if ( internalFormat == GVRFMT_PAL_4BIT )
        {
            formatName += " PAL4";
        }
        else if ( internalFormat == GVRFMT_PAL_8BIT )
        {
            formatName += " PAL8";
        }
    }
    else if ( internalFormat == GVRFMT_CMP )
    {
        formatName += "DXT1";
    }
    else
    {
        formatName += "unknown";
    }

    size_t formatNameLen = formatName.size();

    if ( buf )
    {
        strncpy( buf, formatName.c_str(), formatNameLen );
    }

    lengthOut = formatNameLen;
}

};