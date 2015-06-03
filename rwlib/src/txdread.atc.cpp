#include <StdInc.h>

#include "pixelformat.hxx"

#include "txdread.atc.hxx"

#include "txdread.common.hxx"

#include "streamutil.hxx"

#include "pluginutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

void atcNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the native image struct.
    {
        BlockProvider texNativeImageStruct( &inputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            if ( texNativeImageStruct.getBlockID() == CHUNK_STRUCT )
            {
                atitc::textureNativeGenericHeader metaHeader;
                texNativeImageStruct.read( &metaHeader, sizeof(metaHeader) );

                uint32 platform = metaHeader.platformDescriptor;

                if (platform != PLATFORM_ATC)
                {
                    throw RwException( "invalid platform type in ATC texture reading" );
                }

                // Cast our native texture.
                NativeTextureATC *platformTex = (NativeTextureATC*)nativeTex;

                // Read the format info.
                metaHeader.formatInfo.parse( *theTexture );

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

                // Check the internal format for validity.
                eATCInternalFormat internalFormat = metaHeader.internalFormat;

                if ( internalFormat != ATC_RGB_AMD &&
                     internalFormat != ATC_RGBA_EXPLICIT_ALPHA_AMD &&
                     internalFormat != ATC_RGBA_INTERPOLATED_ALPHA_AMD )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has an invalid ATC compression type" );
                }

                platformTex->internalFormat = internalFormat;

                platformTex->hasAlpha = metaHeader.hasAlpha;

                // Store some unknowns.
                platformTex->unk1 = metaHeader.unk1;
                platformTex->unk2 = metaHeader.unk2;

                assert( metaHeader.unk1 == false );

                // Read the data sizes and keep track of how much we have read already.
                uint32 validImageStreamSize = metaHeader.imageSectionStreamSize;

                uint32 maybeMipmapCount = metaHeader.mipmapCount;

                // First comes a list of mipmap data sizes.
                // Read them into a temporary buffer.
                std::vector <uint32> mipDataSizes;

                mipDataSizes.resize( maybeMipmapCount );

                // Keep track of how much we have read.
                uint32 imageDataSectionSize = 0;

                for ( uint32 n = 0; n < maybeMipmapCount; n++ )
                {
                    uint32 dataSize = texNativeImageStruct.readUInt32();
                    
                    imageDataSectionSize += sizeof( uint32 );
                    imageDataSectionSize += dataSize;

                    mipDataSizes[ n ] = dataSize;
                }

                // Read the mipmap layers.
                uint32 compressionBlockSize = getATCCompressionBlockSize( internalFormat );

                if ( compressionBlockSize == 0 )
                {
                    throw RwException( "failed to determine compression block size for texture " + theTexture->GetName() );
                }

                mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

                if ( !mipLevelGen.isValidLevel() )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has invalid dimensions" );
                }

                uint32 mipmapCount = 0;

                for ( uint32 n = 0; n < maybeMipmapCount; n++ )
                {
                    bool couldEstablishLevel = true;

                    if ( n > 0 )
                    {
                        couldEstablishLevel = mipLevelGen.incrementLevel();
                    }

                    if ( !couldEstablishLevel )
                    {
                        break;
                    }

                    // Read the data.
                    NativeTextureATC::mipmapLayer newLayer;

                    newLayer.layerWidth = mipLevelGen.getLevelWidth();
                    newLayer.layerHeight = mipLevelGen.getLevelHeight();

                    uint32 texWidth = newLayer.layerWidth;
                    uint32 texHeight = newLayer.layerHeight;
                    {
                        // We are compressing in 4x4 blocks.
                        texWidth = ALIGN_SIZE( texWidth, 4u );
                        texHeight = ALIGN_SIZE( texHeight, 4u );
                    }

                    newLayer.width = texWidth;
                    newLayer.height = texHeight;

                    // Verify the data size.
                    uint32 compressedItemCount = ( texWidth * texHeight ) / 16;

                    uint32 texReqDataSize = ( compressedItemCount * compressionBlockSize );

                    uint32 actualDataSize = mipDataSizes[ n ];

                    if ( texReqDataSize != actualDataSize )
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has damaged mipmaps" );
                    }

                    // Add the layer.
                    newLayer.dataSize = texReqDataSize;

                    // Verify that we even have that much data in the stream.
                    texNativeImageStruct.check_read_ahead( texReqDataSize );

                    newLayer.texels = engineInterface->PixelAllocate( texReqDataSize );

                    try
                    {
                        texNativeImageStruct.read( newLayer.texels, texReqDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( newLayer.texels );

                        throw;
                    }

                    platformTex->mipmaps.push_back( newLayer );

                    // Increment our mipmap count.
                    mipmapCount++;
                }

                if ( mipmapCount == 0 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " is empty" );
                }

                // Fix filtering mode.
                fixFilteringMode( *theTexture, mipmapCount );

                // Increment past any mipmap data that we skipped.
                if ( mipmapCount < maybeMipmapCount )
                {
                    for ( uint32 n = mipmapCount; n < maybeMipmapCount; n++ )
                    {
                        uint32 skipDataSize = mipDataSizes[ n ];

                        texNativeImageStruct.skip( skipDataSize );
                    }
                }
            }
            else
            {
                engineInterface->PushWarning( "could not find image data struct inside of ATC texture native" );
            }
        }
        catch( ... )
        {
            texNativeImageStruct.LeaveContext();

            throw;
        }

        texNativeImageStruct.LeaveContext();
    }

    // Deserialize extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

inline ATI_TC_FORMAT getATITCFormatFromInternalFormat( eATCInternalFormat internalFormat )
{
    ATI_TC_FORMAT actualFormat;

    if ( internalFormat == ATC_RGB_AMD )
    {
        actualFormat = ATI_TC_FORMAT_ATC_RGB;
    }
    else if ( internalFormat == ATC_RGBA_EXPLICIT_ALPHA_AMD )
    {
        actualFormat = ATI_TC_FORMAT_ATC_RGBA_Explicit;
    }
    else if ( internalFormat == ATC_RGBA_INTERPOLATED_ALPHA_AMD )
    {
        actualFormat = ATI_TC_FORMAT_ATC_RGBA_Interpolated;
    }
    else
    {
        assert( 0 );
    }

    return actualFormat;
}

inline void getATITCFormatTargetParams(
    eATCInternalFormat internalFormat, ATI_TC_FORMAT& requiredOutputCodec,
    eRasterFormat& outputCodecRasterFormat, uint32& outputCodecDepth, eColorOrdering& outputCodecColorOrder
)
{
    if ( internalFormat == ATC_RGB_AMD )
    {
        requiredOutputCodec = ATI_TC_FORMAT_ARGB_8888;

        outputCodecRasterFormat = RASTER_888;
        outputCodecDepth = 32;
        outputCodecColorOrder = COLOR_BGRA;
    }
    else if ( internalFormat == ATC_RGBA_EXPLICIT_ALPHA_AMD ||
              internalFormat == ATC_RGBA_INTERPOLATED_ALPHA_AMD )
    {
        requiredOutputCodec = ATI_TC_FORMAT_ARGB_8888;

        outputCodecRasterFormat = RASTER_8888;
        outputCodecDepth = 32;
        outputCodecColorOrder = COLOR_BGRA;
    }
    else
    {
        assert( 0 );
    }
}

// Pixel API.
inline void DecompressATCMipmap(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, uint32 layerWidth, uint32 layerHeight, const void *srcTexels, uint32 srcDataSize,
    eRasterFormat atcRasterFormat, uint32 atcDepth, eColorOrdering atcColorOrder,
    eRasterFormat targetRasterFormat, uint32 targetDepth, eColorOrdering targetColorOrder,
    ATI_TC_FORMAT srcRasterATITCFormat, ATI_TC_FORMAT dstRasterATITCFormat,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    ATI_TC_Texture srcTexture;
    srcTexture.dwSize = sizeof( ATI_TC_Texture );
    srcTexture.dwWidth = mipWidth;
    srcTexture.dwHeight = mipHeight;
    srcTexture.dwPitch = 0;
    srcTexture.format = srcRasterATITCFormat;
    srcTexture.dwDataSize = srcDataSize;
    srcTexture.pData = (ATI_TC_BYTE*)srcTexels;

    ATI_TC_Texture dstTexture;
    dstTexture.dwSize = sizeof( ATI_TC_Texture );
    dstTexture.dwWidth = mipWidth;
    dstTexture.dwHeight = mipHeight;
    dstTexture.dwPitch = 0;
    dstTexture.format = dstRasterATITCFormat;
    dstTexture.dwDataSize = ATI_TC_CalculateBufferSize( &dstTexture );
    dstTexture.pData = (ATI_TC_BYTE*)engineInterface->PixelAllocate( dstTexture.dwDataSize );

    // Decompress, wazaaa!
    ATI_TC_ERROR atcErrorCode = ATI_TC_ConvertTexture( &srcTexture, &dstTexture, NULL, NULL, NULL, NULL );

    assert( atcErrorCode == ATI_TC_OK );

    // Put the texels into a format we want.
    void *dstTexels = dstTexture.pData;
    size_t dstDataSize = dstTexture.dwDataSize;

    if ( atcRasterFormat != targetRasterFormat || atcDepth != targetDepth || atcColorOrder != targetColorOrder || mipWidth != layerWidth || mipHeight != layerHeight )
    {
        void *atcTexels = dstTexels;

        if ( atcDepth != targetDepth || mipWidth != layerWidth || mipHeight != layerHeight )
        {
            uint32 texelUnitCount = ( layerWidth * layerHeight );

            dstDataSize = getRasterDataSize( texelUnitCount, targetDepth );

            dstTexels = engineInterface->PixelAllocate( dstDataSize );
        }

        for ( uint32 y = 0; y < layerHeight; y++ )
        {
            for ( uint32 x = 0; x < layerWidth; x++ )
            {
                uint8 r, g, b, a;

                uint32 srcColorIndex = PixelFormat::coord2index(x, y, mipWidth);

                bool gotColor = browsetexelcolor(atcTexels, PALETTE_NONE, NULL, 0, srcColorIndex, atcRasterFormat, atcColorOrder, atcDepth, r, g, b, a);

                if ( !gotColor )
                {
                    r = 0;
                    g = 0;
                    b = 0;
                    a = 0;
                }

                uint32 dstColorIndex = PixelFormat::coord2index(x, y, layerWidth);

                puttexelcolor(dstTexels, dstColorIndex, targetRasterFormat, targetColorOrder, targetDepth, r, g, b, a);
            }
        }

        if ( dstTexels != atcTexels )
        {
            engineInterface->PixelFree( atcTexels );
        }
    }

    dstTexelsOut = dstTexels;
    dstDataSizeOut = dstDataSize;
}

void atcNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    // Get properties of the compressed texture.
    eATCInternalFormat internalFormat = nativeTex->internalFormat;

    // Decompress the texels into a good format.
    eRasterFormat targetRasterFormat = RASTER_8888;
    uint32 targetDepth = 32;
    eColorOrdering targetColorOrder = COLOR_RGBA;

    uint32 mipmapCount = nativeTex->mipmaps.size();

    pixelsOut.mipmaps.resize( mipmapCount );

    // Calculate the properties of the ATI TC source and destination textures.
    ATI_TC_FORMAT srcRasterATITCFormat = getATITCFormatFromInternalFormat( internalFormat );

    // Fetch format properties that are required as decompression destination surface.
    eRasterFormat atcRasterFormat = RASTER_8888;
    uint32 atcDepth = 32;
    eColorOrdering atcColorOrder = COLOR_RGBA;

    ATI_TC_FORMAT dstRasterATITCFormat;

    getATITCFormatTargetParams( internalFormat, dstRasterATITCFormat, atcRasterFormat, atcDepth, atcColorOrder );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureATC::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

        // Get important properties onto the stack.
        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        pixelDataTraversal::mipmapResource newLayer;

        // We will end up with an uncompressed texture, so the layer dimensions match the raw dimensions.
        newLayer.width = layerWidth;
        newLayer.height = layerHeight;

        newLayer.mipWidth = layerWidth;
        newLayer.mipHeight = layerHeight;

        // Decompress now.
        size_t texDataSize = 0;
        void *mipTexels = NULL;
        
        DecompressATCMipmap(
            engineInterface,
            mipWidth, mipHeight, layerWidth, layerHeight, mipLayer.texels, mipLayer.dataSize,
            atcRasterFormat, atcDepth, atcColorOrder,
            targetRasterFormat, targetDepth, targetColorOrder,
            srcRasterATITCFormat, dstRasterATITCFormat,
            mipTexels, texDataSize
        );

        // Apply the data to the mipmap layer.
        newLayer.texels = mipTexels;
        newLayer.dataSize = texDataSize;

        // Store the layer.
        pixelsOut.mipmaps[ n ] = newLayer;
    }

    // Give the raster format to the runtime.
    pixelsOut.rasterFormat = targetRasterFormat;
    pixelsOut.depth = targetDepth;
    pixelsOut.colorOrder = targetColorOrder;
    pixelsOut.paletteType = PALETTE_NONE;
    pixelsOut.paletteData = NULL;
    pixelsOut.paletteSize = 0;
    pixelsOut.hasAlpha = nativeTex->hasAlpha;
    
    pixelsOut.cubeTexture = false;
    pixelsOut.autoMipmaps = false;
    pixelsOut.rasterType = 4;

    pixelsOut.compressionType = RWCOMPRESS_NONE;

    // Since we decompress our texels, we are always newly allocated.
    pixelsOut.isNewlyAllocated = true;
}

inline void CompressMipmapToATC(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, const void *srcTexels,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder, ePaletteType srcPaletteType, const void *srcPaletteData, uint32 srcPaletteSize,
    eRasterFormat feedRasterFormat, uint32 feedDepth, eColorOrdering feedColorOrder,
    uint32 compressionBlockSize,
    ATI_TC_FORMAT srcTextureFormat, ATI_TC_FORMAT dstTextureFormat,
    uint32& dstWidthOut, uint32& dstHeightOut,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    // Put this mipmap in a ATI_Compress compatible texture.
    uint32 srcLayerTexUnitCount = ( mipWidth * mipHeight );

    uint32 srcTextureDataSize = getRasterDataSize( srcLayerTexUnitCount, feedDepth );

    void *feedTexels = engineInterface->PixelAllocate( srcTextureDataSize );

    for ( uint32 index = 0; index < srcLayerTexUnitCount; index++ )
    {
        uint8 r, g, b, a;

        bool gotColor = browsetexelcolor(
            srcTexels, srcPaletteType, srcPaletteData, srcPaletteSize, index,
            srcRasterFormat, srcColorOrder, srcDepth,
            r, g, b, a
        );

        if ( !gotColor )
        {
            r = 0;
            g = 0;
            b = 0;
            a = 0;
        }

        puttexelcolor(feedTexels, index, feedRasterFormat, feedColorOrder, feedDepth, r, g, b, a);
    }

    // Determine the compressed texture dimensions.
    uint32 compressWidth = ALIGN_SIZE( mipWidth, 4u );
    uint32 compressHeight = ALIGN_SIZE( mipHeight, 4u );

    uint32 compressionBlockCount = ( compressWidth * compressHeight ) / 16;

    // Allocate the output buffer.
    size_t dstDataSize = ( compressionBlockCount * compressionBlockSize );

    void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

    // Compress the texture now.
    {
        ATI_TC_Texture srcTexture;
        srcTexture.dwSize = sizeof( srcTexture );
        srcTexture.dwWidth = mipWidth;
        srcTexture.dwHeight = mipHeight;
        srcTexture.dwPitch = 0;
        srcTexture.format = srcTextureFormat;
        srcTexture.dwDataSize = srcTextureDataSize;
        srcTexture.pData = (ATI_TC_BYTE*)feedTexels;

        ATI_TC_Texture dstTexture;
        dstTexture.dwSize = sizeof( dstTexture );
        dstTexture.dwWidth = mipWidth;
        dstTexture.dwHeight = mipHeight;
        dstTexture.dwPitch = 0;
        dstTexture.format = dstTextureFormat;
        dstTexture.dwDataSize = dstDataSize;
        dstTexture.pData = (ATI_TC_BYTE*)dstTexels;

        // Do the conversion.
        ATI_TC_ERROR atcErrorCode =
            ATI_TC_ConvertTexture( &srcTexture, &dstTexture, NULL, NULL, NULL, NULL );

        assert( atcErrorCode == ATI_TC_OK );
    }

    // Free the feedTexture that was used as a proxy.
    engineInterface->PixelFree( feedTexels );

    // Give parameters to the runtime.
    dstWidthOut = compressWidth;
    dstHeightOut = compressHeight;
    dstTexelsOut = dstTexels;
    dstDataSizeOut = dstDataSize;
}

void atcNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    // We expect raw bitmaps here.
    assert( pixelsIn.compressionType == RWCOMPRESS_NONE );

    // Free any image data that may have been there.
    //nativeTex->clearImageData();

    // Get the pixel properties onto stack, as we have to compress them.
    eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
    uint32 srcDepth = pixelsIn.depth;
    eColorOrdering srcColorOrder = pixelsIn.colorOrder;
    ePaletteType srcPaletteType = pixelsIn.paletteType;
    const void *srcPaletteData = pixelsIn.paletteData;
    uint32 srcPaletteSize = pixelsIn.paletteSize;

    bool hasAlpha = pixelsIn.hasAlpha;

    // Determine how to compress the texture.
    eATCInternalFormat internalFormat = ATC_RGB_AMD;

    if ( hasAlpha )
    {
        internalFormat = ATC_RGBA_EXPLICIT_ALPHA_AMD;
    }
    else
    {
        internalFormat = ATC_RGB_AMD;
    }

    // Do it.
    {
        // Get the format that we will output the feed-in texture as.
        eRasterFormat feedRasterFormat = RASTER_8888;
        uint32 feedDepth = 32;
        eColorOrdering feedColorOrder = COLOR_BGRA;

        ATI_TC_FORMAT srcTextureFormat;

        getATITCFormatTargetParams( internalFormat, srcTextureFormat, feedRasterFormat, feedDepth, feedColorOrder );

        ATI_TC_FORMAT dstTextureFormat = getATITCFormatFromInternalFormat( internalFormat );

        uint32 compressionBlockSize = getATCCompressionBlockSize( internalFormat );

        // Parse all mipmaps.
        uint32 mipmapCount = pixelsIn.mipmaps.size();

        nativeTex->mipmaps.resize( mipmapCount );

        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            const pixelDataTraversal::mipmapResource& mipLayer = pixelsIn.mipmaps[ n ];

            // Get imporant properties onto stack.
            uint32 mipWidth = mipLayer.width;
            uint32 mipHeight = mipLayer.height;

            const void *srcTexels = mipLayer.texels;

            // Compress the level.
            uint32 compressWidth, compressHeight;

            void *dstTexels = NULL;
            uint32 dstDataSize = 0;

            CompressMipmapToATC(
                engineInterface,
                mipWidth, mipHeight, srcTexels,
                srcRasterFormat, srcDepth, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize,
                feedRasterFormat, feedDepth, feedColorOrder,
                compressionBlockSize,
                srcTextureFormat, dstTextureFormat,
                compressWidth, compressHeight,
                dstTexels, dstDataSize
            );

            // Create a new mipmap layer with the texel information.
            NativeTextureATC::mipmapLayer newLayer;

            newLayer.width = compressWidth;
            newLayer.height = compressHeight;

            newLayer.layerWidth = mipLayer.mipWidth;
            newLayer.layerHeight = mipLayer.mipHeight;

            newLayer.dataSize = dstDataSize;
            newLayer.texels = dstTexels;

            // Store this new layer.
            nativeTex->mipmaps[ n ] = newLayer;
        }
    }

    // Store texture properties.
    nativeTex->internalFormat = internalFormat;
    nativeTex->hasAlpha = hasAlpha;
    nativeTex->unk1 = false;
    nativeTex->unk2 = 0;

    // Since we compress from the pixels, we cannot directly acquire.
    feedbackOut.hasDirectlyAcquired = false;
}

void atcNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    // Remove texel links from this texture.
    if ( deallocate )
    {
        // Delete mipmap layers.
        deleteMipmapLayers( engineInterface, nativeTex->mipmaps );
    }

    // Clear the mipmap layers.
    nativeTex->mipmaps.clear();

    // Reset our format properties for cleanlyness sake.
    nativeTex->internalFormat = ATC_RGB_AMD;
    nativeTex->hasAlpha = false;
}

struct atcMipmapManager
{
    NativeTextureATC *nativeTex;

    inline atcMipmapManager( NativeTextureATC *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureATC::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureATC::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {
        eATCInternalFormat internalFormat = nativeTex->internalFormat;

        uint32 mipWidth = mipLayer.width;
        uint32 mipHeight = mipLayer.height;

        uint32 layerWidth = mipLayer.layerWidth;
        uint32 layerHeight = mipLayer.layerHeight;

        const void *srcTexels = mipLayer.texels;
        uint32 srcDataSize = mipLayer.dataSize;

        // Decompress the texels into a good format.
        eRasterFormat targetRasterFormat = RASTER_8888;
        uint32 targetDepth = 32;
        eColorOrdering targetColorOrder = COLOR_RGBA;

        // We decompress the layer and give it as new texels.
        eRasterFormat atcRasterFormat;
        uint32 atcDepth;
        eColorOrdering atcColorOrder;

        ATI_TC_FORMAT dstRasterATITCFormat;

        getATITCFormatTargetParams( internalFormat, dstRasterATITCFormat, atcRasterFormat, atcDepth, atcColorOrder );

        ATI_TC_FORMAT srcRasterATITCFormat = getATITCFormatFromInternalFormat( internalFormat );

        // Perform it.
        void *dstTexels = NULL;
        uint32 dstDataSize = 0;

        DecompressATCMipmap(
            engineInterface,
            mipWidth, mipHeight, layerWidth, layerHeight, srcTexels, srcDataSize,
            atcRasterFormat, atcDepth, atcColorOrder,
            targetRasterFormat, targetDepth, targetColorOrder,
            srcRasterATITCFormat, dstRasterATITCFormat,
            dstTexels, dstDataSize
        );

        // Give to the runtime.
        widthOut = layerWidth;
        heightOut = layerHeight;
        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstRasterFormat = targetRasterFormat;
        dstDepth = targetDepth;
        dstColorOrder = targetColorOrder;

        dstPaletteType = PALETTE_NONE;
        dstPaletteData = NULL;
        dstPaletteSize = 0;

        dstCompressionType = RWCOMPRESS_NONE;

        hasAlpha = nativeTex->hasAlpha;

        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;

        isNewlyAllocatedOut = true;
        isPaletteNewlyAllocated = false;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureATC::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // We need to compress the same way as the texture is compressed as.
        eATCInternalFormat internalFormat = nativeTex->internalFormat;

        // If the input is not in raw bitmap format, convert it to raw format.
        bool srcTexelsNewlyAllocated = false;

        if ( compressionType != RWCOMPRESS_NONE )
        {
            eRasterFormat targetRasterFormat = RASTER_8888;
            uint32 targetDepth = 32;
            eColorOrdering targetColorOrder = COLOR_BGRA;

            bool hasChanged =
                ConvertMipmapLayerNative(
                    engineInterface,
                    width, height, layerWidth, layerHeight, srcTexels, dataSize,
                    rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                    targetRasterFormat, targetDepth, targetColorOrder, PALETTE_NONE, NULL, 0, RWCOMPRESS_NONE,
                    false,
                    width, height,
                    srcTexels, dataSize
                );

            if ( hasChanged == false )
            {
                throw RwException( "failed to decompress in ATC native texture mipmap manager" );
            }

            // We are now in raw format.
            compressionType = RWCOMPRESS_NONE;

            rasterFormat = targetRasterFormat;
            depth = targetDepth;
            colorOrder = targetColorOrder;

            paletteType = PALETTE_NONE;
            paletteData = NULL;
            paletteSize = 0;

            srcTexelsNewlyAllocated = true;
        }

        // Get the format that we will output the feed-in texture as.
        eRasterFormat feedRasterFormat = RASTER_8888;
        uint32 feedDepth = 32;
        eColorOrdering feedColorOrder = COLOR_BGRA;

        ATI_TC_FORMAT srcTextureFormat;

        getATITCFormatTargetParams( internalFormat, srcTextureFormat, feedRasterFormat, feedDepth, feedColorOrder );

        ATI_TC_FORMAT dstTextureFormat = getATITCFormatFromInternalFormat( internalFormat );

        uint32 compressionBlockSize = getATCCompressionBlockSize( internalFormat );

        // Do it.
        uint32 compressedWidth, compressedHeight;

        void *dstTexels = NULL;
        uint32 dstDataSize = 0;

        CompressMipmapToATC(
            engineInterface,
            width, height, srcTexels,
            rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
            feedRasterFormat, feedDepth, feedColorOrder,
            compressionBlockSize,
            srcTextureFormat, dstTextureFormat,
            compressedWidth, compressedHeight,
            dstTexels, dstDataSize
        );

        if ( srcTexelsNewlyAllocated )
        {
            engineInterface->PixelFree( srcTexels );
        }

        // Store this new layer.
        mipLayer.width = compressedWidth;
        mipLayer.height = compressedHeight;

        mipLayer.layerWidth = layerWidth;
        mipLayer.layerHeight = layerHeight;

        mipLayer.texels = dstTexels;
        mipLayer.dataSize = dstDataSize;

        hasDirectlyAcquiredOut = false;
    }
};

bool atcNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    atcMipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTextureATC::mipmapLayer> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool atcNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    atcMipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTextureATC::mipmapLayer> (
            engineInterface, mipMan,
            nativeTex->mipmaps, layerIn,
            feedbackOut
        );
}

void atcNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    virtualClearMipmaps <NativeTextureATC::mipmapLayer> ( engineInterface, nativeTex->mipmaps );
}

void atcNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTextureATC *nativeTex = (NativeTextureATC*)objMem;

    uint32 mipmapCount = nativeTex->mipmaps.size();

    infoOut.mipmapCount = mipmapCount;

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount > 0 )
    {
        baseWidth = nativeTex->mipmaps[ 0 ].layerWidth;
        baseHeight = nativeTex->mipmaps[ 0 ].layerHeight;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
}

static PluginDependantStructRegister <atcNativeTextureTypeProvider, RwInterfaceFactory_t> atcNativeTexturePluginStore( engineFactory );

void registerATCNativeTexture( Interface *engineInterface )
{
    atcNativeTextureTypeProvider *atcTexEnv = atcNativeTexturePluginStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( atcTexEnv )
    {
        // Register it.
        RegisterNativeTextureType( engineInterface, "ATI_Compress", atcTexEnv, sizeof( NativeTextureATC ) );
    }
}

};