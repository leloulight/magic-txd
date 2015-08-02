#include "StdInc.h"

#include "txdread.gc.hxx"

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

                inputProvider.readStruct( metaHeader );

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

                // Parse the raster format.
                eRasterFormat rasterFormat;
                ePaletteType paletteType;
                bool hasMipmaps;
                bool autoMipmaps;

                uint32 rasterFormatFlags = metaHeader.rasterFormat;

                readRasterFormatFlags( rasterFormatFlags, rasterFormat, paletteType, hasMipmaps, autoMipmaps );

                // Read advanced things.
                uint8 rasterType = ( rasterFormatFlags & 0xFF );



                throw RwException( "reading gamecube native textures not implemented yet" );
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

bool gamecubeNativeTextureTypeProvider::GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut )
{
    return false;
}

bool gamecubeNativeTextureTypeProvider::AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, texNativeTypeProvider::acquireFeedback_t& acquireFeedback )
{
    return false;
}

void gamecubeNativeTextureTypeProvider::ClearMipmaps( Interface *engineInterface, void *objMem )
{

}

void gamecubeNativeTextureTypeProvider::GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut )
{
    const NativeTextureGC *nativeTex = (const NativeTextureGC*)objMem;

    uint32 mipmapCount = nativeTex->mipmaps.size();

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