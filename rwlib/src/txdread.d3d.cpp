#include <StdInc.h>

#include "txdread.d3d.hxx"

#include "pixelformat.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.dxt.hxx"

#include "pluginutil.hxx"

namespace rw
{

inline uint32 getCompressionFromD3DFormat( D3DFORMAT d3dFormat )
{
    uint32 compressionIndex = 0;

    if ( d3dFormat == D3DFMT_DXT1 )
    {
        compressionIndex = 1;
    }
    else if ( d3dFormat == D3DFMT_DXT2 )
    {
        compressionIndex = 2;
    }
    else if ( d3dFormat == D3DFMT_DXT3 )
    {
        compressionIndex = 3;
    }
    else if ( d3dFormat == D3DFMT_DXT4 )
    {
        compressionIndex = 4;
    }
    else if ( d3dFormat == D3DFMT_DXT5 )
    {
        compressionIndex = 5;
    }

    return compressionIndex;
}

void d3dNativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    {
        BlockProvider texNativeImageStruct( &inputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            if ( texNativeImageStruct.getBlockID() == CHUNK_STRUCT )
            {
                d3d::textureMetaHeaderStructGeneric metaHeader;
                texNativeImageStruct.read( &metaHeader, sizeof(metaHeader) );

	            uint32 platform = metaHeader.platformDescriptor;

	            if (platform != PLATFORM_D3D8 && platform != PLATFORM_D3D9)
                {
                    throw RwException( "invalid platform type in Direct3D texture reading" );
                }

                // Recast the texture to our native type.
                NativeTextureD3D *platformTex = (NativeTextureD3D*)nativeTex;

                // Store the proper platform type.
                if ( platform == PLATFORM_D3D8 )
                {
                    platformTex->platformType = NativeTextureD3D::PLATFORM_D3D8;
                }
                else if ( platform == PLATFORM_D3D9 )
                {
                    platformTex->platformType = NativeTextureD3D::PLATFORM_D3D9;
                }

                int engineWarningLevel = engineInterface->GetWarningLevel();

                bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

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

                // Read texture format.
                metaHeader.texFormat.parse( *theTexture );

                // Deconstruct the format flags.
                bool hasMipmaps = false;    // TODO: actually use this flag.

                readRasterFormatFlags( metaHeader.rasterFormat, platformTex->rasterFormat, platformTex->paletteType, hasMipmaps, platformTex->autoMipmaps );

                platformTex->hasAlpha = false;

                if ( platform == PLATFORM_D3D9 )
                {
                    D3DFORMAT d3dFormat;

	                texNativeImageStruct.read( &d3dFormat, sizeof(d3dFormat) );

                    // Alpha is not decided here.
                    platformTex->d3dFormat = d3dFormat;
                }
                else
                {
	                platformTex->hasAlpha = ( inputProvider.readUInt32() != 0 );

                    // Set d3dFormat later.
                }

                d3d::textureMetaHeaderStructDimInfo dimInfo;
                inputProvider.read( &dimInfo, sizeof(dimInfo) );

                uint32 depth = dimInfo.depth;
                uint32 maybeMipmapCount = dimInfo.mipmapCount;

                platformTex->depth = depth;

                platformTex->rasterType = dimInfo.rasterType;

                if ( platform == PLATFORM_D3D9 )
                {
                    // Here we decide about alpha.
                    d3d::textureContentInfoStruct contentInfo;
                    texNativeImageStruct.read( &contentInfo, sizeof(contentInfo) );

	                platformTex->hasAlpha = contentInfo.hasAlpha;
                    platformTex->isCubeTexture = contentInfo.isCubeTexture;
                    platformTex->autoMipmaps = contentInfo.autoMipMaps;

	                if ( contentInfo.isCompressed )
                    {
		                // Detect FOUR-CC versions for compression method.
                        uint32 dxtCompression = getCompressionFromD3DFormat(platformTex->d3dFormat);

                        if ( dxtCompression == 0 )
                        {
                            throw RwException( "invalid Direct3D texture compression format" );
                        }

                        platformTex->dxtCompression = dxtCompression;
                    }
	                else
                    {
		                platformTex->dxtCompression = 0;
                    }
                }
                else
                {
                    uint32 dxtInfo = texNativeImageStruct.readUInt8();

                    platformTex->dxtCompression = dxtInfo;

                    // Auto-decide the Direct3D format.
                    D3DFORMAT d3dFormat = D3DFMT_A8R8G8B8;

                    if ( dxtInfo != 0 )
                    {
                        if ( dxtInfo == 1 )
                        {
                            d3dFormat = D3DFMT_DXT1;
                        }
                        else if ( dxtInfo == 2 )
                        {
                            d3dFormat = D3DFMT_DXT2;
                        }
                        else if ( dxtInfo == 3 )
                        {
                            d3dFormat = D3DFMT_DXT3;
                        }
                        else if ( dxtInfo == 4 )
                        {
                            d3dFormat = D3DFMT_DXT4;
                        }
                        else if ( dxtInfo == 5 )
                        {
                            d3dFormat = D3DFMT_DXT5;
                        }
                        else
                        {
                            throw RwException( "invalid Direct3D texture compression format" );
                        }
                    }
                    else
                    {
                        eRasterFormat paletteRasterType = platformTex->rasterFormat;
                        ePaletteType paletteType = platformTex->paletteType;
                        
                        eColorOrdering probableColorOrder = COLOR_BGRA;

                        if (paletteType != PALETTE_NONE)
                        {
                            probableColorOrder = COLOR_RGBA;
                        }

                        bool hasFormat = getD3DFormatFromRasterType( paletteRasterType, paletteType, probableColorOrder, depth, d3dFormat );

                        if ( !hasFormat )
                        {
                            throw RwException( "could not determine D3DFORMAT for texture " + theTexture->GetName() );
                        }
                    }

                    platformTex->d3dFormat = d3dFormat;
                }

                // Verify raster properties and attempt to fix broken textures.
                // Broken textures travel with mods like San Andreas Retextured.
                // - Verify compression.
                D3DFORMAT d3dFormat = platformTex->d3dFormat;
                {
                    uint32 actualCompression = getCompressionFromD3DFormat( d3dFormat );

                    if (actualCompression != platformTex->dxtCompression)
                    {
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid compression parameters (ignoring)" );

                        platformTex->dxtCompression = actualCompression;
                    }
                }
                // - Verify raster format.
                {
                    bool isValidFormat = false;
                    bool isRasterFormatRequired = true;

                    eColorOrdering colorOrder;
                    eRasterFormat d3dRasterFormat;

                    bool isD3DFORMATImportant = true;

                    bool hasActualD3DFormat = false;
                    D3DFORMAT actualD3DFormat;

                    // Do special logic for palettized textures.
                    // (thank you DK22Pac)
                    if (platformTex->paletteType != PALETTE_NONE)
                    {
                        // This overrides the D3DFORMAT field.
                        // We are not forced to using the eRasterFormat property.
                        isD3DFORMATImportant = false;

                        colorOrder = COLOR_RGBA;

                        d3dRasterFormat = platformTex->rasterFormat;

                        hasActualD3DFormat = true;
                        actualD3DFormat = D3DFMT_P8;

                        isValidFormat = ( d3dFormat == D3DFMT_P8 );
                    }
                    else
                    {
                        // Set it for clarity sake.
                        isD3DFORMATImportant = true;

                        if (d3dFormat == D3DFMT_A8R8G8B8)
                        {
                            d3dRasterFormat = RASTER_8888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X8R8G8B8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_R8G8B8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_R5G6B5)
                        {
                            d3dRasterFormat = RASTER_565;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X1R5G5B5)
                        {
                            d3dRasterFormat = RASTER_555;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A1R5G5B5)
                        {
                            d3dRasterFormat = RASTER_1555;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A4R4G4B4)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A8B8G8R8)
                        {
                            d3dRasterFormat = RASTER_8888;

                            colorOrder = COLOR_RGBA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X8B8G8R8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_RGBA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_DXT1)
                        {
                            if (platformTex->hasAlpha)
                            {
                                d3dRasterFormat = RASTER_1555;
                            }
                            else
                            {
                                d3dRasterFormat = RASTER_565;
                            }

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_DXT2 || d3dFormat == D3DFMT_DXT3)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_DXT4 || d3dFormat == D3DFMT_DXT5)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_P8)
                        {
                            // We cannot be a palette texture without having actual palette data.
                            isValidFormat = false;
                        }
                    }

                    if ( isValidFormat == false )
                    {
                        if ( isD3DFORMATImportant == true )
                        {
                            throw RwException( "invalid D3DFORMAT in texture " + theTexture->GetName() );
                        }
                        else
                        {
                            if ( engineIgnoreSecureWarnings == false )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has a wrong D3DFORMAT field (ignoring)" );
                            }
                        }

                        // Fix it (if possible).
                        if ( hasActualD3DFormat )
                        {
                            d3dFormat = actualD3DFormat;

                            platformTex->d3dFormat = actualD3DFormat;
                        }
                        else
                        {
                            assert( 0 );
                        }
                    }

                    eRasterFormat rasterFormat = platformTex->rasterFormat;

                    if ( rasterFormat != d3dRasterFormat )
                    {
                        if ( isRasterFormatRequired || !engineIgnoreSecureWarnings )
                        {
                            if ( engineWarningLevel >= 3 )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid raster format (ignoring)" );
                            }
                        }

                        // Fix it.
                        platformTex->rasterFormat = d3dRasterFormat;
                    }

                    // Store the color ordering.
                    platformTex->colorOrdering = colorOrder;

                    // When reading a texture native, we must have a D3DFORMAT.
                    platformTex->hasD3DFormat = true;
                }
                // - Verify depth.
                {
                    bool hasInvalidDepth = false;

                    if (platformTex->paletteType == PALETTE_4BIT)
                    {
                        if (depth != 4 && depth != 8)
                        {
                            hasInvalidDepth = true;
                        }
                    }
                    else if (platformTex->paletteType == PALETTE_8BIT)
                    {
                        if (depth != 8)
                        {
                            hasInvalidDepth = true;
                        }
                    }

                    if (hasInvalidDepth == true)
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has an invalid depth" );

                        // We cannot fix an invalid depth.
                    }
                }

                if (platformTex->paletteType != PALETTE_NONE)
                {
                    uint32 reqPalItemCount = getD3DPaletteCount( platformTex->paletteType );

                    uint32 palDepth = Bitmap::getRasterFormatDepth( platformTex->rasterFormat );

                    assert( palDepth != 0 );

                    size_t paletteDataSize = getRasterDataSize( reqPalItemCount, palDepth );

                    // Check whether we have palette data in the stream.
                    texNativeImageStruct.check_read_ahead( paletteDataSize );

                    void *palData = engineInterface->PixelAllocate( paletteDataSize );

                    try
                    {
	                    texNativeImageStruct.read( palData, paletteDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( palData );

                        throw;
                    }

                    // Store the palette.
                    platformTex->palette = palData;
                    platformTex->paletteSize = reqPalItemCount;
                }

                mipGenLevelGenerator mipLevelGen( dimInfo.width, dimInfo.height );

                if ( !mipLevelGen.isValidLevel() )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has invalid dimensions" );
                }

                uint32 mipmapCount = 0;

                uint32 processedMipmapCount = 0;

                uint32 dxtCompression = platformTex->dxtCompression;

                bool hasDamagedMipmaps = false;

                for (uint32 i = 0; i < maybeMipmapCount; i++)
                {
                    bool couldEstablishLevel = true;

	                if (i > 0)
                    {
                        couldEstablishLevel = mipLevelGen.incrementLevel();
                    }

                    if (!couldEstablishLevel)
                    {
                        break;
                    }

                    // Create a new mipmap layer.
                    NativeTextureD3D::mipmapLayer newLayer;

                    newLayer.layerWidth = mipLevelGen.getLevelWidth();
                    newLayer.layerHeight = mipLevelGen.getLevelHeight();

                    // Process dimensions.
                    uint32 texWidth = newLayer.layerWidth;
                    uint32 texHeight = newLayer.layerHeight;
                    {
		                // DXT compression works on 4x4 blocks,
		                // no smaller values allowed
		                if (dxtCompression != 0)
                        {
			                texWidth = ALIGN_SIZE( texWidth, 4u );
                            texHeight = ALIGN_SIZE( texHeight, 4u );
		                }
                    }

                    newLayer.width = texWidth;
                    newLayer.height = texHeight;

	                uint32 texDataSize = texNativeImageStruct.readUInt32();

                    // We started processing this mipmap.
                    processedMipmapCount++;

                    // Verify the data size.
                    bool isValidMipmap = true;
                    {
                        uint32 texItemCount = ( texWidth * texHeight );

                        uint32 actualDataSize = 0;

                        if (dxtCompression != 0)
                        {
                            actualDataSize = getDXTRasterDataSize(dxtCompression, texItemCount);
                        }
                        else
                        {
                            actualDataSize = getRasterDataSize(texItemCount, depth);
                        }

                        if (actualDataSize != texDataSize)
                        {
                            isValidMipmap = false;
                        }
                    }

                    if ( !isValidMipmap )
                    {
                        // Even the Rockstar games texture generator appeared to have problems with mipmap generation.
                        // This is why textures appear to have the size of zero.

                        if (texDataSize != 0)
                        {
                            if ( !engineIgnoreSecureWarnings )
                            {
                               engineInterface->PushWarning( "texture " + theTexture->GetName() + " has damaged mipmaps (ignoring)" );
                            }

                            hasDamagedMipmaps = true;
                        }

                        // Skip the damaged bytes.
                        if (texDataSize != 0)
                        {
                            texNativeImageStruct.skip( texDataSize );
                        }
                        break;
                    }

                    // We first have to check whether there is enough data in the stream.
                    // Otherwise we would just flood the memory in case of an error;
                    // that could be abused by exploiters.
                    texNativeImageStruct.check_read_ahead( texDataSize );
                    
                    void *texelData = engineInterface->PixelAllocate( texDataSize );

                    try
                    {
	                    texNativeImageStruct.read( texelData, texDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( texelData );

                        throw;
                    }

                    // Store mipmap properties.
	                newLayer.dataSize = texDataSize;

                    // Store the image data pointer.
	                newLayer.texels = texelData;

                    // Put the layer.
                    platformTex->mipmaps.push_back( newLayer );

                    mipmapCount++;
                }
                
                if ( mipmapCount == 0 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " is empty" );
                }

                // mipmapCount can only be smaller than maybeMipmapCount.
                // This is logically true and would be absurd to assert here.

                if ( processedMipmapCount < maybeMipmapCount )
                {
                    // Skip the remaining mipmaps (most likely zero-sized).
                    bool hasSkippedNonZeroSized = false;

                    for ( uint32 n = processedMipmapCount; n < maybeMipmapCount; n++ )
                    {
                        uint32 mipSize = texNativeImageStruct.readUInt32();

                        if ( mipSize != 0 )
                        {
                            hasSkippedNonZeroSized = true;

                            // Skip the section.
                            texNativeImageStruct.skip( mipSize );
                        }
                    }

                    if ( !engineIgnoreSecureWarnings && !hasDamagedMipmaps )
                    {
                        // Print the debug message.
                        if ( !hasSkippedNonZeroSized )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " has zero sized mipmaps" );
                        }
                        else
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " violates mipmap rules" );
                        }
                    }
                }

                // Fix filtering mode.
                fixFilteringMode( *theTexture, mipmapCount );

                // - Verify auto mipmap
                {
                    bool hasAutoMipmaps = platformTex->autoMipmaps;

                    if ( hasAutoMipmaps )
                    {
                        bool canHaveAutoMipmaps = ( mipmapCount == 1 );

                        if ( !canHaveAutoMipmaps )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid auto-mipmap flag (fixing)" );

                            platformTex->autoMipmaps = false;
                        }
                    }
                }
            }
            else
            {
                engineInterface->PushWarning( "failed to find texture native image struct in D3D texture native" );
            }
        }
        catch( ... )
        {
            texNativeImageStruct.LeaveContext();

            throw;
        }

        texNativeImageStruct.LeaveContext();
    }

    // Read extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static PluginDependantStructRegister <d3dNativeTextureTypeProvider, RwInterfaceFactory_t> d3dNativeTexturePluginRegister( engineFactory );

void registerD3DNativeTexture( Interface *engineInterface )
{
    d3dNativeTextureTypeProvider *d3dTexEnv = d3dNativeTexturePluginRegister.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( d3dTexEnv )
    {
        RegisterNativeTextureType( engineInterface, "Direct3D", d3dTexEnv, sizeof( NativeTextureD3D ) );
    }
}

};