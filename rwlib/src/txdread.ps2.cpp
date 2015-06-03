#include <StdInc.h>

#include <bitset>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>
#include <algorithm>
#include <cmath>

#include "txdread.ps2.hxx"
#include "txdread.d3d.hxx"

#include "pixelformat.hxx"

#include "txdread.ps2gsman.hxx"

#include "txdread.common.hxx"

#include "pluginutil.hxx"

#include "txdread.miputil.hxx"

namespace rw
{

inline void verifyTexture( const NativeTexturePS2::GSTexture& gsTex, bool hasHeaders, eFormatEncodingType currentEncodingType, eFormatEncodingType imageDecodeFormatType, ps2MipmapTransmissionData& transmissionOffset )
{
    // If the texture had the headers, then it should have come with the required registers.
    if ( hasHeaders )
    {
        // Debug register contents (only the important ones).
        uint32 regCount = gsTex.storedRegs.size();

        bool hasTRXPOS = false;
        bool hasTRXREG = false;
        bool hasTRXDIR = false;

        for ( uint32 n = 0; n < regCount; n++ )
        {
            const NativeTexturePS2::GSTexture::GSRegInfo& regInfo = gsTex.storedRegs[ n ];

            if ( regInfo.regID == GIF_REG_TRXPOS )
            {
                // TRXPOS
                const ps2GSRegisters::TRXPOS_REG& trxpos = (const ps2GSRegisters::TRXPOS_REG&)regInfo.content;

                assert(trxpos.ssax == 0);
                assert(trxpos.ssay == 0);
                assert(trxpos.dir == 0);

                // Give the transmission settings to the runtime.
                transmissionOffset.destX = trxpos.dsax;
                transmissionOffset.destY = trxpos.dsay;

                hasTRXPOS = true;
            }
            else if ( regInfo.regID == GIF_REG_TRXREG )
            {
                // TRXREG
                const ps2GSRegisters::TRXREG_REG& trxreg = (const ps2GSRegisters::TRXREG_REG&)regInfo.content;

                // Convert this to swizzle width and height.
                uint32 storedSwizzleWidth = trxreg.transmissionAreaWidth;
                uint32 storedSwizzleHeight = trxreg.transmissionAreaHeight;

                if (currentEncodingType == FORMAT_TEX32 && imageDecodeFormatType == FORMAT_IDTEX8_COMPRESSED)
                {
                    storedSwizzleWidth /= 2;
                }

                assert(storedSwizzleWidth == gsTex.swizzleWidth);
                assert(storedSwizzleHeight == gsTex.swizzleHeight);

                hasTRXREG = true;
            }
            else if ( regInfo.regID == GIF_REG_TRXDIR )
            {
                // TRXDIR
                const ps2GSRegisters::TRXDIR_REG& trxdir = (const ps2GSRegisters::TRXDIR_REG&)regInfo.content;

                // Textures have to be transferred to the GS memory.
                assert(trxdir.xdir == 0);

                hasTRXDIR = true;
            }
        }

        // We kinda require all registers.
        assert(hasTRXPOS == true);
        assert(hasTRXREG == true);
        assert(hasTRXDIR == true);
    }
}

uint32 NativeTexturePS2::GSTexture::readGIFPacket(Interface *engineInterface, BlockProvider& inputProvider, bool hasHeaders, bool& corruptedHeaders_out)
{
    // See https://www.dropbox.com/s/onjaprt82y81sj7/EE_Users_Manual.pdf page 151

    uint32 readCount = 0;

	if (hasHeaders)
    {
        // A GSTexture always consists of a register list and the image data.
        struct invalid_gif_exception
        {
        };

        bool corruptedHeaders = false;

        int64 streamOff_safe = inputProvider.tell();

        uint32 gif_readCount = 0;

        try
        {
            {
                GIFtag regListTag;
                inputProvider.read( &regListTag, sizeof(regListTag) );

                gif_readCount += sizeof(regListTag);

                // If we have a register list, parse it.
                if (regListTag.flg == 0)
                {
                    if (regListTag.eop != false ||
                        regListTag.pre != false ||
                        regListTag.prim != 0)
                    {
                        throw invalid_gif_exception();
                    }

                    // Only allow the register list descriptor.
                    if (regListTag.nreg != 1 ||
                        regListTag.getRegisterID(0) != 0xE)
                    {
                        throw invalid_gif_exception();
                    }

                    uint32 numRegs = regListTag.nloop;

                    // Preallocate the register space.
                    this->storedRegs.resize( numRegs );

                    for ( uint32 n = 0; n < numRegs; n++ )
                    {
                        // Read the register content.
                        unsigned long long regContent;
                        inputProvider.read( &regContent, sizeof(regContent) );
                        
                        // Read the register ID.
                        struct regID_struct
                        {
                            unsigned long long regID : 8;
                            unsigned long long pad1 : 56;
                        };

                        regID_struct regID;
                        inputProvider.read( &regID, sizeof(regID_struct) );

                        // Put the register into the register storage.
                        GSRegInfo& regInfo = this->storedRegs[ n ];

                        regInfo.regID = (eGSRegister)regID.regID;
                        regInfo.content = regContent;
                    }

                    gif_readCount += numRegs * ( sizeof(unsigned long long) * 2 );
                }
                else
                {
                    throw invalid_gif_exception();
                }
            }

            // Read the image data GIFtag.
            {
                GIFtag imgDataTag;
                inputProvider.read( &imgDataTag, sizeof(imgDataTag) );

                gif_readCount += sizeof(imgDataTag);

                // Verify that this is an image data tag.
                if (imgDataTag.eop != false ||
                    imgDataTag.pre != false ||
                    imgDataTag.prim != 0 ||
                    imgDataTag.flg != 2 ||
                    imgDataTag.nreg != 0)
                {
                    throw invalid_gif_exception();
                }

                // Verify the image data size.
                if (imgDataTag.nloop != (this->dataSize / (sizeof(uint64) * 2)))
                {
                    throw invalid_gif_exception();
                }
            }
        }
        catch( invalid_gif_exception& )
        {
            // We ignore the headers and try to read the image data.
            inputProvider.seek( streamOff_safe + 0x50, RWSEEK_BEG );

            gif_readCount = 0x50;

            corruptedHeaders = true;
        }

        readCount += gif_readCount;

        corruptedHeaders_out = corruptedHeaders;
	}

    uint32 texDataSize = this->dataSize;

    void *texelData = NULL;

    if ( texDataSize != 0 )
    {
        // Check that we even have that much data in the stream.
        inputProvider.check_read_ahead( texDataSize );

        texelData = engineInterface->PixelAllocate( texDataSize );

        try
        {
            inputProvider.read( texelData, texDataSize );
        }
        catch( ... )
        {
            engineInterface->PixelFree( texelData );

            throw;
        }

        readCount += texDataSize;
    }

    this->texels = texelData;

    return readCount;
}

inline void readStringChunk( Interface *engineInterface, std::string& stringOut, BlockProvider& inputProvider )
{
    BlockProvider stringBlock( &inputProvider );

    stringBlock.EnterContext();

    try
    {
        if ( stringBlock.getBlockID() == CHUNK_STRING )
        {
            int64 chunkLength = stringBlock.getBlockLength();

            if ( chunkLength < 0x100000000L )
            {
                size_t strLen = (size_t)chunkLength;

                char *buffer = new char[strLen+1];
                stringBlock.read(buffer, strLen);

                buffer[strLen] = '\0';

                stringOut = buffer;

                delete[] buffer;
            }
            else
            {
                engineInterface->PushWarning( "too long string in string chunk" );
            }
        }
        else
        {
            engineInterface->PushWarning( "could not find string chunk" );
        }
    }
    catch( ... )
    {
        stringBlock.LeaveContext();

        throw;
    }

    stringBlock.LeaveContext();
}

eTexNativeCompatibility ps2NativeTextureTypeProvider::IsCompatibleTextureBlock( BlockProvider& inputProvider ) const
{
    eTexNativeCompatibility returnCompat = RWTEXCOMPAT_NONE;

    // Go into the master header.
    BlockProvider texNativeMasterHeader( &inputProvider );

    texNativeMasterHeader.EnterContext();

    try
    {
        if ( texNativeMasterHeader.getBlockID() == CHUNK_STRUCT )
        {
            // We simply verify the checksum.
            // If it matches, we believe it definately is a PS2 texture.
            uint32 checksum = texNativeMasterHeader.readUInt32();

            if ( checksum == PLATFORM_PS2FOURCC )
            {
                returnCompat = RWTEXCOMPAT_ABSOLUTE;
            }
        }
    }
    catch( ... )
    {
        texNativeMasterHeader.LeaveContext();

        throw;
    }

    texNativeMasterHeader.LeaveContext();

    // We are not a PS2 texture for whatever reason.
    return returnCompat;
}

void ps2NativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    // Read the PS2 master header struct.
    {
        BlockProvider texNativeMasterHeader( &inputProvider );

        texNativeMasterHeader.EnterContext();

        try
        {
            if ( texNativeMasterHeader.getBlockID() == CHUNK_STRUCT )
            {
	            uint32 checksum = texNativeMasterHeader.readUInt32();

	            if (checksum != PLATFORM_PS2FOURCC)
                {
                    throw RwException( "invalid platform for PS2 texture reading" );
                }

                uint32 engineWarningLevel = engineInterface->GetWarningLevel();

                texFormatInfo formatInfo;
                texNativeMasterHeader.read( &formatInfo, sizeof(formatInfo) );

                // Read texture format.
                formatInfo.parse( *theTexture );
            }
            else
            {
                throw RwException( "could not find texture native master header struct for PS2 texture native" );
            }
        }
        catch( ... )
        {
            texNativeMasterHeader.LeaveContext();

            throw;
        }

        texNativeMasterHeader.LeaveContext();
    }

    int engineWarningLevel = engineInterface->GetWarningLevel();

    // Cast our native texture.
    NativeTexturePS2 *platformTex = (NativeTexturePS2*)nativeTex;

    // Read the name chunk section.
    {
        std::string nameOut;

        readStringChunk( engineInterface, nameOut, inputProvider );

        theTexture->SetName( nameOut.c_str() );
    }

    // Read the mask name chunk section.
    {
        std::string nameOut;

        readStringChunk( engineInterface, nameOut, inputProvider );

        theTexture->SetMaskName( nameOut.c_str() );
    }

    // Absolute maximum of mipmaps.
    const size_t maxMipmaps = 7;

    // Graphics Synthesizer package struct.
    {
        BlockProvider gsNativeBlock( &inputProvider );

        gsNativeBlock.EnterContext();

        try
        {
            if ( gsNativeBlock.getBlockID() == CHUNK_STRUCT )
            {
                // Texture Meta Struct.
                textureMetaDataHeader textureMeta;
                {
                    BlockProvider textureMetaChunk( &gsNativeBlock );

                    textureMetaChunk.EnterContext();

                    try
                    {
                        if ( textureMetaChunk.getBlockID() == CHUNK_STRUCT )
                        {
                            // Read it.
                            textureMetaChunk.read( &textureMeta, sizeof( textureMeta ) );
                        }
                        else
                        {
                            throw RwException( "could not find texture meta information struct in PS2 texture native" );
                        }
                    }
                    catch( ... )
                    {
                        textureMetaChunk.LeaveContext();

                        throw;
                    }

                    textureMetaChunk.LeaveContext();
                }
                
                uint32 depth = textureMeta.depth;

                // Deconstruct the rasterFormat.
                bool hasMipmaps = false;        // TODO: actually use this flag.

                readRasterFormatFlags( textureMeta.rasterFormat, platformTex->rasterFormat, platformTex->paletteType, hasMipmaps, platformTex->autoMipmaps );

                // Verify the raster format.
                eRasterFormat rasterFormat = platformTex->rasterFormat;

                if ( !isValidRasterFormat( rasterFormat ) )
                {
                    throw RwException( "invalid raster format in PS2 texture" );
                }

                // Verify the texture depth.
                {
                    uint32 texDepth = 0;

                    if (platformTex->paletteType == PALETTE_4BIT)
                    {
                        texDepth = 4;
                    }
                    else if (platformTex->paletteType == PALETTE_8BIT)
                    {
                        texDepth = 8;
                    }
                    else
                    {
                        texDepth = Bitmap::getRasterFormatDepth(rasterFormat);
                    }

                    if (texDepth != depth)
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has an invalid depth" );
                    }
                }

                platformTex->requiresHeaders = ( textureMeta.rasterFormat & 0x20000 ) != 0;
                platformTex->hasSwizzle = ( textureMeta.rasterFormat & 0x10000 ) != 0;

                // Store the raster type.
                platformTex->rasterType = ( textureMeta.rasterFormat & 0xFF );

                platformTex->depth = depth;

                // Store unique parameters from the texture registers.
                platformTex->gsParams.maxMIPLevel = textureMeta.tex1.maximumMIPLevel;
                platformTex->gsParams.mtba = textureMeta.tex1.mtba;
                platformTex->gsParams.textureFunction = textureMeta.tex0.texFunction;
                platformTex->gsParams.lodCalculationModel = textureMeta.tex1.lodCalculationModel;
                platformTex->gsParams.mmag = textureMeta.tex1.mmag;
                platformTex->gsParams.mmin = textureMeta.tex1.mmin;
                platformTex->gsParams.lodParamL = textureMeta.tex1.lodParamL;
                platformTex->gsParams.lodParamK = textureMeta.tex1.lodParamK;

                platformTex->gsParams.gsTEX1Unknown1 = textureMeta.tex1.unknown;
                platformTex->gsParams.gsTEX1Unknown2 = textureMeta.tex1.unknown2;
                
                // If we are on the GTA III engine, we need to store the recommended buffer base pointer.
                LibraryVersion libVer = inputProvider.getBlockVersion();

                if (libVer.rwLibMinor <= 3)
                {
                    platformTex->recommendedBufferBasePointer = textureMeta.tex0.textureBasePointer;
                }

                uint32 dataSize = textureMeta.dataSize;
                uint32 paletteDataSize = textureMeta.paletteDataSize;

                platformTex->skyMipMapVal = textureMeta.skyMipmapVal;

                // 0x00000 means the texture is not swizzled and has no headers
                // 0x10000 means the texture is swizzled and has no headers
                // 0x20000 means swizzling information is contained in the header
                // the rest is the same as the generic raster format
                bool hasHeader = platformTex->requiresHeaders;
                bool hasSwizzle = platformTex->hasSwizzle;

                platformTex->hasAlpha = false;

                if (theTexture->GetMaskName().empty() == false || depth == 16)
                {
                    platformTex->hasAlpha = true;
                }

                // GS packet struct.
                {
                    BlockProvider gsPacketBlock( &gsNativeBlock );
                    
                    gsPacketBlock.EnterContext();

                    try
                    {
                        if ( gsPacketBlock.getBlockID() == CHUNK_STRUCT )
                        {
                            // Decide about texture properties.
                            eFormatEncodingType imageEncodingType = platformTex->getHardwareRequiredEncoding(inputProvider.getBlockVersion());

                            // Get the format we should decode to.
                            eFormatEncodingType actualEncodingType = getFormatEncodingFromRasterFormat(rasterFormat, platformTex->paletteType);
                            
                            if (imageEncodingType == FORMAT_UNKNOWN)
                            {
                                throw RwException( "unknown image decoding format" );
                            }
                            if (actualEncodingType == FORMAT_UNKNOWN)
                            {
                                throw RwException( "unknown image encoding format" );
                            }

                            platformTex->swizzleEncodingType = imageEncodingType;

                            // Reserve that much space for mipmaps.
                            platformTex->mipmaps.reserve( maxMipmaps );

                            ps2MipmapTransmissionData _origMipmapTransData[ maxMipmaps ];

                            // TODO: are PS2 rasters always RGBA?
                            // If not, adjust the color order parameter!

                            /* Pixels/Indices */
                            int64 end = gsPacketBlock.tell();
                            end += (long)dataSize;
                            uint32 i = 0;

                            long remainingImageData = dataSize;

                            mipGenLevelGenerator mipLevelGen( textureMeta.width, textureMeta.height );

                            if ( !mipLevelGen.isValidLevel() )
                            {
                                throw RwException( "texture " + theTexture->GetName() + " has invalid dimensions" );
                            }

                            while (gsPacketBlock.tell() < end)
                            {
                                if (i == maxMipmaps)
                                {
                                    // We cannot have more than the maximum mipmaps.
                                    break;
                                }

                                if (i > 0 && !hasMipmaps)
                                {
                                    break;
                                }

                                // half dimensions if we have mipmaps
                                bool couldEstablishMipmap = true;

                                if (i > 0)
                                {
                                    couldEstablishMipmap = mipLevelGen.incrementLevel();
                                }

                                if ( !couldEstablishMipmap )
                                {
                                    break;
                                }

                                // Create a new mipmap.
                                platformTex->mipmaps.resize( i + 1 );

                                NativeTexturePS2::GSMipmap& newMipmap = platformTex->mipmaps[i];

                                newMipmap.width = mipLevelGen.getLevelWidth();
                                newMipmap.height = mipLevelGen.getLevelHeight();

                                // Calculate the encoded dimensions.
                                {
                                    uint32 packedWidth, packedHeight;

                                    bool gotPackedDimms =
                                        ps2GSPixelEncodingFormats::getPackedFormatDimensions(
                                            actualEncodingType, imageEncodingType,
                                            newMipmap.width, newMipmap.height,
                                            packedWidth, packedHeight
                                        );

                                    if ( gotPackedDimms == false )
                                    {
                                        throw RwException( "failed to get encoded dimensions for mipmap" );
                                    }

                                    newMipmap.swizzleWidth = packedWidth;
                                    newMipmap.swizzleHeight = packedHeight;
                                }

                                // Calculate the texture data size.
                                newMipmap.dataSize = newMipmap.getDataSize( imageEncodingType );

                                // Read the GIF packet data.
                                bool hasCorruptedHeaders = false;

                                uint32 readCount = newMipmap.readGIFPacket(engineInterface, gsPacketBlock, hasHeader, hasCorruptedHeaders);

                                remainingImageData -= readCount;

                                if ( !hasCorruptedHeaders )
                                {
                                    // Verify this mipmap.
                                    verifyTexture( newMipmap, hasHeader, imageEncodingType, actualEncodingType, _origMipmapTransData[i] );
                                }
                                else
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has corrupted image GIF packets" );
                                }

                                i++;
                            }

                            // Assume we have at least one texture.
                            if ( platformTex->mipmaps.size() == 0 )
                            {
                                throw RwException( "empty texture" );
                            }

                            if ( remainingImageData > 0 )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has image meta data" );

                                // Make sure we are past the image data.
                                gsPacketBlock.skip( remainingImageData );
                            }

                            /* Palette */
                            // vc dyn_trash.txd is weird here
                            ps2MipmapTransmissionData palTransData;
                            bool hasPalTransData = false;

                            long remainingPaletteData = textureMeta.paletteDataSize;

                            if (platformTex->paletteType != PALETTE_NONE)
                            {
                                // Craft the palette texture.
                                NativeTexturePS2::GSTexture& palTex = platformTex->paletteTex;

                                // The dimensions of this texture depend on game version.
                                getPaletteTextureDimensions(platformTex->paletteType, gsPacketBlock.getBlockVersion(), palTex.swizzleWidth, palTex.swizzleHeight);

                                // Decide about encoding type.
                                // Only a limited amount of types are truly supported.
                                eFormatEncodingType palEncodingType = getFormatEncodingFromRasterFormat(rasterFormat, PALETTE_NONE);

                                if (palEncodingType != FORMAT_TEX32 && palEncodingType != FORMAT_TEX16)
                                {
                                    throw RwException( "invalid palette raster format" );
                                }

                                platformTex->paletteSwizzleEncodingType = palEncodingType;

                                // Calculate the texture data size.
                                palTex.dataSize = palTex.getDataSize( palEncodingType );

                                // Read the GIF packet.
                                bool hasCorruptedHeaders = false;

                                uint32 readCount = palTex.readGIFPacket(engineInterface, gsPacketBlock, hasHeader, hasCorruptedHeaders);

                                if ( !hasCorruptedHeaders )
                                {
                                    // Also debug this texture.
                                    verifyTexture( palTex, hasHeader, palEncodingType, palEncodingType, palTransData );
                                }
                                else
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has corrupted palette GIF packets" );
                                }

                                remainingPaletteData -= readCount;

                                if (hasHeader)
                                {
                                    hasPalTransData = true;
                                }
                            }

                            if ( remainingPaletteData > 0 )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has palette meta data" );

                                // Make sure we are past the palette data.
                                gsPacketBlock.skip( remainingPaletteData );
                            }

                            // Allocate texture memory.
                            uint32 mipmapBasePointer[ maxMipmaps ];
                            uint32 mipmapMemorySize[ maxMipmaps ];
                            uint32 mipmapBufferWidth[ maxMipmaps ];

                            ps2MipmapTransmissionData mipmapTransData[ maxMipmaps ];

                            uint32 clutBasePointer;
                            uint32 clutMemSize;
                            ps2MipmapTransmissionData clutTransData;

                            eMemoryLayoutType decodedMemLayoutType;

                            bool hasAllocatedMemory =
                                platformTex->allocateTextureMemory(mipmapBasePointer, mipmapBufferWidth, mipmapMemorySize, mipmapTransData, maxMipmaps, decodedMemLayoutType, clutBasePointer, clutMemSize, clutTransData);

                            // Could fail if no memory left.
                            if ( !hasAllocatedMemory )
                            {
                                throw RwException( "failed to allocate texture memory" );
                            }

                            // Verify that our memory calculation routine is correct.
                            uint32 gpuMinMemory = platformTex->calculateGPUDataSize(mipmapBasePointer, mipmapMemorySize, maxMipmaps, decodedMemLayoutType, clutBasePointer, clutMemSize);

                            if ( textureMeta.combinedGPUDataSize > gpuMinMemory )
                            {
                                // If this assertion is triggered, then adjust the gpu size calculation algorithm
                                // so it outputs a big enough number.
                                engineInterface->PushWarning( "too small GPU data size for texture " + theTexture->GetName() );

                                // TODO: handle this as error?
                            }
                            else if ( textureMeta.combinedGPUDataSize != gpuMinMemory )
                            {
                                // It would be perfect if this condition were never triggered for official R* games textures.
                                engineInterface->PushWarning( "invalid GPU data size for texture " + theTexture->GetName() );
                            }

                            // Verify that our GPU data calculation routine is correct.
                            ps2GSRegisters gpuData;
                            
                            bool isValidTexture = platformTex->generatePS2GPUData(gsPacketBlock.getBlockVersion(), gpuData, mipmapBasePointer, mipmapBufferWidth, mipmapMemorySize, maxMipmaps, decodedMemLayoutType, clutBasePointer);

                            // If any of those assertions fail then either our is routine incomplete
                            // or the input texture is invalid (created by wrong tool probably.)
                            if ( !isValidTexture )
                            {
                                throw RwException( "invalid texture format" );
                            }

                            if ( gpuData.tex0 != textureMeta.tex0 )
                            {
                                if ( engineWarningLevel >= 3 )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid TEX0 register" );
                                }
                            }
                            if ( gpuData.tex1 != textureMeta.tex1 )
                            {
                                if ( engineWarningLevel >= 2 )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid TEX1 register" );
                                }
                            }
                            if ( gpuData.miptbp1 != textureMeta.miptbp1 )
                            {
                                if ( engineWarningLevel >= 1 )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid MIPTBP1 register" );
                                }
                            }
                            if ( gpuData.miptbp2 != textureMeta.miptbp2 )
                            {
                                if ( engineWarningLevel >= 1 )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid MIPTBP2 register" );
                                }
                            }

                            // Verify transmission rectangle same-ness.
                            if (hasHeader)
                            {
                                bool hasValidTransmissionRects = true;

                                uint32 mipmapCount = platformTex->mipmaps.size();

                                for ( uint32 n = 0; n < mipmapCount; n++ )
                                {
                                    const ps2MipmapTransmissionData& srcTransData = _origMipmapTransData[ n ];
                                    const ps2MipmapTransmissionData& dstTransData = mipmapTransData[ n ];

                                    if ( srcTransData.destX != dstTransData.destX ||
                                         srcTransData.destY != dstTransData.destY )
                                    {
                                        hasValidTransmissionRects = false;
                                        break;
                                    }
                                }

                                if ( hasValidTransmissionRects == false )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid mipmap transmission offsets" );
                                }
                            }

                            // Verify palette transmission rectangle.
                            if (platformTex->paletteType != PALETTE_NONE && hasPalTransData)
                            {
                                if ( clutTransData.destX != palTransData.destX ||
                                     clutTransData.destY != palTransData.destY )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid CLUT transmission offset" );
                                }
                            }

                            // Fix filtering mode.
                            fixFilteringMode( *theTexture, platformTex->mipmaps.size() );
                        }
                        else
                        {
                            engineInterface->PushWarning( "could not find GS image packet block in PS2 texture native" );
                        }
                    }
                    catch( ... )
                    {
                        gsPacketBlock.LeaveContext();

                        throw;
                    }

                    gsPacketBlock.LeaveContext();
                }

                // Done reading native block.
            }
            else
            {
                engineInterface->PushWarning( "could not find GS native data chunk in PS2 texture native" );
            }
        }
        catch( ... )
        {
            gsNativeBlock.LeaveContext();

            throw;
        }

        gsNativeBlock.LeaveContext();
    }

    // Deserialize extensions aswell.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static PluginDependantStructRegister <ps2NativeTextureTypeProvider, RwInterfaceFactory_t> ps2NativeTexturePlugin( engineFactory );

void registerPS2NativeTexture( Interface *engineInterface )
{
    ps2NativeTextureTypeProvider *ps2TexEnv = ps2NativeTexturePlugin.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( ps2TexEnv )
    {
        RegisterNativeTextureType( engineInterface, "PlayStation2", ps2TexEnv, sizeof( NativeTexturePS2 ) );
    }
}

/* convert from CLUT format used by the ps2 */
const static uint32 _clut_permute_psmct32[] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

static bool clut(
    Interface *engineInterface,
    ePaletteType paletteType, void *srcTexels,
    uint32 clutWidth, uint32 clutHeight, uint32 clutDataSize, eFormatEncodingType swizzleEncodingType,
    void*& newTexels
)
{
    const uint32 *permuteData = NULL;
    uint32 permuteWidth, permuteHeight;
    uint32 itemDepth;

    bool requiresSwizzling = true;

    if (paletteType == PALETTE_8BIT)
    {
        permuteData = _clut_permute_psmct32;

        permuteWidth = 16;
        permuteHeight = 2;

        itemDepth = getFormatEncodingDepth(swizzleEncodingType);
    }
    else if (paletteType == PALETTE_4BIT)
    {
        requiresSwizzling = false;
    }

    bool success = false;

    if ( requiresSwizzling )
    {
        if (permuteData != NULL)
        {
            // Create a new permutation destination.
            void *dstTexels = engineInterface->PixelAllocate( clutDataSize );

            uint32 alignedClutWidth = ALIGN_SIZE( clutWidth, permuteWidth );
            uint32 alignedClutHeight = ALIGN_SIZE( clutHeight, permuteHeight );

            uint32 colsWidth = ( alignedClutWidth / permuteWidth );
            uint32 colsHeight = ( alignedClutHeight / permuteHeight );

            // Perform the permutation.
            ps2GSPixelEncodingFormats::permuteArray(
                srcTexels, clutWidth, clutHeight, itemDepth, permuteWidth, permuteHeight,
                dstTexels, clutWidth, clutHeight, itemDepth, permuteWidth, permuteHeight,
                colsWidth, colsHeight,
                permuteData, permuteData, permuteWidth, permuteHeight, 1, 1,
                false
            );

            // Return the new texels.
            newTexels = dstTexels;

            success = true;
        }
    }
    else
    {
        newTexels = srcTexels;

        success = true;
    }

    return success;
}

inline double clampcolor( double theColor )
{
    return std::min( 1.0, std::max( 0.0, theColor ) );
}

inline uint8 convertPCAlpha2PS2Alpha( uint8 pcAlpha )
{
    double pcAlphaDouble = clampcolor( (double)pcAlpha / 255.0 );

    double ps2AlphaDouble = pcAlphaDouble * 128.0;

    ps2AlphaDouble = floor( ps2AlphaDouble + 0.5 );

    uint8 ps2Alpha = (uint8)( ps2AlphaDouble );

    return ps2Alpha;
}

inline uint8 convertPS2Alpha2PCAlpha( uint8 ps2Alpha )
{
    double ps2AlphaDouble = clampcolor( (double)ps2Alpha / 128.0 );

    double pcAlphaDouble = ps2AlphaDouble * 255.0;

    pcAlphaDouble = floor( pcAlphaDouble + 0.495 );

    uint8 pcAlpha = (uint8)( pcAlphaDouble );

    return pcAlpha;
}

inline void getEffectivePaletteTextureDimensions(ePaletteType paletteType, uint32& palWidth_out, uint32& palHeight_out)
{
    uint32 palWidth = 0;
    uint32 palHeight = 0;

    if (paletteType == PALETTE_4BIT)
    {
        palWidth = 8;
        palHeight = 2;
    }
    else if (paletteType == PALETTE_8BIT)
    {
        palWidth = 16;
        palHeight = 16;
    }
    else
    {
        assert( 0 );
    }

    palWidth_out = palWidth;
    palHeight_out = palHeight;
}

inline void convertTexelsFromPS2(
    const void *texelSource, void *dstTexels, uint32 texelCount,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder,
    bool fixAlpha
)
{
    if ( fixAlpha || srcColorOrder != dstColorOrder || srcRasterFormat != dstRasterFormat || srcDepth != dstDepth )
    {
	    for (uint32 i = 0; i < texelCount; i++)
        {
            uint8 red, green, blue, alpha;
            browsetexelcolor(texelSource, PALETTE_NONE, NULL, 0, i, srcRasterFormat, srcColorOrder, srcDepth, red, green, blue, alpha);

	        // fix alpha
            if (fixAlpha)
            {
                uint8 newAlpha = convertPS2Alpha2PCAlpha(alpha);

#ifdef DEBUG_ALPHA_LEVELS
                assert(convertPCAlpha2PS2Alpha(newAlpha) == alpha);
#endif //DEBUG_ALPHA_LEVELS

                alpha = newAlpha;
            }

            puttexelcolor(dstTexels, i, dstRasterFormat, dstColorOrder, dstDepth, red, green, blue, alpha);
	    }
    }
}

inline void GetPS2TexturePalette(
    Interface *engineInterface,
    uint32 clutWidth, uint32 clutHeight, eFormatEncodingType clutEncodingType, void *clutTexels,
    eRasterFormat srcRasterFormat, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, eColorOrdering dstColorOrder,
    ePaletteType paletteType,
    void*& dstPalTexels, uint32& dstPalSize
)
{
    // Prepare the palette colors.
    void *paletteTexelSource = clutTexels;

    // Prepare the unclut operation.
    uint32 palSize = ( clutWidth * clutHeight );

    uint32 srcPalFormatDepth = Bitmap::getRasterFormatDepth(srcRasterFormat);

    uint32 srcPalTexDataSize = getRasterDataSize( palSize, srcPalFormatDepth );

    uint32 dstPalFormatDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

    uint32 dstPalTexDataSize = getRasterDataSize( palSize, dstPalFormatDepth );

    // Unswizzle the palette, now.
    void *clutPalTexels = NULL;
    {
        bool unclutSuccess = clut(engineInterface, paletteType, paletteTexelSource, clutWidth, clutHeight, srcPalTexDataSize, clutEncodingType, clutPalTexels);

        if ( unclutSuccess == false )
        {
            throw RwException( "failed to unswizzle the CLUT" );
        }
    }

    // If we have not swizzled, then we need to allocate a new array.
    const void *srcTexels = clutPalTexels;

    if ( clutPalTexels == paletteTexelSource )
    {
        clutPalTexels = engineInterface->PixelAllocate( srcPalTexDataSize );
    }

    // Repair the colors.
    {
        uint32 realSwizzleWidth, realSwizzleHeight;

        getEffectivePaletteTextureDimensions(paletteType, realSwizzleWidth, realSwizzleHeight);

        convertTexelsFromPS2(
            srcTexels, clutPalTexels, realSwizzleWidth * realSwizzleHeight,
            srcRasterFormat, srcPalFormatDepth, srcColorOrder,
            dstRasterFormat, dstPalFormatDepth, dstColorOrder,
            true
        );
    }

    // Give stuff to the runtime.
    dstPalTexels = clutPalTexels;
    dstPalSize = palSize;
}

inline void GetPS2TextureTranscodedMipmapData(
    Interface *engineInterface,
    uint32 layerWidth, uint32 layerHeight, uint32 swizzleWidth, uint32 swizzleHeight, const void *srcTexels, uint32 srcTexDataSize,
    eFormatEncodingType mipmapSwizzleEncodingType, eFormatEncodingType mipmapDecodeFormat,
    eRasterFormat srcRasterFormat, uint32 srcDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstDepth, eColorOrdering dstColorOrder,
    ePaletteType paletteType, uint32 paletteSize,
    void*& dstTexelsOut, uint32& dstDataSizeOut
)
{
    bool hasToTranscode = ( mipmapSwizzleEncodingType != mipmapDecodeFormat );

    void *texelData = NULL;
    uint32 dstDataSize = 0;

    uint32 texLinearUnitCount = ( layerWidth * layerHeight );

    bool doesSourceNeedDeletion = false;

    if ( hasToTranscode )
    {
        uint32 newDataSize;

        void *newTexels =
            ps2GSPixelEncodingFormats::unpackImageData(
                engineInterface,
                mipmapSwizzleEncodingType, mipmapDecodeFormat,
                srcDepth,
                srcTexels,
                swizzleWidth, swizzleHeight,
                newDataSize,
                layerWidth, layerHeight
            );

        assert( newTexels != NULL );

        if ( srcDepth == dstDepth )
        {
            texelData = newTexels;
            dstDataSize = newDataSize;

            // We want to operate the conversion on ourselves.
            srcTexels = texelData;
        }
        else
        {
            dstDataSize = getRasterDataSize( texLinearUnitCount, dstDepth );

            srcTexels = newTexels;

            texelData = engineInterface->PixelAllocate( dstDataSize );

            // We need to transcode into a bigger array.
            doesSourceNeedDeletion = true;
        }
    }
    else
    {
        if ( srcDepth == dstDepth )
        {
            // At best, we simply want to copy the texels.
            dstDataSize = srcTexDataSize;
        }
        else
        {
            dstDataSize = getRasterDataSize( texLinearUnitCount, dstDepth );
        }

        texelData = engineInterface->PixelAllocate( dstDataSize );
    }

    // Now that the texture is in linear format, we can prepare it.
    bool fixAlpha = false;

    // TODO: do we have to fix alpha for 16bit raster depths?

    if (srcRasterFormat == RASTER_8888)
    {
        fixAlpha = true;
    }

    // Prepare colors.
    if (paletteType == PALETTE_NONE)
    {
        convertTexelsFromPS2(
            srcTexels, texelData, texLinearUnitCount,
            srcRasterFormat, srcDepth, srcColorOrder, 
            dstRasterFormat, dstDepth, dstColorOrder,
            fixAlpha
        );
    }
    else
    {
        if ( texelData != srcTexels )
        {
            if ( srcDepth == dstDepth )
            {
                memcpy( texelData, srcTexels, std::min(dstDataSize, srcTexDataSize) );
            }
            else
            {
                // We need to convert the palette indice into another bit depth.
                ConvertPaletteDepth(
                    srcTexels, texelData,
                    texLinearUnitCount,
                    paletteType, paletteSize,
                    srcDepth, dstDepth
                );
            }
        }
    }

    // Return stuff to the runtime.
    dstTexelsOut = texelData;
    dstDataSizeOut = dstDataSize;
}

void ps2NativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{
    // Cast to our native platform texture.
    NativeTexturePS2 *platformTex = (NativeTexturePS2*)objMem;

    uint32 mipmapCount = platformTex->mipmaps.size();

    eRasterFormat rasterFormat = platformTex->rasterFormat;
    ePaletteType paletteType = platformTex->paletteType;

    // Copy over general attributes.
    uint32 depth = platformTex->depth;

    // Fix wrong auto mipmap property.
    bool hasAutoMipmaps = false;

    if ( mipmapCount == 1 )
    {
        // Direct3D textures can only have mipmaps if they dont come with mipmaps.
        hasAutoMipmaps = platformTex->autoMipmaps;
    }

    pixelsOut.autoMipmaps = hasAutoMipmaps;
    pixelsOut.rasterType = platformTex->rasterType;

    pixelsOut.cubeTexture = false;

    // We will have to swap colors.
    eColorOrdering ps2ColorOrder = platformTex->colorOrdering;
    eColorOrdering d3dColorOrder = COLOR_BGRA;

    // First we want to decode the CLUT.
    void *palTexels = NULL;
    uint32 palSize = 0;

    if (paletteType != PALETTE_NONE)
    {
        // We need to decode the PS2 CLUT.
        GetPS2TexturePalette(
            engineInterface,
            platformTex->paletteTex.swizzleWidth, platformTex->paletteTex.swizzleHeight, platformTex->paletteSwizzleEncodingType, platformTex->paletteTex.texels,
            rasterFormat, ps2ColorOrder,
            rasterFormat, d3dColorOrder,
            paletteType,
            palTexels, palSize
        );
    }

    // Process the mipmaps.
    eFormatEncodingType mipmapSwizzleEncodingType = platformTex->swizzleEncodingType;

    eFormatEncodingType mipmapDecodeFormat = getFormatEncodingFromRasterFormat(rasterFormat, paletteType);

    // Make sure there is no unknown format.
    assert( mipmapSwizzleEncodingType != FORMAT_UNKNOWN && mipmapDecodeFormat != FORMAT_UNKNOWN );

    pixelsOut.mipmaps.resize( mipmapCount );

    for (uint32 j = 0; j < mipmapCount; j++)
    {
        NativeTexturePS2::GSMipmap& gsTex = platformTex->mipmaps[ j ];

        // We have to create a new texture buffer that is unswizzled to linear format.
        uint32 layerWidth = gsTex.width;
        uint32 layerHeight = gsTex.height;

        const void *srcTexels = gsTex.texels;
        uint32 texDataSize = gsTex.dataSize;

        void *dstTexels = NULL;
        uint32 dstDataSize = 0;

        GetPS2TextureTranscodedMipmapData(
            engineInterface,
            layerWidth, layerHeight, gsTex.swizzleWidth, gsTex.swizzleHeight, gsTex.texels, gsTex.dataSize,
            mipmapSwizzleEncodingType, mipmapDecodeFormat,
            rasterFormat, depth, ps2ColorOrder,
            rasterFormat, depth, d3dColorOrder,
            paletteType, palSize,
            dstTexels, dstDataSize
        );

        // Move over the texture data to pixel storage.
        pixelDataTraversal::mipmapResource newLayer;

        newLayer.width = layerWidth;
        newLayer.height = layerHeight;
        newLayer.mipWidth = layerWidth;   // layer dimensions.
        newLayer.mipHeight = layerHeight;

        newLayer.texels = dstTexels;
        newLayer.dataSize = dstDataSize;

        // Store the layer.
        pixelsOut.mipmaps[ j ] = newLayer;
    }

    // Set up general raster attributes.
    pixelsOut.rasterFormat = rasterFormat;
    pixelsOut.colorOrder = d3dColorOrder;
    pixelsOut.depth = depth;

    // Copy over more advanced attributes.
    pixelsOut.paletteData = palTexels;
    pixelsOut.paletteSize = palSize;
    pixelsOut.paletteType = paletteType;

    // We are an uncompressed raster.
    pixelsOut.compressionType = RWCOMPRESS_NONE;

    // Actually, since there is no alpha flag in PS2 textures, we should recalculate the alpha flag here.
    pixelsOut.hasAlpha = calculateHasAlpha( pixelsOut );

    // For now, we will always allocate new pixels due to the complexity of the encoding.
    pixelsOut.isNewlyAllocated = true;
}

inline void convertTexelsToPS2(
    const void *srcTexelData, void *dstTexelData, uint32 texelCount,
    eRasterFormat srcRasterFormat, eRasterFormat dstRasterFormat,
    uint32 srcItemDepth, uint32 dstItemDepth,
    eColorOrdering srcColorOrder, eColorOrdering ps2ColorOrder,
    bool fixAlpha
)
{
    if ( fixAlpha || srcColorOrder != ps2ColorOrder || srcRasterFormat != dstRasterFormat || srcItemDepth != dstItemDepth )
    {
		for (uint32 i = 0; i < texelCount; i++)
        {
            uint8 red, green, blue, alpha;
            browsetexelcolor(srcTexelData, PALETTE_NONE, NULL, 0, i, srcRasterFormat, srcColorOrder, srcItemDepth, red, green, blue, alpha);

		    // fix alpha
            if (fixAlpha)
            {
                uint8 newAlpha = convertPCAlpha2PS2Alpha(alpha);

#ifdef DEBUG_ALPHA_LEVELS
                assert(convertPS2Alpha2PCAlpha(newAlpha) == alpha);
#endif //DEBUG_ALPHA_LEVELS

                alpha = newAlpha;
            }

            puttexelcolor(dstTexelData, i, dstRasterFormat, ps2ColorOrder, dstItemDepth, red, green, blue, alpha);
		}
    }
}

inline void ConvertMipmapToPS2Format(
    Interface *engineInterface,
    uint32 mipWidth, uint32 mipHeight, const void *srcTexelData, uint32 srcDataSize,
    eFormatEncodingType linearMipmapInternalFormat, eFormatEncodingType swizzleMipmapRequiredEncoding,
    eRasterFormat srcRasterFormat, uint32 srcItemDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstItemDepth, eColorOrdering dstColorOrder,
    ePaletteType paletteType, uint32 paletteSize,
    uint32& swizzleWidthOut, uint32& swizzleHeightOut,
    void*& dstSwizzledTexelsOut, uint32& dstSwizzledDataSizeOut
)
{
    // We need to convert the texels before storing them in the PS2 texture.
    bool fixAlpha = false;

    // TODO: do we have to fix alpha for 16bit rasters?

    if (dstRasterFormat == RASTER_8888)
    {
        fixAlpha = true;
    }

    // Allocate a new copy of the texel data.
    uint32 itemCount = ( mipWidth * mipHeight );

    uint32 dstLinearDataSize = getRasterDataSize( itemCount, dstItemDepth );

    void *dstLinearTexelData = engineInterface->PixelAllocate( dstLinearDataSize );

    // Convert the texels.
    if (paletteType == PALETTE_NONE)
    {
        convertTexelsToPS2(
            srcTexelData, dstLinearTexelData, itemCount,
            srcRasterFormat, dstRasterFormat,
            srcItemDepth, dstItemDepth,
            srcColorOrder, dstColorOrder,
            fixAlpha
        );
    }
    else
    {
        // Maybe we need to fix the indice (if the texture comes from PC or XBOX architecture).
        ConvertPaletteDepth(
            srcTexelData, dstLinearTexelData,
            itemCount,
            paletteType, paletteSize,
            srcItemDepth, dstItemDepth
        );
    }

    // Swizzle the mipmap.
    // We need to store dimensions into the texture of the current encoding.
    uint32 packedWidth, packedHeight;
    
    // Perform swizzling.
    void *dstSwizzledTexelData = NULL;
    uint32 dstSwizzledDataSize = 0;

    if ( linearMipmapInternalFormat != swizzleMipmapRequiredEncoding )
    {
        dstSwizzledTexelData =
            ps2GSPixelEncodingFormats::packImageData(
                engineInterface,
                linearMipmapInternalFormat, swizzleMipmapRequiredEncoding,
                dstItemDepth,
                dstLinearTexelData,
                mipWidth, mipHeight,
                dstSwizzledDataSize, packedWidth, packedHeight
            );

        if ( dstSwizzledTexelData == NULL )
        {
            // The probability of this failing is medium.
            throw RwException( "failed to swizzle texture" );
        }
    }
    else
    {
        // Just get the encoding dimensions manually.
        ps2GSPixelEncodingFormats::getPackedFormatDimensions(
            linearMipmapInternalFormat, swizzleMipmapRequiredEncoding,
            mipWidth, mipHeight,
            packedWidth, packedHeight
        );

        dstSwizzledTexelData = dstLinearTexelData;
        dstSwizzledDataSize = dstLinearDataSize;
    }

    // Free temporary unswizzled texels.
    if ( dstSwizzledTexelData != dstLinearTexelData )
    {
        engineInterface->PixelFree( dstLinearTexelData );
    }

    // Give results to the runtime.
    swizzleWidthOut = packedWidth;
    swizzleHeightOut = packedHeight;

    dstSwizzledTexelsOut = dstSwizzledTexelData;
    dstSwizzledDataSizeOut = dstSwizzledDataSize;
}

inline void GeneratePS2CLUT(
    Interface *engineInterface, LibraryVersion targetVersion,
    const void *srcPalTexelData, ePaletteType paletteType, uint32 paletteSize,
    eFormatEncodingType clutRequiredEncoding,
    eRasterFormat srcRasterFormat, uint32 srcPalFormatDepth, eColorOrdering srcColorOrder,
    eRasterFormat dstRasterFormat, uint32 dstPalFormatDepth, eColorOrdering dstColorOrder,
    uint32& dstCLUTWidth, uint32& dstCLUTHeight,
    void*& dstCLUTTexelData, uint32& dstCLUTDataSize
)
{
    // Allocate a new destination texel array.
    void *dstPalTexelData = NULL;
    {
        uint32 palDataSize = getRasterDataSize(paletteSize, dstPalFormatDepth);

        dstPalTexelData = engineInterface->PixelAllocate( palDataSize );
    }

    convertTexelsToPS2(
        srcPalTexelData, dstPalTexelData, paletteSize,
        srcRasterFormat, dstRasterFormat,
        srcPalFormatDepth, dstPalFormatDepth,
        srcColorOrder, dstColorOrder,
        true
    );

    // Generate a palette texture.
    void *newPalTexelData;
    uint32 newPalDataSize;

    uint32 palWidth, palHeight;

    genpalettetexeldata(
        targetVersion, dstPalTexelData, dstRasterFormat, paletteType, paletteSize,
        newPalTexelData, newPalDataSize, palWidth, palHeight
    );

    // If we allocated a new palette array, we need to free the source one.
    if ( newPalTexelData != dstPalTexelData )
    {
        engineInterface->PixelFree( dstPalTexelData );
    }

    // Now CLUT the palette.
    void *clutSwizzledTexels = NULL;

    bool clutSuccess = clut( engineInterface, paletteType, newPalTexelData, palWidth, palHeight, newPalDataSize, clutRequiredEncoding, clutSwizzledTexels );

    if ( clutSuccess == false )
    {
        // The probability of this failing is slim like a straw of hair.
        throw RwException( "failed to swizzle the CLUT" );
    }

    // If we allocated new texels for the CLUT, delete the old ones.
    if ( clutSwizzledTexels != newPalTexelData )
    {
        engineInterface->PixelFree( newPalTexelData );
    }

    // Return values to the runtime.
    dstCLUTWidth = palWidth;
    dstCLUTHeight = palHeight;

    dstCLUTTexelData = clutSwizzledTexels;
    dstCLUTDataSize = newPalDataSize;
}

void ps2NativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut )
{
    // Cast to our native format.
    NativeTexturePS2 *ps2tex = (NativeTexturePS2*)objMem;

    LibraryVersion currentVersion = ps2tex->texVersion;

    // Make sure that we got uncompressed bitmap data.
    assert( pixelsIn.compressionType == RWCOMPRESS_NONE );

    // Free any image data that may have been there previously.
    //ps2tex->clearImageData();

    // The maximum amount of mipmaps supported by PS2 textures.
    const uint32 maxMipmaps = 7;

    {
        // The PlayStation 2 does NOT support all raster formats.
        // We need to avoid giving it raster formats that are prone to crashes, like RASTER_888.
        eRasterFormat srcRasterFormat = pixelsIn.rasterFormat;
        uint32 srcItemDepth = pixelsIn.depth;

        eRasterFormat targetRasterFormat = srcRasterFormat;
        uint32 dstItemDepth = srcItemDepth;

        ePaletteType paletteType = pixelsIn.paletteType;
        uint32 paletteSize = pixelsIn.paletteSize;

        if (targetRasterFormat == RASTER_888)
        {
            // Since this raster takes the same memory space as RASTER_8888, we can silently convert it.
            targetRasterFormat = RASTER_8888;
        }
        else if (targetRasterFormat != RASTER_1555)
        {
            // We need to change the format of the texture, as we do not support it.
            targetRasterFormat = RASTER_8888;
        }

        if (paletteType != PALETTE_NONE)
        {
            // The architecture does not support 8bit PALETTE_4BIT rasters.
            // Fix that.
            if (paletteType == PALETTE_4BIT)
            {
                dstItemDepth = 4;
            }
            else if (paletteType == PALETTE_8BIT)
            {
                dstItemDepth = 8;
            }
        }

        uint32 targetRasterDepth = Bitmap::getRasterFormatDepth(targetRasterFormat);

        if (paletteType == PALETTE_NONE)
        {
            dstItemDepth = targetRasterDepth;
        }

        // Set the palette type.
        ps2tex->paletteType = paletteType;

        // Finally, set the raster format.
        ps2tex->rasterFormat = targetRasterFormat;

        // Prepare mipmap data.
        eColorOrdering d3dColorOrder = pixelsIn.colorOrder;
        eColorOrdering ps2ColorOrder = ps2tex->colorOrdering;

        // Prepare swizzling parameters.
        eFormatEncodingType linearMipmapInternalFormat = 
            getFormatEncodingFromRasterFormat(targetRasterFormat, paletteType);

        assert(linearMipmapInternalFormat != FORMAT_UNKNOWN);

        // Get the format we need to encode mipmaps in.
        eFormatEncodingType swizzleMipmapRequiredEncoding = ps2tex->getHardwareRequiredEncoding(currentVersion);

        assert(swizzleMipmapRequiredEncoding != FORMAT_UNKNOWN);

        uint32 mipmapCount = pixelsIn.mipmaps.size();
        {
            uint32 mipProcessCount = std::min( maxMipmaps, mipmapCount );

            ps2tex->mipmaps.resize( mipProcessCount );

            for ( uint32 n = 0; n < mipProcessCount; n++ )
            {
                // Process every mipmap individually.
                NativeTexturePS2::GSMipmap& newMipmap = ps2tex->mipmaps[ n ];

                const pixelDataTraversal::mipmapResource& oldMipmap = pixelsIn.mipmaps[ n ];

                uint32 layerWidth = oldMipmap.width;
                uint32 layerHeight = oldMipmap.height;
                uint32 srcDataSize = oldMipmap.dataSize;

                const void *srcTexelData = oldMipmap.texels;

                // Convert the mipmap and fetch new parameters.
                uint32 packedWidth, packedHeight;

                void *dstSwizzledTexelData;
                uint32 dstSwizzledDataSize;

                ConvertMipmapToPS2Format(
                    engineInterface,
                    layerWidth, layerHeight, srcTexelData, srcDataSize,
                    linearMipmapInternalFormat, swizzleMipmapRequiredEncoding,
                    srcRasterFormat, srcItemDepth, d3dColorOrder,
                    targetRasterFormat, dstItemDepth, ps2ColorOrder,
                    paletteType, paletteSize,
                    packedWidth, packedHeight,
                    dstSwizzledTexelData, dstSwizzledDataSize
                );

                // Store the new mipmap.
                newMipmap.width = layerWidth;
                newMipmap.height = layerHeight;

                newMipmap.swizzleWidth = packedWidth;
                newMipmap.swizzleHeight = packedHeight;

                newMipmap.texels = dstSwizzledTexelData;
                newMipmap.dataSize = dstSwizzledDataSize;
            }
        }

        // We are now properly encoded.
        ps2tex->swizzleEncodingType = swizzleMipmapRequiredEncoding;

        // Copy over general attributes.
        ps2tex->depth = dstItemDepth;

        // Make sure we apply autoMipmap property just like the R* converter.
        bool hasAutoMipmaps = pixelsIn.autoMipmaps;

        if ( mipmapCount > 1 )
        {
            hasAutoMipmaps = true;
        }

        ps2tex->autoMipmaps = hasAutoMipmaps;
        ps2tex->rasterType = pixelsIn.rasterType;

        ps2tex->hasAlpha = pixelsIn.hasAlpha;

        // Move over the palette texels.
        if (paletteType != PALETTE_NONE)
        {
            eFormatEncodingType clutRequiredEncoding = getFormatEncodingFromRasterFormat(targetRasterFormat, PALETTE_NONE);

            // Prepare the palette texels.
            const void *srcPalTexelData = pixelsIn.paletteData;

            uint32 srcPalFormatDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
            uint32 targetPalFormatDepth = targetRasterDepth;

            // Swizzle the CLUT.
            uint32 palWidth, palHeight;

            void *clutSwizzledTexels = NULL;
            uint32 newPalDataSize = 0;

            GeneratePS2CLUT(
                engineInterface, currentVersion,
                srcPalTexelData, paletteType, paletteSize, clutRequiredEncoding,
                srcRasterFormat, srcPalFormatDepth, d3dColorOrder,
                targetRasterFormat, targetPalFormatDepth, ps2ColorOrder,
                palWidth, palHeight,
                clutSwizzledTexels, newPalDataSize
            );

            // Store the palette texture.
            NativeTexturePS2::GSTexture& palTex = ps2tex->paletteTex;

            palTex.swizzleWidth = palWidth;
            palTex.swizzleHeight = palHeight;
            palTex.dataSize = newPalDataSize;
            
            ps2tex->paletteSwizzleEncodingType = clutRequiredEncoding;

            palTex.texels = clutSwizzledTexels;
        }
    }

    // TODO: improve exception safety.

    // Generate valid gsParams for this texture, as we lost our original ones.
    ps2tex->getOptimalGSParameters(ps2tex->gsParams);

    // We do not take the pixels directly, because we need to decode them.
    feedbackOut.hasDirectlyAcquired = false;
}

void ps2NativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    if ( deallocate )
    {
        uint32 mipmapCount = nativeTex->mipmaps.size();

        // Free all mipmaps.
        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            NativeTexturePS2::GSMipmap& mipLayer = nativeTex->mipmaps[ n ];

            mipLayer.FreeTexels( engineInterface );
        }

        // Free the palette texture.
        nativeTex->paletteTex.FreeTexels( engineInterface );
    }

    // Clear our mipmaps.
    nativeTex->mipmaps.clear();

    // Clear the palette texture.
    nativeTex->paletteTex.DetachTexels();

    // For debugging purposes, reset the texture raster information.
    nativeTex->rasterFormat = RASTER_DEFAULT;
    nativeTex->depth = 0;
    nativeTex->paletteType = PALETTE_NONE;
    nativeTex->recommendedBufferBasePointer = 0;
    nativeTex->swizzleEncodingType = FORMAT_UNKNOWN;
    nativeTex->paletteSwizzleEncodingType = FORMAT_UNKNOWN;
    nativeTex->autoMipmaps = false;
    nativeTex->hasAlpha = false;
    nativeTex->rasterType = 4;
    nativeTex->colorOrdering = COLOR_RGBA;
}

struct ps2MipmapManager
{
    NativeTexturePS2 *nativeTex;

    inline ps2MipmapManager( NativeTexturePS2 *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTexturePS2::GSMipmap& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.width;
        layerHeight = mipLayer.height;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTexturePS2::GSMipmap& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocatedOut
    )
    {
        // We need to decode our mipmap layer.
        uint32 layerWidth = mipLayer.width;
        uint32 layerHeight = mipLayer.height;

        void *srcTexels = mipLayer.texels;
        uint32 dataSize = mipLayer.dataSize;

        eRasterFormat srcRasterFormat = nativeTex->rasterFormat;
        uint32 srcDepth = nativeTex->depth;
        eColorOrdering srcColorOrder = nativeTex->colorOrdering;

        ePaletteType srcPaletteType = nativeTex->paletteType;

        // Get the decoded palette data.
        void *decodedPaletteData = NULL;
        uint32 decodedPaletteSize = 0;

        if (srcPaletteType != PALETTE_NONE)
        {
            // We need to decode the PS2 CLUT.
            GetPS2TexturePalette(
                engineInterface,
                nativeTex->paletteTex.swizzleWidth, nativeTex->paletteTex.swizzleHeight, nativeTex->paletteSwizzleEncodingType, nativeTex->paletteTex.texels,
                srcRasterFormat, srcColorOrder,
                srcRasterFormat, srcColorOrder,
                srcPaletteType,
                decodedPaletteData, decodedPaletteSize
            );
        }

        // Process the mipmap texels.
        eFormatEncodingType mipmapSwizzleEncodingType = nativeTex->swizzleEncodingType;

        eFormatEncodingType mipmapDecodeFormat = getFormatEncodingFromRasterFormat(srcRasterFormat, srcPaletteType);

        // Make sure there is no unknown format.
        assert( mipmapSwizzleEncodingType != FORMAT_UNKNOWN && mipmapDecodeFormat != FORMAT_UNKNOWN );

        bool hasToDeswizzle = ( mipmapSwizzleEncodingType != mipmapDecodeFormat );

        // Get the unswizzled texel data.
        void *dstTexels = NULL;
        uint32 dstDataSize = 0;

        GetPS2TextureTranscodedMipmapData(
            engineInterface,
            layerWidth, layerHeight, mipLayer.swizzleWidth, mipLayer.swizzleHeight, srcTexels, dataSize,
            mipmapSwizzleEncodingType, mipmapDecodeFormat,
            srcRasterFormat, srcDepth, srcColorOrder,
            srcRasterFormat, srcDepth, srcColorOrder,
            srcPaletteType, decodedPaletteSize,
            dstTexels, dstDataSize
        );

        // Return parameters to the runtime.
        widthOut = layerWidth;
        heightOut = layerHeight;

        layerWidthOut = layerWidth;
        layerHeightOut = layerHeight;

        dstRasterFormat = srcRasterFormat;
        dstDepth = srcDepth;
        dstColorOrder = srcColorOrder;

        dstPaletteType = srcPaletteType;
        dstPaletteData = decodedPaletteData;
        dstPaletteSize = decodedPaletteSize;

        dstCompressionType = RWCOMPRESS_NONE;

        hasAlpha = nativeTex->hasAlpha; // TODO: this is not entirely true

        dstTexelsOut = dstTexels;
        dstDataSizeOut = dstDataSize;

        // We have newly allocated both texels and palette data.
        isNewlyAllocatedOut = true;
        isPaletteNewlyAllocatedOut = true;
    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTexturePS2::GSMipmap& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {
        // Check whether we have reached the maximum mipmap count.
        const uint32 maxMipmaps = 7;
    
        if ( nativeTex->mipmaps.size() >= maxMipmaps )
        {
            throw RwException( "cannot add mipmap in PS2 texture because too many" );
        }

        LibraryVersion currentVersion = nativeTex->texVersion;

        // Get the texture properties on the stack.
        eRasterFormat texRasterFormat = nativeTex->rasterFormat;
        uint32 texDepth = nativeTex->depth;
        eColorOrdering texColorOrder = nativeTex->colorOrdering;

        ePaletteType texPaletteType = nativeTex->paletteType;

        // If we are a palette texture, decode our palette for remapping.
        void *texPaletteData = NULL;
        uint32 texPaletteSize = 0;

        if ( texPaletteType != PALETTE_NONE )
        {
            // We need to decode the PS2 CLUT.
            GetPS2TexturePalette(
                engineInterface,
                nativeTex->paletteTex.swizzleWidth, nativeTex->paletteTex.swizzleHeight, nativeTex->paletteSwizzleEncodingType, nativeTex->paletteTex.texels,
                texRasterFormat, texColorOrder,
                texRasterFormat, texColorOrder,
                texPaletteType,
                texPaletteData, texPaletteSize
            );
        }

        // Convert the input data to our texture's format.
        bool srcTexelsNewlyAllocated = false;

        bool hasConverted =
            ConvertMipmapLayerNative(
                engineInterface,
                width, height, layerWidth, layerHeight, srcTexels, dataSize,
                rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                texRasterFormat, texDepth, texColorOrder, texPaletteType, texPaletteData, texPaletteSize, RWCOMPRESS_NONE,
                false,
                width, height,
                srcTexels, dataSize
            );

        if ( hasConverted )
        {
            srcTexelsNewlyAllocated = true;
        }

        // We do not need the CLUT anymore, if we allocated it.
        if ( texPaletteData )
        {
            engineInterface->PixelFree( texPaletteData );
        }

        // Prepare swizzling parameters.
        eFormatEncodingType linearMipmapInternalFormat = 
            getFormatEncodingFromRasterFormat(texRasterFormat, texPaletteType);

        assert(linearMipmapInternalFormat != FORMAT_UNKNOWN);

        // Get the format we need to encode mipmaps in.
        eFormatEncodingType swizzleMipmapRequiredEncoding = nativeTex->getHardwareRequiredEncoding(currentVersion);

        assert(swizzleMipmapRequiredEncoding != FORMAT_UNKNOWN);

        // Now we have to encode our texels.
        void *dstSwizzledTexels = NULL;
        uint32 dstSwizzledDataSize = 0;

        uint32 packedWidth, packedHeight;

        ConvertMipmapToPS2Format(
            engineInterface,
            layerWidth, layerHeight, srcTexels, dataSize,
            linearMipmapInternalFormat, swizzleMipmapRequiredEncoding,
            texRasterFormat, texDepth, texColorOrder,
            texRasterFormat, texDepth, texColorOrder,
            texPaletteType, texPaletteSize,
            packedWidth, packedHeight,
            dstSwizzledTexels, dstSwizzledDataSize
        );

        // Free the linear data.
        if ( srcTexelsNewlyAllocated )
        {
            engineInterface->PixelFree( srcTexels );
        }

        // Store the encoded texels.
        mipLayer.width = layerWidth;
        mipLayer.height = layerHeight;

        mipLayer.swizzleWidth = packedWidth;
        mipLayer.swizzleHeight = packedHeight;

        mipLayer.texels = dstSwizzledTexels;
        mipLayer.dataSize = dstSwizzledDataSize;

        // Since we encoded the texels, we cannot ever directly acquire them.
        hasDirectlyAcquiredOut = false;
    }
};

bool ps2NativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    ps2MipmapManager mipMan( nativeTex );

    return
        virtualGetMipmapLayer <NativeTexturePS2::GSMipmap> (
            engineInterface, mipMan,
            mipIndex,
            nativeTex->mipmaps, layerOut
        );
}

bool ps2NativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    ps2MipmapManager mipMan( nativeTex );

    return
        virtualAddMipmapLayer <NativeTexturePS2::GSMipmap> (
            engineInterface, mipMan,
            nativeTex->mipmaps,
            layerIn, feedbackOut
        );
}

void ps2NativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    return
        virtualClearMipmaps <NativeTexturePS2::GSMipmap> ( engineInterface, nativeTex->mipmaps );
}

void ps2NativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    NativeTexturePS2 *nativeTex = (NativeTexturePS2*)objMem;

    uint32 mipmapCount = nativeTex->mipmaps.size();

    infoOut.mipmapCount = mipmapCount;

    uint32 baseWidth = 0;
    uint32 baseHeight = 0;

    if ( mipmapCount > 0 )
    {
        baseWidth = nativeTex->mipmaps[ 0 ].width;
        baseHeight = nativeTex->mipmaps[ 0 ].height;
    }

    infoOut.baseWidth = baseWidth;
    infoOut.baseHeight = baseHeight;
}

};