#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.common.hxx"

#include "txdread.d3d9.hxx"

#include "streamutil.hxx"

#include "pixelformat.hxx"

#include "pixelutil.hxx"

namespace rw
{

// DDS main header flags.
#define DDSD_CAPS               0x00000001
#define DDSD_HEIGHT             0x00000002
#define DDSD_WIDTH              0x00000004
#define DDSD_PITCH              0x00000008
#define DDSD_PIXELFORMAT        0x00001000
#define DDSD_MIPMAPCOUNT        0x00020000
#define DDSD_LINEARSIZE         0x00080000
#define DDSD_DEPTH              0x00800000

// Pixel format flags.
// TODO: there are some interesting new fields, we gotta add support for them.
#define DDPF_ALPHAPIXELS        0x00000001
#define DDPF_ALPHA              0x00000002
#define DDPF_FOURCC             0x00000004
#define DDPF_PALETTEINDEXED4    0x00000008
#define DDPF_PALETTEINDEXEDTO8  0x00000010
#define DDPF_PALETTEINDEXED8    0x00000020
#define DDPF_RGB                0x00000040
#define DDPF_COMPRESSED         0x00000080
#define DDPF_RGBTOYUV           0x00000100
#define DDPF_YUV                0x00000200
#define DDPF_ZBUFFER            0x00000400
#define DDPF_PALETTEINDEXED1    0x00000800
#define DDPF_PALETTEINDEXED2    0x00001000
#define DDPF_ZPIXELS            0x00002000
#define DDPF_STENCILBUFFER      0x00004000
#define DDPF_ALPHAPREMULT       0x00008000
#define DDPF_LUMINANCE          0x00020000
#define DDPF_BUMPLUMINANCE      0x00040000
#define DDPF_BUMPDUDV           0x00080000

// Capability flags.
#define DDSCAPS_RESERVED1       0x00000001
#define DDSCAPS_ALPHA           0x00000002
#define DDSCAPS_BACKBUFFER      0x00000004
#define DDSCAPS_COMPLEX         0x00000008
#define DDSCAPS_FLIP            0x00000010
#define DDSCAPS_FRONTBUFFER     0x00000020
#define DDSCAPS_OFFSCREENPLAIN  0x00000040
#define DDSCAPS_OVERLAY         0x00000080
#define DDSCAPS_PALETTE         0x00000100
#define DDSCAPS_PRIMARYSURFACE  0x00000200
#define DDSCAPS_SYSTEMMEMORY    0x00000800
#define DDSCAPS_TEXTURE         0x00001000
#define DDSCAPS_3DDEVICE        0x00002000
#define DDSCAPS_VIDEOMEMORY     0x00004000
#define DDSCAPS_VISIBLE         0x00008000
#define DDSCAPS_WRITEONLY       0x00010000
#define DDSCAPS_ZBUFFER         0x00020000
#define DDSCAPS_OWNDC           0x00040000
#define DDSCAPS_LIVEVIDEO       0x00080000
#define DDSCAPS_HWCODEC         0x00100000
#define DDSCAPS_MODEX           0x00200000
#define DDSCAPS_MIPMAP          0x00400000
#define DDSCAPS_ALLOCONLOAD     0x04000000
#define DDSCAPS_VIDEOPORT       0x08000000
#define DDSCAPS_LOCALVIDMEM     0x10000000
#define DDSCAPS_NONLOCALVIDMEM  0x20000000
#define DDSCAPS_STANDARDVGAMODE 0x40000000
#define DDSCAPS_OPTIMIZED       0x80000000

#define DDSCAPS2_HARDWAREDEINTERLACE    0x00000002
#define DDSCAPS2_HINTDYNAMIC            0x00000004
#define DDSCAPS2_HINTSTATIC             0x00000008
#define DDSCAPS2_TEXTUREMANAGE          0x00000010
#define DDSCAPS2_OPAQUE                 0x00000080
#define DDSCAPS2_HINTANTIALIASING       0x00000100
#define DDSCAPS2_CUBEMAP                0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX      0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX      0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY      0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY      0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ      0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ	    0x00008000
#define DDSCAPS2_MIPMAPSUBLEVEL         0x00010000
#define DDSCAPS2_D3DTEXTUREMANAGE       0x00020000
#define DDSCAPS2_DONOTPERSIST           0x00040000
#define DDSCAPS2_STEREOSURFACELEFT      0x00080000
#define DDSCAPS2_VOLUME                 0x00200000

struct dds_pixelformat
{
    endian::little_endian <uint32> dwSize;
    endian::little_endian <uint32> dwFlags;
    endian::little_endian <uint32> dwFourCC;
    endian::little_endian <uint32> dwRGBBitCount;
    endian::little_endian <uint32> dwRBitMask;
    endian::little_endian <uint32> dwGBitMask;
    endian::little_endian <uint32> dwBBitMask;
    endian::little_endian <uint32> dwABitMask;
};

struct dds_header
{
    endian::little_endian <uint32> dwSize;
    endian::little_endian <uint32> dwFlags;
    endian::little_endian <uint32> dwHeight;
    endian::little_endian <uint32> dwWidth;
    endian::little_endian <uint32> dwPitchOrLinearSize;
    endian::little_endian <uint32> dwDepth;
    endian::little_endian <uint32> dwMipmapCount;
    endian::little_endian <uint32> dwReserved1[11];
    
    dds_pixelformat ddspf;

    endian::little_endian <uint32> dwCaps;
    endian::little_endian <uint32> dwCaps2;
    endian::little_endian <uint32> dwCaps3;
    endian::little_endian <uint32> dwCaps4;
    endian::little_endian <uint32> dwReserved2;

    static inline bool getBitDepth( uint32 dwFourCC, uint32& depthOut )
    {
        switch( dwFourCC )
        {
        case D3DFMT_R8G8B8:             depthOut = 24; break;
        case D3DFMT_A8R8G8B8:           depthOut = 32; break;
        case D3DFMT_X8R8G8B8:           depthOut = 32; break;
        case D3DFMT_R5G6B5:             depthOut = 16; break;
        case D3DFMT_X1R5G5B5:           depthOut = 16; break;
        case D3DFMT_A1R5G5B5:           depthOut = 16; break;
        case D3DFMT_A4R4G4B4:           depthOut = 16; break;
        case D3DFMT_R3G3B2:             depthOut = 8; break;
        case D3DFMT_A8:                 depthOut = 8; break;
        case D3DFMT_A8R3G3B2:           depthOut = 16; break;
        case D3DFMT_X4R4G4B4:           depthOut = 16; break;
        case D3DFMT_A2B10G10R10:        depthOut = 32; break;
        case D3DFMT_A8B8G8R8:           depthOut = 32; break;
        case D3DFMT_X8B8G8R8:           depthOut = 32; break;
        case D3DFMT_G16R16:             depthOut = 32; break;
        case D3DFMT_A2R10G10B10:        depthOut = 32; break;
        case D3DFMT_A16B16G16R16:       depthOut = 64; break;
        case D3DFMT_A8P8:               depthOut = 16; break;
        case D3DFMT_P8:                 depthOut = 8; break;
        case D3DFMT_L8:                 depthOut = 8; break;
        case D3DFMT_A8L8:               depthOut = 16; break;
        case D3DFMT_A4L4:               depthOut = 8; break;
        case D3DFMT_V8U8:               depthOut = 16; break;
        case D3DFMT_L6V5U5:             depthOut = 16; break;
        case D3DFMT_X8L8V8U8:           depthOut = 32; break;
        case D3DFMT_Q8W8V8U8:           depthOut = 32; break;
        case D3DFMT_V16U16:             depthOut = 32; break;
        case D3DFMT_A2W10V10U10:        depthOut = 32; break;
        case D3DFMT_UYVY:               depthOut = 8; break;
        case D3DFMT_R8G8_B8G8:          depthOut = 16; break;
        case D3DFMT_YUY2:               depthOut = 8; break;
        case D3DFMT_G8R8_G8B8:          depthOut = 16; break;
        case D3DFMT_DXT1:               depthOut = 4; break;
        case D3DFMT_DXT2:               depthOut = 8; break;
        case D3DFMT_DXT3:               depthOut = 8; break;
        case D3DFMT_DXT4:               depthOut = 8; break;
        case D3DFMT_DXT5:               depthOut = 8; break;
        case D3DFMT_D16_LOCKABLE:       depthOut = 16; break;
        case D3DFMT_D32:                depthOut = 32; break;
        case D3DFMT_D15S1:              depthOut = 16; break;
        case D3DFMT_D24S8:              depthOut = 32; break;
        case D3DFMT_D24X8:              depthOut = 32; break;
        case D3DFMT_D24X4S4:            depthOut = 32; break;
        case D3DFMT_D16:                depthOut = 16; break;
        case D3DFMT_D32F_LOCKABLE:      depthOut = 32; break;
        case D3DFMT_D24FS8:             depthOut = 32; break;
        case D3DFMT_L16:                depthOut = 16; break;
        case D3DFMT_INDEX16:            depthOut = 16; break;
        case D3DFMT_INDEX32:            depthOut = 32; break;
        case D3DFMT_Q16W16V16U16:       depthOut = 64; break;
        case D3DFMT_R16F:               depthOut = 16; break;
        case D3DFMT_G16R16F:            depthOut = 32; break;
        case D3DFMT_A16B16G16R16F:      depthOut = 64; break;
        case D3DFMT_R32F:               depthOut = 32; break;
        case D3DFMT_G32R32F:            depthOut = 64; break;
        case D3DFMT_A32B32G32R32F:      depthOut = 128; break;
        case D3DFMT_CxV8U8:             depthOut = 16; break;
        default: return false;
        }

        return true;
    }

    static inline bool calculateSurfaceDimensions( uint32 dwFourCC, uint32 layerWidth, uint32 layerHeight, uint32& mipWidthOut, uint32& mipHeightOut )
    {
        // Per recommendation of MSDN, we calculate the stride ourselves.
        switch( dwFourCC )
        {
        case D3DFMT_R8G8B8:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_A16B16G16R16:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A8L8:
        case D3DFMT_A4L4:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D32:
        case D3DFMT_D15S1:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D16:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_L16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_R16F:
        case D3DFMT_G16R16F:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_G32R32F:
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_CxV8U8:
            // uncompressed.
            mipWidthOut = layerWidth;
            mipHeightOut = layerHeight;
            return true;
        case D3DFMT_UYVY:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_YUY2:
        case D3DFMT_G8R8_G8B8:
            // 2x2 block format.
            mipWidthOut = ALIGN_SIZE( layerWidth, 2u );
            mipHeightOut = ALIGN_SIZE( layerHeight, 2u );
            return true;
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // block compression format.
            mipWidthOut = ALIGN_SIZE( layerWidth, 4u );
            mipHeightOut = ALIGN_SIZE( layerHeight, 4u );
            return true;
        }

        return false;
    }

    static inline bool doesFormatSupportPitch( D3DFORMAT d3dFormat )
    {
        switch( d3dFormat )
        {
        case D3DFMT_R8G8B8:
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_R3G3B2:
        case D3DFMT_A8:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_X4R4G4B4:
        case D3DFMT_A2B10G10R10:
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        case D3DFMT_G16R16:
        case D3DFMT_A2R10G10B10:
        case D3DFMT_A16B16G16R16:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_L8:
        case D3DFMT_A8L8:
        case D3DFMT_A4L4:
        case D3DFMT_V8U8:
        case D3DFMT_L6V5U5:
        case D3DFMT_X8L8V8U8:
        case D3DFMT_Q8W8V8U8:
        case D3DFMT_V16U16:
        case D3DFMT_A2W10V10U10:
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D32:
        case D3DFMT_D15S1:
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D16:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D24FS8:
        case D3DFMT_L16:
        case D3DFMT_Q16W16V16U16:
        case D3DFMT_R16F:
        case D3DFMT_G16R16F:
        case D3DFMT_A16B16G16R16F:
        case D3DFMT_R32F:
        case D3DFMT_G32R32F:
        case D3DFMT_A32B32G32R32F:
        case D3DFMT_CxV8U8:
            // uncompressed.
            return true;
        case D3DFMT_UYVY:
        case D3DFMT_R8G8_B8G8:
        case D3DFMT_YUY2:
        case D3DFMT_G8R8_G8B8:
            // 2x2 block format.
            return false;
        case D3DFMT_DXT1:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            // block compression format.
            return false;
        }

        // No idea about anything else.
        // Let's say it does not support pitch.
        return false;
    }
};

inline void calculateSurfaceDimensions(
    uint32 layerWidth, uint32 layerHeight, bool hasValidFourCC, uint32 fourCC,
    uint32& surfWidthOut, uint32& surfHeightOut
)
{
    bool hasValidDimensions = false;

    if ( !hasValidDimensions )
    {
        // We most likely want to decide by four cc.
        if ( hasValidFourCC )
        {
            bool couldGet = dds_header::calculateSurfaceDimensions(
                fourCC, layerWidth, layerHeight, surfWidthOut, surfHeightOut
            );

            if ( couldGet )
            {
                // Most likely correct.
                hasValidDimensions = true;
            }
        }
    }

    if ( !hasValidDimensions )
    {
        // For formats we do not know about, we assume the texture has proper dimensions already.
        surfWidthOut = layerWidth;
        surfHeightOut = layerHeight;

        hasValidDimensions = true;
    }
}

bool d3d9NativeTextureTypeProvider::IsNativeImageFormat( Interface *engineInterface, Stream *inputStream ) const
{
    // We check kinda thoroughly.
    endian::little_endian <uint32> magic;
    {
        size_t magicReadCount = inputStream->read( &magic, sizeof( magic ) );

        if ( magicReadCount != sizeof( magic ) )
        {
            return false;
        }
    }

    if ( magic != ' SDD' )
    {
        return false;
    }

    dds_header header;
    {
        size_t headerReadCount = inputStream->read( &header, sizeof( header ) );

        if ( headerReadCount != sizeof( header ) )
        {
            return false;
        }
    }

    // Check some basic semantic properties of the header.
    if ( header.dwSize != sizeof( header ) )
    {
        return false;
    }

    if ( header.ddspf.dwSize != sizeof( header.ddspf ) )
    {
        return false;
    }

    if ( header.ddspf.dwFourCC == MAKEFOURCC( 'D', 'X', '1', '0' ) )
    {
        // Direct3D 9 cannot understand DirectX 10 rasters.
        return false;
    }

    uint32 ddsFlags = header.dwFlags;

    uint32 pixelFormatFlags = header.ddspf.dwFlags;

    uint32 fourCC = header.ddspf.dwFourCC;
    bool hasValidFourCC = ( pixelFormatFlags & DDPF_FOURCC ) != 0;

    uint32 layerWidth = header.dwWidth;
    uint32 layerHeight = header.dwHeight;

    uint32 bitDepth;

    bool hasValidBitDepth = false;
    
    if ( !hasValidBitDepth )
    {
        // We could have a valid bit depth stored in the header instead.
        if ( pixelFormatFlags & ( DDPF_RGB | DDPF_YUV | DDPF_LUMINANCE ) )
        {
            // We take the value from the header.
            bitDepth = header.ddspf.dwRGBBitCount;

            hasValidBitDepth = true;
        }
        else if ( pixelFormatFlags & DDPF_PALETTEINDEXED1 )
        {
            bitDepth = 1;

            hasValidBitDepth = true;
        }
        else if ( pixelFormatFlags & DDPF_PALETTEINDEXED2 )
        {
            bitDepth = 2;

            hasValidBitDepth = true;
        }
        else if ( pixelFormatFlags & DDPF_PALETTEINDEXED4 )
        {
            bitDepth = 4;

            hasValidBitDepth = true;
        }
        else if ( pixelFormatFlags & DDPF_PALETTEINDEXED8 )
        {
            bitDepth = 8;

            hasValidBitDepth = true;
        }
    }

    if ( !hasValidBitDepth )
    {
        if ( hasValidFourCC )
        {
            // We get it from the D3DFORMAT field.
            bool couldGet = dds_header::getBitDepth( fourCC, bitDepth );

            if ( couldGet )
            {
                hasValidBitDepth = true;
            }
        }
    }

    if ( !hasValidBitDepth )
    {
        // We do not know the depth of every format.
        // Zero is a valid value then.
        bitDepth = 0;
    }

    // Get the pitch or linear size.
    // Either way, we should be able to understand this.
    uint32 mainSurfacePitchOrLinearSize = header.dwPitchOrLinearSize;

    bool isLinearSize = ( ( ddsFlags & DDSD_LINEARSIZE ) != 0 );
    bool isPitch ( ( ddsFlags & DDSD_PITCH ) != 0 );

    // Check that we have all the mipmap data.
    uint32 mipCount = ( ( ddsFlags & DDSD_MIPMAPCOUNT ) != 0 ? header.dwMipmapCount : 1 );

    mipGenLevelGenerator mipGen( layerWidth, layerHeight );

    if ( mipGen.isValidLevel() )
    {
        for ( uint32 n = 0; n < mipCount; n++ )
        {
            bool hasThisLevel = true;

            if ( n != 0 )
            {
                hasThisLevel = mipGen.incrementLevel();
            }

            if ( !hasThisLevel )
            {
                break;
            }

            // Get the surface dimensions.
            uint32 surfWidth, surfHeight;

            calculateSurfaceDimensions( mipGen.getLevelWidth(), mipGen.getLevelHeight(), hasValidFourCC, fourCC, surfWidth, surfHeight );

            // Verify that we have this mipmap data.
            uint32 mipLevelDataSize;

            bool hasDataSize = false;

            if ( n == 0 )
            {
                if ( isLinearSize )
                {
                    mipLevelDataSize = mainSurfacePitchOrLinearSize;

                    hasDataSize = true;
                }
                else if ( isPitch )
                {
                    mipLevelDataSize = getRasterDataSizeByRowSize( mainSurfacePitchOrLinearSize, surfHeight );

                    hasDataSize = true;
                }
            }
            
            if ( !hasDataSize )
            {
                uint32 mipStride;

                // For this we need to have valid bit depth.
                if ( hasValidBitDepth )
                {
                    mipStride = getRasterDataRowSize( surfWidth, bitDepth, 1 );  // byte alignment.
                }
                else
                {
                    // No way we can read this.
                    break;
                }

                mipLevelDataSize = getRasterDataSizeByRowSize( mipStride, surfHeight );
            }

            // Check the stream for the data.
            skipAvailable( inputStream, mipLevelDataSize );
        }
    }

    return true;
}

struct rgba_mask_info
{
    uint32 redMask;
    uint32 greenMask;
    uint32 blueMask;
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const rgba_mask_info rgba_masks[] =
{
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, 24, D3DFMT_R8G8B8 },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 32, D3DFMT_A8R8G8B8 },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000, 32, D3DFMT_X8R8G8B8 },
    { 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000, 16, D3DFMT_R5G6B5 },
    { 0x00007C00, 0x000003E0, 0x0000001F, 0x00000000, 16, D3DFMT_X1R5G5B5 },
    { 0x00007C00, 0x000003E0, 0x0000001F, 0x00008000, 16, D3DFMT_A1R5G5B5 },
    { 0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000, 16, D3DFMT_A4R4G4B4 },
    { 0x000000E0, 0x0000001C, 0x00000003, 0x00000000, 8,  D3DFMT_R3G3B2 },
    { 0x000000E0, 0x0000001C, 0x00000003, 0x0000FF00, 16, D3DFMT_A8R3G3B2 },
    { 0x00000F00, 0x000000F0, 0x0000000F, 0x00000000, 16, D3DFMT_X4R4G4B4 },
    { 0x000003FF, 0x000FFC00, 0x3FF00000, 0xC0000000, 32, D3DFMT_A2B10G10R10 },
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000, 32, D3DFMT_A8B8G8R8 },
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000, 32, D3DFMT_X8B8G8R8 },
    { 0x0000FFFF, 0xFFFF0000, 0x00000000, 0x00000000, 32, D3DFMT_G16R16 },
    { 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000, 32, D3DFMT_A2R10G10B10 }
};

struct yuv_mask_info
{
    uint32 yMask;
    uint32 uMask;
    uint32 vMask;
    uint32 depth;

    D3DFORMAT format;
};

static const yuv_mask_info yuv_masks[] =
{
    { 0x00000000, 0x000000FF, 0x0000FF00, 16, D3DFMT_V8U8 },
    { 0x00000000, 0x0000FFFF, 0xFFFF0000, 32, D3DFMT_V16U16 }
};

struct lum_mask_info
{
    uint32 lumMask;
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const lum_mask_info lum_masks[] =
{
    { 0x000000FF, 0x00000000,  8, D3DFMT_L8 },
    { 0x000000FF, 0x0000FF00, 16, D3DFMT_A8L8 },
    { 0x0000000F, 0x000000F0,  8, D3DFMT_A4L4 }
};

struct alpha_mask_info
{
    uint32 alphaMask;
    uint32 depth;

    D3DFORMAT format;
};

static const alpha_mask_info alpha_masks[] =
{
    { 0x000000FF, 8, D3DFMT_A8 }
};

#define ELMCNT( x ) ( sizeof(x) / sizeof(*x) )

inline bool getDDSMappingFromD3DFormat(
    D3DFORMAT d3dFormat,
    bool& isRGBOut, bool& isLumOut, bool& isAlphaOnlyOut, bool& isYUVOut, bool& isFourCCOut,
    uint32& redMaskOut, uint32& greenMaskOut, uint32& blueMaskOut, uint32& alphaMaskOut, uint32& fourCCOut
)
{
    uint32 redMask = 0;
    uint32 greenMask = 0;
    uint32 blueMask = 0;
    uint32 alphaMask = 0;
    uint32 fourCC = 0;

    bool hasDirectMapping = false;

    bool isRGBA = false;

    if ( !hasDirectMapping )
    {
        // Check RGBA first.
        uint32 rgba_count = ELMCNT( rgba_masks );

        for ( uint32 n = 0; n < rgba_count; n++ )
        {
            const rgba_mask_info& curInfo = rgba_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.redMask;
                greenMask = curInfo.greenMask;
                blueMask = curInfo.blueMask;
                alphaMask = curInfo.alphaMask;

                isRGBA = true;
                hasDirectMapping = true;
                break;
            }
        }
    }

    bool isLum = false;

    if ( !hasDirectMapping )
    {
        // Check luminance next.
        uint32 lum_count = ELMCNT( lum_masks );

        for ( uint32 n = 0; n < lum_count; n++ )
        {
            const lum_mask_info& curInfo = lum_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.lumMask;
                alphaMask = curInfo.alphaMask;

                isLum = true;
                hasDirectMapping = true;
                break;
            }
        }
    }

    bool isYUV = false;

    if ( !hasDirectMapping )
    {
        // Check YUV next.
        uint32 yuv_count = ELMCNT( yuv_masks );

        for ( uint32 n = 0; n < yuv_count; n++ )
        {
            const yuv_mask_info& curInfo = yuv_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                redMask = curInfo.yMask;
                greenMask = curInfo.uMask;
                blueMask = curInfo.vMask;

                isYUV = true;
                hasDirectMapping = true;
                break;
            }
        }
    }

    bool isAlphaOnly = false;

    if ( !hasDirectMapping )
    {
        // Check alpha next.
        uint32 alpha_count = ELMCNT( alpha_masks );

        for ( uint32 n = 0; n < alpha_count; n++ )
        {
            const alpha_mask_info& curInfo = alpha_masks[ n ];

            if ( curInfo.format == d3dFormat )
            {
                alphaMask = curInfo.alphaMask;

                isAlphaOnly = true;
                hasDirectMapping = true;
                break;
            }
        }
    }

    bool isFourCC = false;

    if ( !hasDirectMapping )
    {
        // Some special D3DFORMAT fields are supported by DDS.
        if ( d3dFormat == D3DFMT_DXT1 ||
             d3dFormat == D3DFMT_DXT2 ||
             d3dFormat == D3DFMT_DXT3 ||
             d3dFormat == D3DFMT_DXT4 ||
             d3dFormat == D3DFMT_DXT5 )
        {
            fourCC = d3dFormat;

            isFourCC = true;
            hasDirectMapping = true;
        }
    }

    if ( hasDirectMapping )
    {
        isRGBOut = isRGBA;
        isLumOut = isLum;
        isYUVOut = isYUV;
        isAlphaOnlyOut = isAlphaOnly;
        isFourCCOut = isFourCC;

        redMaskOut = redMask;
        greenMaskOut = greenMask;
        blueMaskOut = blueMask;
        alphaMaskOut = alphaMask;
        fourCCOut = fourCC;
    }

    return hasDirectMapping;
}

void d3d9NativeTextureTypeProvider::SerializeNativeImage( Interface *engineInterface, Stream *inputStream, void *objMem ) const
{
    // Simply write out this native texture's content.
    const NativeTextureD3D9 *nativeTex = (const NativeTextureD3D9*)objMem;

    size_t mipmapCount = nativeTex->mipmaps.size();

    if ( mipmapCount == 0 )
    {
        throw RwException( "attempt to serialize empty Direct3D 9 native texture" );
    }

    // First, we write the magic number.
    endian::little_endian <uint32> magic( ' SDD' );

    inputStream->write( &magic, sizeof( magic ) );

    const NativeTextureD3D9::mipmapLayer& baseLayer = nativeTex->mipmaps[ 0 ];

    // Decide how to write our native texture into the DDS.
    eRasterFormat srcRasterFormat = nativeTex->rasterFormat;
    uint32 srcDepth = nativeTex->depth;
    uint32 srcRowAlignment = getD3DTextureDataRowAlignment();
    eColorOrdering srcColorOrder = nativeTex->colorOrdering;

    ePaletteType srcPaletteType = nativeTex->paletteType;
    const void *srcPaletteData = nativeTex->palette;
    uint32 srcPaletteSize = nativeTex->paletteSize;

    uint32 dxtType = nativeTex->dxtCompression;

    bool d3dRasterFormatLink = nativeTex->d3dRasterFormatLink;
    d3dpublic::nativeTextureFormatHandler *usedFormatHandler = nativeTex->anonymousFormatLink;

    D3DFORMAT d3dFormat = nativeTex->d3dFormat;

    eRasterFormat dstRasterFormat;
    uint32 dstDepth;
    uint32 dstRowAlignment = 1;
    eColorOrdering dstColorOrder;

    ePaletteType dstPaletteType = PALETTE_NONE;
    uint32 dstPaletteSize = 0;

    D3DFORMAT dstFormat = d3dFormat;

    bool hasAlpha = nativeTex->hasAlpha;
    
    // Determine whether we can directly map this D3DFORMAT to a DDS format.
    uint32 redMask = 0;
    uint32 greenMask = 0;
    uint32 blueMask = 0;
    uint32 alphaMask = 0;
    uint32 fourCC = 0;

    bool isRGB = false;
    bool isLum = false;
    bool isYUV = false;
    bool isAlphaOnly = false;
    bool isFourCC = false;

    bool isRawRasterEncoding = false;
    bool hasDirectMapping = false;

    // We can also write a palette image.
    if ( srcPaletteType != PALETTE_NONE )
    {
        if ( srcPaletteType == PALETTE_4BIT || srcPaletteType == PALETTE_8BIT )
        {
            // We can directly write the palette indice.
            dstPaletteType = srcPaletteType;
        }
        else if ( srcPaletteType == PALETTE_4BIT_LSB )
        {
            dstPaletteType = PALETTE_4BIT;
        }
        else
        {
            // We encountered an unknown palette encoding, so expand to full width.
            dstPaletteType = PALETTE_8BIT;
        }

        // Determine the depth.
        if ( dstPaletteType == PALETTE_4BIT )
        {
            dstDepth = 4;
        }
        else if ( dstPaletteType == PALETTE_8BIT )
        {
            dstDepth = 8;
        }
        else
        {
            assert( 0 );
        }

        // The palette can only be written as 32bit RGBA.
        dstRasterFormat = RASTER_8888;
        dstColorOrder = COLOR_RGBA;

        // Check for direct mapping support.
        hasDirectMapping = ( srcDepth == dstDepth && srcPaletteType == dstPaletteType );

        isRawRasterEncoding = true;
    }
    else
    {
        // There are some formats where the artist does not want a direct acquisition.
        // We have to detect this here.
        bool wantsDirectAcquisition = true;

        bool recalculateD3DFORMATSupport = false;

        if ( d3dRasterFormatLink ) // RWCOMPRESS_NONE
        {
            if ( engineInterface->GetPreferPackedSampleExport() )
            {
                eRasterFormat redirRasterFormat;
                uint32 redirDepth;

                bool hasRedirect = RasterFormatSamplePackingTransform(
                    srcRasterFormat, srcDepth, hasAlpha,
                    redirRasterFormat, redirDepth
                );

                if ( hasRedirect )
                {
                    // We will have to check for D3DFORMAT support.
                    recalculateD3DFORMATSupport = true;
                 
                    // We take the same format specification but change the depth.
                    dstRasterFormat = redirRasterFormat;
                    dstDepth = redirDepth;
                    dstColorOrder = srcColorOrder;

                    wantsDirectAcquisition = false;
                }
            }
        }

        if ( wantsDirectAcquisition )
        {
            // Otherwise we write some D3DFORMAT encoding.
            hasDirectMapping =
                getDDSMappingFromD3DFormat(
                    d3dFormat, isRGB, isLum, isAlphaOnly, isYUV, isFourCC,
                    redMask, greenMask, blueMask, alphaMask, fourCC
                );

            // If we do not have any direct mapping, we will have to convert our native texture into something
            // understandable.
            if ( !hasDirectMapping )
            {
                // Writing into the DDS depends on what type of texture we have.
                if ( d3dRasterFormatLink || usedFormatHandler != NULL )
                {
                    // We will write a full quality color DDS.
                    dstRasterFormat = RASTER_8888;
                    dstDepth = 32;
                    dstColorOrder = COLOR_BGRA;
                }
                else
                {
                    throw RwException( "unsupported texture format in DDS serialization of Direct3D 9 native texture" );
                }
        
                recalculateD3DFORMATSupport = true;
            }
        }

        if ( recalculateD3DFORMATSupport )
        {
            // Calculate the D3DFORMAT that represents the new destination raster format.
            D3DFORMAT mapD3DFORMAT;

            bool canMapToFormat =
                getD3DFormatFromRasterType(
                    dstRasterFormat, PALETTE_NONE, dstColorOrder, dstDepth,
                    mapD3DFORMAT
                );

            assert( canMapToFormat == true );

            // Get a DDS mapping for it.
            bool hasIndirectMapping =
                getDDSMappingFromD3DFormat(
                    mapD3DFORMAT,
                    isRGB, isLum, isAlphaOnly, isYUV, isFourCC,
                    redMask, greenMask, blueMask, alphaMask, fourCC
                );

            assert( hasIndirectMapping == true );

            dstFormat = mapD3DFORMAT;
        }
    }

    // Calculate the real surface dimensions of the base layer now.
    uint32 baseDstSurfWidth, baseDstSurfHeight;

    bool couldGetSurfaceDimensions = false;

    if ( isRawRasterEncoding == false )
    {
        // Having a raw raster encoding means that our d3dFormat field does not matter.
        couldGetSurfaceDimensions = dds_header::calculateSurfaceDimensions(
            dstFormat, baseLayer.layerWidth, baseLayer.layerHeight,
            baseDstSurfWidth, baseDstSurfHeight
        );
    }

    if ( !couldGetSurfaceDimensions )
    {
        baseDstSurfWidth = baseLayer.layerWidth;
        baseDstSurfHeight = baseLayer.layerHeight;
    }

    // Decide whether a pitch is possible.
    // Also decide whether we want to output with pitch or with linear size.
    bool outputAsPitchOrLinearSize;     // true == pitch, false == linear size
    {
        if ( isRawRasterEncoding )
        {
            // Basically, raw raste encodings _must_ have a pitch.
            outputAsPitchOrLinearSize = true;
        }
        else
        {
            // This entirely depends on the destination D3DFORMAT.
            if ( dds_header::doesFormatSupportPitch( dstFormat ) == false )
            {
                outputAsPitchOrLinearSize = false;
            }
            else
            {
                outputAsPitchOrLinearSize = true;
            }

            assert( dds_header::doesFormatSupportPitch( d3dFormat ) == outputAsPitchOrLinearSize );
        }
    }

    // Get the bit depth.
    uint32 bitDepth;

    if ( !hasDirectMapping )
    {
        bitDepth = dstDepth;
    }
    else
    {
        bitDepth = srcDepth;
    }

    // Calculate the pitch and the data size of the destination data main surface.
    uint32 baseDstPitch;
    uint32 baseDstDataSize;
    {
        if ( outputAsPitchOrLinearSize == true )
        {
            // If we output as pitch, then we can be sure that the format properly supports a pitch.
            // This means we can just do the row size metrics, and the format is also alignable!
            baseDstPitch = getRasterDataRowSize( baseLayer.width, bitDepth, 1 );
        }
        else
        {
            assert( isRawRasterEncoding == false );

            // Here things start becoming more complicated.
            // This format does not support aligning, so we have to calculate its size somehow.
            // Even if the format did support aligning, it should not screw up this math.

            // Calculate the real data size.
            uint32 dxtType = 0;

            if ( dstFormat == D3DFMT_DXT1 )
            {
                dxtType = 1;
            }
            else if ( dstFormat == D3DFMT_DXT2 )
            {
                dxtType = 2;
            }
            else if ( dstFormat == D3DFMT_DXT3 )
            {
                dxtType = 3;
            }
            else if ( dstFormat == D3DFMT_DXT4 )
            {
                dxtType = 4;
            }
            else if ( dstFormat == D3DFMT_DXT5 )
            {
                dxtType = 5;
            }

            if ( dxtType != 0 )
            {
                baseDstDataSize = getDXTRasterDataSize( dxtType, baseDstSurfWidth * baseDstSurfHeight );
            }
            else
            {
                // Since we do not know the exact logic to calculate the size, we just do it through a virtual pitch.
                // It should work, anyway.
                uint32 virtualPitch = getRasterDataRowSize( baseDstSurfWidth, bitDepth, 1 );

                baseDstDataSize = getRasterDataSizeByRowSize( virtualPitch, baseLayer.height );
            }
        }
    }

    // Get special information about the first mipmap layer.
    uint32 mainSurfacePitchOrLinearSize = 0;

    // If we are compressed, we want to set the linear size.
    // Otherwise it is good to set the pitch.
    bool isLinearSize = false;
    bool isPitch = false;

    if ( outputAsPitchOrLinearSize == false )
    {
        mainSurfacePitchOrLinearSize = baseDstDataSize;

        isLinearSize = true;
    }
    else
    {
        mainSurfacePitchOrLinearSize = baseDstPitch;

        isPitch = true;
    }

    // Calculate main DDS flags.
    uint32 mainFlags = ( DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT );

    if ( mipmapCount > 1 )
    {
        mainFlags |= DDSD_MIPMAPCOUNT;
    }

    if ( isPitch )
    {
        mainFlags |= DDSD_PITCH;
    }

    if ( isLinearSize )
    {
        mainFlags |= DDSD_LINEARSIZE;
    }

    // Calculate the pixel format DDS flags.
    uint32 pixelFlags = 0;

    if ( alphaMask != 0 )
    {
        pixelFlags |= DDPF_ALPHAPIXELS;
    }

    if ( isRGB )
    {
        pixelFlags |= DDPF_RGB;
    }
    else if ( isLum )
    {
        pixelFlags |= DDPF_LUMINANCE;
    }
    else if ( isYUV )
    {
        pixelFlags |= DDPF_YUV;
    }
    else if ( isAlphaOnly )
    {
        pixelFlags |= DDPF_ALPHA;
    }
    else if ( isFourCC )
    {
        pixelFlags |= DDPF_FOURCC;
    }
    else if ( dstPaletteType != PALETTE_NONE )
    {
        if ( dstPaletteType == PALETTE_4BIT )
        {
            pixelFlags |= DDPF_PALETTEINDEXED4;
        }
        else if ( dstPaletteType == PALETTE_8BIT )
        {
            pixelFlags |= DDPF_PALETTEINDEXED8;
        }
    }

    // Calculate first capability flags.
    uint32 caps1 = ( DDSCAPS_TEXTURE );

    if ( mipmapCount > 1 )
    {
        caps1 |= DDSCAPS_COMPLEX;
    }

    // Calculate second capability flags.
    uint32 caps2 = 0;

    // Cache mipmap raster format we will use for output when we have anonymous formats.
    eRasterFormat mipRasterFormat;
    uint32 mipDepth;
    eColorOrdering mipColorOrder;

    if ( !hasDirectMapping )
    {
        if ( !d3dRasterFormatLink && usedFormatHandler )
        {
            usedFormatHandler->GetTextureRWFormat( mipRasterFormat, mipDepth, mipColorOrder );
        }
        else
        {
            // Basically, we transform from the source.
            mipRasterFormat = srcRasterFormat;
            mipDepth = srcDepth;
            mipColorOrder = srcColorOrder;
        }
    }

    // TODO: actually implement cube map logic for Direct3D 9 native texture.
    
    // Write header information.
    dds_header header;
    header.dwSize = sizeof( header );
    header.dwFlags = mainFlags;
    header.dwWidth = baseLayer.layerWidth;
    header.dwHeight = baseLayer.layerHeight;
    header.dwPitchOrLinearSize = mainSurfacePitchOrLinearSize;
    header.dwDepth = 0; // TODO.
    header.dwMipmapCount = (uint32)mipmapCount;
    memset( header.dwReserved1, 0, sizeof( header.dwReserved1 ) );
    header.ddspf.dwSize = sizeof( header.ddspf );
    header.ddspf.dwFlags = pixelFlags;
    header.ddspf.dwFourCC = fourCC;
    header.ddspf.dwRGBBitCount = ( isRGB || isLum || isYUV || isAlphaOnly || isRawRasterEncoding ? bitDepth : 0 );
    header.ddspf.dwRBitMask = redMask;
    header.ddspf.dwGBitMask = greenMask;
    header.ddspf.dwBBitMask = blueMask;
    header.ddspf.dwABitMask = alphaMask;
    header.dwCaps = caps1;
    header.dwCaps2 = caps2;
    header.dwCaps3 = 0;
    header.dwCaps4 = 0;
    header.dwReserved2 = 0;

    // Write the header.
    inputStream->write( &header, sizeof( header ) );

    // If we are palettized, we want to write the palette.
    if ( dstPaletteType != PALETTE_NONE )
    {
        assert( srcPaletteType != PALETTE_NONE );

        uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( srcRasterFormat );
        uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

        dstPaletteSize = getPaletteItemCount( dstPaletteType );

        uint32 dstPaletteDataSize = getPaletteDataSize( dstPaletteSize, dstPalRasterDepth );

        // Check whether we can directly aquire the palette.
        bool canPaletteDirectlyAcquire = ( srcRasterFormat == dstRasterFormat && srcColorOrder == dstColorOrder );

        if ( canPaletteDirectlyAcquire )
        {
            // Just write it.
            uint32 actualPaletteDataSize = getPaletteDataSize( srcPaletteSize, dstPalRasterDepth );

            writePartialStreamSafe( inputStream, srcPaletteData, actualPaletteDataSize, dstPaletteDataSize );
        }
        else
        {
            // We have to convert before writing.
            void *dstPaletteData = engineInterface->PixelAllocate( dstPaletteDataSize );

            if ( dstPaletteData == NULL )
            {
                throw RwException( "failed to allocate destination palette data for DDS color format transformation" );
            }

            try
            {
                ConvertPaletteData(
                    srcPaletteData, dstPaletteData,
                    srcPaletteSize, dstPaletteSize,
                    srcRasterFormat, srcColorOrder, srcPalRasterDepth,
                    dstRasterFormat, dstColorOrder, dstPalRasterDepth
                );

                // Now write it.
                inputStream->write( dstPaletteData, dstPaletteDataSize );
            }
            catch( ... )
            {
                engineInterface->PixelFree( dstPaletteData );

                throw;
            }

            // Release the memory.
            engineInterface->PixelFree( dstPaletteData );
        }
    }

    // Now write the mipmap layers.
    for ( size_t n = 0; n < mipmapCount; n++ )
    {
        const NativeTextureD3D9::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

        // Calculate the surface dimensions.
        uint32 surfWidth = mipLayer.width;
        uint32 surfHeight = mipLayer.height;

        uint32 dstSurfWidth, dstSurfHeight;

        bool gotDstDimms = false;

        if ( !isRawRasterEncoding )
        {
            gotDstDimms = dds_header::calculateSurfaceDimensions(
                dstFormat,
                mipLayer.layerWidth, mipLayer.layerHeight,
                dstSurfWidth, dstSurfHeight
            );
        }

        if ( !gotDstDimms )
        {
            dstSurfWidth = mipLayer.layerWidth;
            dstSurfHeight = mipLayer.layerHeight;
        }

        // Decide how to directly write this mipmap.
        // We can directly write it if the row size is the same and it is directly mapped.
        bool canWriteDirectly = true;

        if ( !hasDirectMapping )
        {
            canWriteDirectly = false;
        }

        if ( canWriteDirectly )
        {
            if ( !isRawRasterEncoding )
            {
                // If we have a different output format, we cannot write directly.
                if ( dstFormat != d3dFormat )
                {
                    canWriteDirectly = false;
                }
            }
            else
            {
                // TODO?
            }
        }

        uint32 srcRowSize;
        uint32 dstRowSize;

        if ( outputAsPitchOrLinearSize == true )
        {
            srcRowSize = getRasterDataRowSize( dstSurfWidth, srcDepth, srcRowAlignment );
        }

        if ( canWriteDirectly )
        {
            // If the texture is re-alignable, we need to check the row sizes.
            if ( outputAsPitchOrLinearSize == true )
            {
                if ( n == 0 )
                {
                    dstRowSize = baseDstPitch;
                }
                else
                {
                    dstRowSize = getRasterDataRowSize( surfWidth, bitDepth, 1 );
                }

                if ( dstRowSize != srcRowSize )
                {
                    canWriteDirectly = false;
                }
            }
        }

        // Write the mipmap layer.
        void *srcTexels = mipLayer.texels;
        uint32 srcDataSize = mipLayer.dataSize;

        if ( canWriteDirectly )
        {
            inputStream->write( srcTexels, srcDataSize );
        }
        else
        {
            // We can only do that if we can write row-by-row.
            if ( outputAsPitchOrLinearSize == false )
            {
                throw RwException( "cannot transform mipmap data that does not support pitch in DDS serialization routine of Direct3D 9 native texture" );
            }

            // If we do not have a raster format link, then we have to temporarily convert the anonymous raster data.
            void *tmpTexels = srcTexels;
            uint32 tmpRowSize = srcRowSize;

            if ( !hasDirectMapping )
            {
                if ( !d3dRasterFormatLink )
                {
                    assert( usedFormatHandler != NULL );

                    tmpRowSize = getD3DRasterDataRowSize( surfWidth, dstDepth );

                    uint32 tmpDataSize = getRasterDataSizeByRowSize( tmpRowSize, surfHeight );

                    tmpTexels = engineInterface->PixelAllocate( tmpDataSize );

                    if ( !tmpTexels )
                    {
                        throw RwException( "failed to allocate texel transformation buffer" );
                    }

                    try
                    {
                        // Do the conversion.
                        usedFormatHandler->ConvertToRW( srcTexels, surfWidth, surfHeight, tmpRowSize, srcDataSize, tmpTexels );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( tmpTexels );

                        throw;
                    }
                }
            }

            try
            {
                // We need to write row-by-row.
                void *rowbuf = engineInterface->PixelAllocate( baseDstPitch );

                if ( rowbuf == NULL )
                {
                    throw RwException( "could not allocate mipmap transformation row buffer" );
                }

                try
                {
                    // Transform stuff.
                    for ( uint32 row = 0; row < dstSurfHeight; row++ )
                    {
                        const void *srcRow = NULL;

                        if ( row < surfHeight )
                        {
                            srcRow = getConstTexelDataRow( tmpTexels, tmpRowSize, row );
                        }

                        if ( srcRow )
                        {
                            if ( hasDirectMapping )
                            {
                                // We just have to change alignment.
                                memcpy( rowbuf, srcRow, std::min( baseDstPitch, tmpRowSize ) );
                            }
                            else
                            {
                                // Transform stuff.
                                moveTexels(
                                    tmpTexels, rowbuf,
                                    0, row,
                                    0, 0,
                                    surfWidth, 1,
                                    surfWidth, surfHeight,
                                    mipRasterFormat, mipDepth, getD3DTextureDataRowAlignment(), mipColorOrder, srcPaletteType, srcPaletteSize,
                                    dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteSize
                                );
                            }

                            // Write the row.
                            inputStream->write( rowbuf, baseDstPitch );
                        }
                    }
                }
                catch( ... )
                {
                    engineInterface->PixelFree( rowbuf );

                    throw;
                }

                engineInterface->PixelFree( rowbuf );
            }
            catch( ... )
            {
                if ( tmpTexels != srcTexels )
                {
                    engineInterface->PixelFree( tmpTexels );
                }

                throw;
            }

            if ( tmpTexels != srcTexels )
            {
                engineInterface->PixelFree( tmpTexels );
            }
        }
    }

    // Done!
}

void d3d9NativeTextureTypeProvider::DeserializeNativeImage( Interface *engineInterface, Stream *outputStream, void *objMem ) const
{
    endian::little_endian <uint32> magic;
    {
        size_t magicReadCount = outputStream->read( &magic, sizeof( magic ) );

        if ( magicReadCount != sizeof( magic ) || magic != ' SDD' )
        {
            // Here, instead of in the checking algorithm, we throw descriptive exceptions for malformations.
            throw RwException( "invalid magic number in DDS file" );
        }
    }

    // Read its header.
    dds_header header;
    {
        size_t headerReadCount = outputStream->read( &header, sizeof( header ) );

        if ( headerReadCount != sizeof( header ) )
        {
            throw RwException( "failed to read DDS header" );
        }
    }

    // Verify some important things.
    if ( header.dwSize != sizeof( header ) )
    {
        throw RwException( "invalid DDS header struct size" );
    }

    if ( header.ddspf.dwSize != sizeof( header.ddspf ) )
    {
        throw RwException( "invalid DDS header pixel format struct size" );
    }

    // Parse the main header flags.
    uint32 ddsFlags = header.dwFlags;

    if ( ( ddsFlags & DDSD_DEPTH ) != 0 )
    {
        throw RwException( "3 dimensional DDS textures not supported" );
    }

    if ( ( ddsFlags & DDSD_CAPS ) == 0 )
    {
        engineInterface->PushWarning( "DDS file is missing DDSD_CAPS" );
    }

    if ( ( ddsFlags & DDSD_HEIGHT ) == 0 )
    {
        engineInterface->PushWarning( "DDS file is missing DDSD_HEIGHT" );
    }

    if ( ( ddsFlags & DDSD_WIDTH ) == 0 )
    {
        engineInterface->PushWarning( "DDS file is missing DDSD_WIDTH" );
    }

    if ( ( ddsFlags & DDSD_PIXELFORMAT ) == 0 )
    {
        engineInterface->PushWarning( "DDS file is missing DDSD_PIXELFORMAT" );
    }

    bool hasMipmapCount = ( ddsFlags & DDSD_MIPMAPCOUNT ) != 0;

    // A DDS file is supposed to be directly acquirable into a Direct3D 9 native texture.
    // The ones that we cannot aquire we consider malformed.

    uint32 ddsPixelFlags = header.ddspf.dwFlags;

    bool hasRGB = ( ddsPixelFlags & DDPF_RGB ) != 0;
    bool hasYUV = ( ddsPixelFlags & DDPF_YUV ) != 0;
    bool hasLum = ( ddsPixelFlags & DDPF_LUMINANCE ) != 0;
    bool hasAlphaOnly = ( ddsPixelFlags & DDPF_ALPHA ) != 0;
    bool hasAlpha = ( ddsPixelFlags & DDPF_ALPHAPIXELS ) != 0;
    bool hasPal4 = ( ddsPixelFlags & DDPF_PALETTEINDEXED4 ) != 0;
    bool hasPal8 = ( ddsPixelFlags & DDPF_PALETTEINDEXED8 ) != 0;

    // To acquire it, we must map it to a valid thing for us.
    // This is if it either has a d3dFormat or a palette format.
    D3DFORMAT d3dFormat;
    bool hasValidFormat = false;

    ePaletteType paletteType = PALETTE_NONE;
    bool hasPaletteFormat = false;

    uint32 bitDepth;
    bool hasBitDepth;

    if ( ddsPixelFlags & DDPF_FOURCC )
    {
        d3dFormat = (D3DFORMAT)(uint32)header.ddspf.dwFourCC;

        // We need to determine the bit depth.
        hasBitDepth = dds_header::getBitDepth( d3dFormat, bitDepth );

        // We can acquire any anonymous format, so thats fine.
        hasValidFormat = true;
    }
    else
    {
        // Check for format conflicts.
        {
            if ( hasAlphaOnly && hasRGB )
            {
                hasAlphaOnly = false;

                engineInterface->PushWarning( "DDS file has ambiguous format type (ALPHA or *RGB)" );
            }

            if ( hasRGB && hasYUV )
            {
                // We upgrade here.
                hasRGB = false;

                engineInterface->PushWarning( "DDS file has ambiguous format type (RGB or *YUV)" );
            }

            if ( hasYUV && hasLum )
            {
                // We upgrade here.
                hasLum = false;

                engineInterface->PushWarning( "DDS file has ambiguous format type (*YUV or LUM)" );
            }

            // Must not ignore palette.
            if ( hasPal4 && hasPal8 )
            {
                throw RwException( "ambiguous palette format in DDS file" );
            }

            if ( hasPal4 && hasYUV )
            {
                // Kinda have to downgrade.
                hasYUV = false;

                engineInterface->PushWarning( "DDS file has ambiguous format type (YUV or *PAL4)" );
            }

            if ( hasPal8 && hasYUV )
            {
                // Kinda have to downgrade.
                hasYUV = false;

                engineInterface->PushWarning( "DDS file has ambiguous format type (YUV or *PAL8)" );
            }
        }

        // Calculation of that field is required.
        // For that we look at the bitfields.
        if ( hasRGB )
        {
            size_t rgba_count = ELMCNT( rgba_masks );

            uint32 redMask = header.ddspf.dwRBitMask;
            uint32 greenMask = header.ddspf.dwGBitMask;
            uint32 blueMask = header.ddspf.dwBBitMask;
            uint32 alphaMask = ( hasAlpha ? header.ddspf.dwABitMask : 0 );

            bitDepth = header.ddspf.dwRGBBitCount;

            hasBitDepth = true;

            for ( size_t n = 0; n < rgba_count; n++ )
            {
                const rgba_mask_info& curInfo = rgba_masks[ n ];

                if ( curInfo.redMask == redMask &&
                     curInfo.greenMask == greenMask &&
                     curInfo.blueMask == blueMask &&
                     curInfo.alphaMask == alphaMask &&
                     curInfo.depth == bitDepth )
                {
                    // We found something.
                    d3dFormat = curInfo.format;

                    hasValidFormat = true;
                    break;
                }
            }

            if ( !hasValidFormat )
            {
                throw RwException( "failed to map RGB DDS file to Direct3D 9 native raster" );
            }
        }
        else if ( hasYUV )
        {
            size_t yuv_count = ELMCNT( yuv_masks );

            uint32 yMask = header.ddspf.dwRBitMask;
            uint32 uMask = header.ddspf.dwGBitMask;
            uint32 vMask = header.ddspf.dwBBitMask;
            uint32 aMask = header.ddspf.dwABitMask;

            bitDepth = header.ddspf.dwRGBBitCount;

            hasBitDepth = true;

            if ( hasAlpha && aMask != 0x00000000 )
            {
                throw RwException( "DDS YUV texture with alpha not supported" );
            }
            
            for ( size_t n = 0; n < yuv_count; n++ )
            {
                const yuv_mask_info& curInfo = yuv_masks[ n ];

                if ( curInfo.yMask == yMask &&
                     curInfo.uMask == uMask &&
                     curInfo.vMask == vMask &&
                     curInfo.depth == bitDepth )
                {
                    d3dFormat = curInfo.format;

                    hasValidFormat = true;
                    break;
                }
            }

            if ( !hasValidFormat )
            {
                throw RwException( "failed to map YUV DDS file to Direct3D 9 native texture" );
            }
        }
        else if ( hasLum )
        {
            size_t lum_count = ELMCNT( lum_masks );

            uint32 lumMask = header.ddspf.dwRBitMask;
            uint32 alphaMask = ( hasAlpha ? header.ddspf.dwABitMask : 0 );

            bitDepth = header.ddspf.dwRGBBitCount;

            hasBitDepth = true;

            for ( size_t n = 0; n < lum_count; n++ )
            {
                const lum_mask_info& curInfo = lum_masks[ n ];

                if ( curInfo.lumMask == lumMask &&
                     curInfo.alphaMask == alphaMask &&
                     curInfo.depth == bitDepth )
                {
                    d3dFormat = curInfo.format;

                    hasValidFormat = true;
                    break;
                }
            }

            if ( !hasValidFormat )
            {
                throw RwException( "failed to map LUM DDS file to Direct3D 9 native texture" );
            }
        }
        else if ( hasAlphaOnly )
        {
            size_t alpha_count = ELMCNT( alpha_masks );

            uint32 alphaMask = header.ddspf.dwABitMask;

            bitDepth = header.ddspf.dwRGBBitCount;

            hasBitDepth = true;

            for ( size_t n = 0; n < alpha_count; n++ )
            {
                const alpha_mask_info& curInfo = alpha_masks[ n ];

                if ( curInfo.alphaMask == alphaMask &&
                     curInfo.depth == bitDepth )
                {
                    d3dFormat = curInfo.format;

                    hasValidFormat = true;
                    break;
                }
            }

            if ( !hasValidFormat )
            {
                throw RwException( "failed to map alpha DDS file to Direct3D 9 native texture" );
            }
        }
        else if ( hasPal4 || hasPal8 )
        {
            bitDepth = header.ddspf.dwRGBBitCount;

            hasBitDepth = true;

            // Determine the actual palette type.
            if ( hasPal4 )
            {
                paletteType = PALETTE_4BIT;
            }
            else if ( hasPal8 )
            {
                paletteType = PALETTE_8BIT;
            }
            else
            {
                assert( 0 );
            }

            hasPaletteFormat = true;

            // Check whether the bit depth is valid.
            bool validDepth = false;

            if ( paletteType == PALETTE_4BIT )
            {
                validDepth = ( bitDepth == 4 || bitDepth == 8 );
            }
            else if ( paletteType == PALETTE_8BIT )
            {
                validDepth = ( bitDepth == 8 );
            }

            if ( !validDepth )
            {
                throw RwException( "invalid palette bit depth in DDS file deserialization" );
            }

            // We have no D3DFORMAT.
            hasValidFormat = false;
        }
        else
        {
            throw RwException( "unknown DDS image pixel format type" );
        }
    }

    if ( !hasValidFormat && !hasPaletteFormat )
    {
        throw RwException( "could not map DDS file to a valid D3D9 raster representation" );
    }

    bool hasValidMapping = false;

    // Check whether we are DXT compressed.
    uint32 dxtType = 0;

    if ( hasValidFormat )
    {
        dxtType = getCompressionFromD3DFormat( d3dFormat );
    }

    if ( dxtType != 0 )
    {
        hasValidMapping = true;
    }

    // Determine whether this is a direct format link.
    bool d3dRasterFormatLink = false;
    bool originalRWCompat = false;

    d3dpublic::nativeTextureFormatHandler *usedFormatHandler = NULL;

    // Determine the mapping to original RW types.
    eRasterFormat rasterFormat = RASTER_DEFAULT;
    eColorOrdering colorOrder = COLOR_BGRA;

    if ( hasValidFormat )
    {
        bool isVirtualMapping = false;

        bool hasRepresentingFormat = getRasterFormatFromD3DFormat(
            d3dFormat, hasAlpha || hasAlphaOnly,
            rasterFormat, colorOrder, isVirtualMapping
        );

        if ( hasRepresentingFormat )
        {
            if ( isVirtualMapping == false )
            {
                d3dRasterFormatLink = true;

                // Could be compatible with RW. Let's check.
                originalRWCompat =
                    isRasterFormatOriginalRWCompatible(
                        rasterFormat, colorOrder, bitDepth,
                        PALETTE_NONE    // This configuration of DDS file has no palette.
                    );
            }

            hasValidMapping = true;
        }
    
        if ( !hasValidMapping )
        {
            // We could still have an extension that takes care of us.
            usedFormatHandler = this->GetFormatHandler( d3dFormat );

            hasValidMapping = true;
        }
    }
    else
    {
        // Palette are always encoded the same.
        d3dRasterFormatLink = true;
        originalRWCompat = true;    // always compatible with original RW.
   
        rasterFormat = RASTER_8888;
        colorOrder = COLOR_RGBA;

        hasValidMapping = true;

        d3dFormat = D3DFMT_P8;
    }

    // If we do not directly map to a D3DFORMAT, then the DDS travels in a special non-native format.
    // In that case we have to make sure we convert the data to a proper format.
    bool canRawDirectlyAcquire = false;

    uint32 dstDepth;    // must set this field.

    eRasterFormat dstRasterFormat = rasterFormat;
    eColorOrdering dstColorOrder = colorOrder;
    ePaletteType dstPaletteType = paletteType;

    if ( hasValidFormat )
    {
        if ( hasBitDepth )
        {
            dstDepth = bitDepth;
        }

        // There are cases when DDS files travel in unoptimized pixel data. The runtime can
        // automatically optimize the pixel data for hardware if you pass true to
        // SetCompatTransformNativeImaging. Doing so tells the runtime that you prefer compatibility
        // over staying true to the formats.
        bool hasRedirect = false;

        if ( d3dRasterFormatLink )  // RWCOMPRESS_NONE
        {
            assert( hasBitDepth == true );

            if ( engineInterface->GetCompatTransformNativeImaging() )
            {
                uint32 recDepth;
                eColorOrdering recColorOrder;
    
                bool hasRecDepth, hasRecColorOrder;

                this->GetRecommendedRasterFormat( dstRasterFormat, dstPaletteType, recDepth, hasRecDepth, recColorOrder, hasRecColorOrder );

                if ( hasRecDepth && recDepth != dstDepth )
                {
                    dstDepth = recDepth;

                    hasRedirect = true;
                }

                if ( hasRecColorOrder && recColorOrder != dstColorOrder )
                {
                    dstColorOrder = recColorOrder;

                    hasRedirect = true;
                }
            }
        }

        if ( !hasRedirect )
        {
            // Since we have not redefined the target, the D3DFORMAT from the DDS applies.
            // This means that DDS writing is very optimized.
            canRawDirectlyAcquire = true;
        }
    }
    else if ( hasPaletteFormat )
    {
        assert( hasBitDepth == true );

        // Ask the runtime about a proper format to encode to.
        dstDepth = bitDepth;

        convertCompatibleRasterFormat( dstRasterFormat, dstColorOrder, dstDepth, dstPaletteType, d3dFormat );

        // Check for direct acquisition.
        canRawDirectlyAcquire =
            doesRasterFormatNeedConversion(
                rasterFormat, bitDepth, colorOrder, paletteType,
                dstRasterFormat, dstDepth, dstColorOrder, dstPaletteType
            ) == false;
    }
    else
    {
        assert( 0 );
    }

    // If we are palette encoded, we want to read the palette and store it.
    // TODO: since this palette reading follows a common pattern, maybe create a helper for reading palette data!
    void *paletteData = NULL;
    uint32 paletteSize = 0;

    if ( paletteType != PALETTE_NONE )
    {
        assert( dstPaletteType != PALETTE_NONE );

        paletteSize = getPaletteItemCount( paletteType );

        uint32 srcPalRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

        uint32 srcPaletteDataSize = getPaletteDataSize( paletteSize, srcPalRasterDepth );

        checkAhead( outputStream, srcPaletteDataSize );

        // We should a buffer that can hold the destination texels.
        // But we will also need a buffe rthat can hold the texels of transformation from the file, which is the buffer
        // we start with.
        void *srcPaletteData = engineInterface->PixelAllocate( srcPaletteDataSize );

        if ( srcPaletteData == NULL )
        {
            throw RwException( "failed to allocate palette color buffer in DDS palettized deserialization" );
        }

        try
        {
            // Read the palette.
            size_t readPalSize = outputStream->read( srcPaletteData, srcPaletteDataSize );

            if ( srcPaletteDataSize != readPalSize )
            {
                throw RwException( "DDS file has corrupted palette data (failed to read completely)" );
            }

            // Now, we might need to transform the texels that we read, or not.
            uint32 dstPalRasterDepth = Bitmap::getRasterFormatDepth( dstRasterFormat );

            bool doesPaletteNeedTransform =
                doesRasterFormatNeedConversion(
                    rasterFormat, srcPalRasterDepth, colorOrder, PALETTE_NONE,
                    dstRasterFormat, dstPalRasterDepth, dstColorOrder, PALETTE_NONE
                );

            if ( doesPaletteNeedTransform == false )
            {
                // We can simply give our buffer.
                paletteData = srcPaletteData;
            }
            else
            {
                // We have to transform things, maybe even into another buffer.
                if ( srcPalRasterDepth != dstPalRasterDepth )
                {
                    uint32 dstPalDataSize = getPaletteDataSize( paletteSize, dstPalRasterDepth );

                    paletteData = engineInterface->PixelAllocate( dstPalDataSize );

                    if ( !paletteData )
                    {
                        throw RwException( "failed to allocate final palette buffer for DDS palette color conversion" );
                    }
                }
                else
                {
                    // We can just transform into the same buffer.
                    paletteData = srcPaletteData;
                }

                try
                {
                    ConvertPaletteData(
                        srcPaletteData, paletteData,
                        paletteSize, paletteSize,
                        rasterFormat, colorOrder, srcPalRasterDepth,
                        dstRasterFormat, dstColorOrder, dstPalRasterDepth
                    );
                }
                catch( ...  )
                {
                    if ( paletteData != srcPaletteData )
                    {
                        engineInterface->PixelFree( paletteData );
                    }

                    throw;
                }
            }
        }
        catch( ... )
        {
            engineInterface->PixelFree( srcPaletteData );

            throw;
        }

        // Release not required buffers.
        if ( srcPaletteData != paletteData )
        {
            engineInterface->PixelFree( srcPaletteData );
        }
    }

    try
    {
        // Begin writing to the texture.
        NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

        uint32 maybeMipmapCount = ( hasMipmapCount ? header.dwMipmapCount : 1 );

        // Verify some capabilities.
        {
            bool isComplex = ( header.dwCaps & DDSCAPS_COMPLEX ) != 0;

            if ( !isComplex && maybeMipmapCount > 1 )
            {
                engineInterface->PushWarning( "DDS file does have mipmaps but is not marked complex" );
            }

            if ( ( header.dwCaps & DDSCAPS_TEXTURE ) == 0 )
            {
                engineInterface->PushWarning( "DDS file is missing DDSCAPS_TEXTURE" );
            }

            // We ignore any cubemap settings.
        }

        // Get the pitch or linear size.
        // Either way, we should be able to understand this.
        uint32 mainSurfacePitchOrLinearSize = header.dwPitchOrLinearSize;

        bool isLinearSize = ( ( header.dwFlags & DDSD_LINEARSIZE ) != 0 );
        bool isPitch ( ( header.dwFlags & DDSD_PITCH ) != 0 );

        if ( isLinearSize && isPitch )
        {
            engineInterface->PushWarning( "ambiguous DDS file property; both pitch and linear size property set (using pitch)" );

            isPitch = false;
        }

        // We now want to read all mipmaps and store them!
        mipGenLevelGenerator mipGen( header.dwWidth, header.dwHeight );

        if ( !mipGen.isValidLevel() )
        {
            throw RwException( "invalid DDS file dimensions" );
        }

        uint32 mipmapCount = 0;

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

            // Read the mipmap data.
            uint32 layerWidth = mipGen.getLevelWidth();
            uint32 layerHeight = mipGen.getLevelHeight();

            // Calculate the surface dimensions.
            uint32 surfWidth, surfHeight;

            calculateSurfaceDimensions( layerWidth, layerHeight, hasValidFormat, d3dFormat, surfWidth, surfHeight );
        
            // Get the source pitch, if available.
            uint32 dstPitch;
            bool hasDstPitch = false;

            if ( hasBitDepth )
            {
                dstPitch = getD3DRasterDataRowSize( surfWidth, dstDepth );

                hasDstPitch = true;
            }

            // Verify that we have this mipmap data.
            uint32 mipLevelDataSize;

            bool hasDataSize = false;

            uint32 srcPitch;
            bool hasSrcPitch = false;

            if ( n == 0 )
            {
                if ( isLinearSize )
                {
                    mipLevelDataSize = mainSurfacePitchOrLinearSize;

                    hasDataSize = true;
                }
                else if ( isPitch )
                {
                    mipLevelDataSize = getRasterDataSizeByRowSize( mainSurfacePitchOrLinearSize, surfHeight );

                    hasDataSize = true;

                    // If we have a pitch, we can check whether we are properly aligned here.
                    srcPitch = mainSurfacePitchOrLinearSize;

                    hasSrcPitch = true;
                }
            }
            
            if ( !hasDataSize )
            {
                // For this we need to have valid bit depth.
                if ( hasBitDepth )
                {
                    uint32 mipStride = getRasterDataRowSize( surfWidth, bitDepth, 1 );  // byte alignment.

                    mipLevelDataSize = getRasterDataSizeByRowSize( mipStride, surfHeight );

                    hasDataSize = true;

                    srcPitch = mipStride;

                    hasSrcPitch = true;
                }
            }

            if ( !hasDataSize )
            {
                throw RwException( "failed to determine DDS mipmap data size" );
            }

            // Check whether we can directly acquire.
            bool canDirectlyAcquireByDimensions = false;

            if ( hasSrcPitch && hasDstPitch )
            {
                canDirectlyAcquireByDimensions = ( srcPitch == dstPitch );
            }
            else
            {
                // TODO: decide whether we want to trust this data, because we do not know whether
                // it is properly aligned.
                canDirectlyAcquireByDimensions = true;
            }

            // If we can directly acquire, then, if the format is important, we must make sure it is the same as the destination.
            bool isFormatSame = false;

            if ( canDirectlyAcquireByDimensions )
            {
                if ( hasValidFormat )
                {
                    isFormatSame = true;
                }
                else
                {
                    isFormatSame = canRawDirectlyAcquire;
                }
            }

            // Data inside of the DDS file can be aligned in whatever way.
            // If the rows are already DWORD aligned, we can directly read this mipmap data.
            // Otherwise we have to read row-by-row.
            checkAhead( outputStream, mipLevelDataSize );

            uint32 dstTexelsDataSize = 0;

            if ( canDirectlyAcquireByDimensions )
            {
                dstTexelsDataSize = mipLevelDataSize;
            }
            else
            {
                if ( hasDstPitch )
                {
                    dstTexelsDataSize = getRasterDataSizeByRowSize( dstPitch, surfHeight );   
                }
                else
                {
                    throw RwException( "failed to determine Direct3D 9 native texture mipmap data size in DDS deserialization" );
                }
            }

            // Allocate the pure destination texel buffer.
            // This does not have to be the target of the read operations, but will be the target of the destination transformation.
            void *texels = engineInterface->PixelAllocate( dstTexelsDataSize );

            if ( !texels )
            {
                throw RwException( "failed to allocate Direct3D 9 native texture texel memory in DDS deserialization" );
            }

            try
            {
                if ( isFormatSame )
                {
                    // Just read the data.
                    size_t texelReadCount = outputStream->read( texels, mipLevelDataSize );

                    if ( texelReadCount != mipLevelDataSize )
                    {
                        throw RwException( "failed to read DDS mipmap texture memory" );
                    }
                }
                else
                {
                    // WARNING: make sure that we only realign actually re-alignable formats!
                    // compressed formats for instance are not re-alignable!
                    // Our current assumption is that we will never trigger this code path
                    // for compressed formats.

                    assert( hasSrcPitch == true && hasDstPitch == true );

                    // Check whether we need a new destination buffer for transformation.
                    void *dstTransformBuffer = texels;

                    bool requiresTransformation = false;

                    if ( hasValidFormat )
                    {
                        // We have to check whether we actually can directly take the DDS texels.
                        // This could not happen if the DDS texels travel in an unoptimized format.
                        if ( canRawDirectlyAcquire == false )
                        {
                            requiresTransformation = true;
                        }
                    }
                    else if ( hasPaletteFormat )
                    {
                        // The only factor of palette samples is the depth.
                        if ( bitDepth != dstDepth )
                        {
                            requiresTransformation = true;
                        }
                    }

                    // So if we figured that we need transformations, do it.
                    if ( requiresTransformation )
                    {
                        // We MUST know all about this format here.
                        assert( d3dRasterFormatLink == true );

                        // If the transformation buffer is not the destination, then it is a single row buffer
                        // that is redirected to the destination buffer.
                        uint32 transBufferSize = srcPitch;

                        dstTransformBuffer = engineInterface->PixelAllocate( transBufferSize );

                        if ( !dstTransformBuffer )
                        {
                            throw RwException( "failed to allocate destination transformation buffer for DDS row parsing" );
                        }
                    }

                    int64 mipmapDataOffset = outputStream->tell();

                    try
                    {
                        // Read the data row by row.
                        for ( uint32 row = 0; row < surfHeight; row++ )
                        {
                            // Seek to the required row.
                            if ( row != 0 )
                            {
                                outputStream->seek( mipmapDataOffset + srcPitch * row, RWSEEK_BEG );
                            }

                            void *dstRowData = getTexelDataRow( texels, dstPitch, row );

                            if ( texels == dstTransformBuffer )
                            {
                                // Read stuff.
                                // We want to simply read anyway, since destination transformation is guarranteed without redirection.
                                outputStream->read( dstRowData, dstPitch );
                            }
                            else
                            {
                                // Read, transform and write.
                                outputStream->read( dstTransformBuffer, srcPitch );

                                // Transform now.
                                assert( d3dRasterFormatLink == true ); // the contract of having source and destination format bridge

                                moveTexels(
                                    dstTransformBuffer, dstRowData,
                                    0, 0,
                                    0, 0,
                                    layerWidth, 1,
                                    surfWidth, surfHeight,
                                    rasterFormat, bitDepth, 1, colorOrder, paletteType, paletteSize,
                                    dstRasterFormat, dstDepth, getD3DTextureDataRowAlignment(), dstColorOrder, dstPaletteType, paletteSize
                                );

                                // We have written in the same step.
                            }
                        }
                    }
                    catch( ... )
                    {
                        if ( texels != dstTransformBuffer )
                        {
                            engineInterface->PixelFree( dstTransformBuffer );
                        }

                        throw;
                    }

                    if ( texels != dstTransformBuffer )
                    {
                        engineInterface->PixelFree( dstTransformBuffer );
                    }

                    // Seek to the real stream end.
                    outputStream->seek( mipmapDataOffset + srcPitch * surfHeight, RWSEEK_BEG );
                }
            }
            catch( ... )
            {
                // Release temporary texel buffer.
                engineInterface->PixelFree( texels );

                throw;
            }

            // Store this mipmap layer.
            NativeTextureD3D9::mipmapLayer mipLevel;
            mipLevel.layerWidth = layerWidth;
            mipLevel.layerHeight = layerHeight;
            mipLevel.width = surfWidth;
            mipLevel.height = surfHeight;
            mipLevel.texels = texels;
            mipLevel.dataSize = dstTexelsDataSize;

            // Add this layer.
            nativeTex->mipmaps.push_back( mipLevel );

            // Increase the amount of actual mipmaps.
            mipmapCount++;
        }

        if ( mipmapCount == 0 )
        {
            throw RwException( "empty DDS file (no mipmaps)" );
        }

        // We have to determine whether this native texture actually has transparent data.
        bool shouldHaveAlpha = ( hasAlpha || hasAlphaOnly );

        if ( dxtType != 0 || d3dRasterFormatLink || usedFormatHandler != NULL )
        {
            bool hasAlphaByTexels = false;

            if ( dxtType != 0 )
            {
                // We traverse DXT mipmap layers and check them.
                for ( uint32 n = 0; n < mipmapCount; n++ )
                {
                    const NativeTextureD3D9::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

                    bool hasLayerAlpha = dxtMipmapCalculateHasAlpha(
                        mipLayer.width, mipLayer.height, mipLayer.layerWidth, mipLayer.layerHeight, mipLayer.texels,
                        dxtType
                    );

                    if ( hasLayerAlpha )
                    {
                        hasAlphaByTexels = true;
                        break;
                    }
                }
            }
            else
            {
                assert( hasBitDepth == true );

                // Determine the raster format we should use for the mipmap alpha check.
                eRasterFormat mipRasterFormat;
                uint32 mipDepth;
                eColorOrdering mipColorOrder;

                bool directCheck = false;
        
                if ( d3dRasterFormatLink )
                {
                    mipRasterFormat = dstRasterFormat;
                    mipDepth = dstDepth;
                    mipColorOrder = dstColorOrder;

                    directCheck = true;
                }
                else if ( usedFormatHandler != NULL )
                {
                    usedFormatHandler->GetTextureRWFormat( mipRasterFormat, mipDepth, mipColorOrder );

                    directCheck = false;
                }

                // Check by raster format whether alpha data is even possible.
                if ( canRasterFormatHaveAlpha( mipRasterFormat ) )
                {
                    for ( uint32 n = 0; n < mipmapCount; n++ )
                    {
                        const NativeTextureD3D9::mipmapLayer& mipLayer = nativeTex->mipmaps[ n ];

                        uint32 mipWidth = mipLayer.width;
                        uint32 mipHeight = mipLayer.height;

                        uint32 srcDataSize = mipLayer.dataSize;
                        uint32 dstDataSize = srcDataSize;

                        void *srcTexels = mipLayer.texels;
                        void *dstTexels = srcTexels;

                        if ( !directCheck )
                        {
                            uint32 dstStride = getD3DRasterDataRowSize( mipWidth, mipDepth ); 

                            dstDataSize = getRasterDataSizeByRowSize( dstStride, mipHeight );

                            dstTexels = engineInterface->PixelAllocate( dstDataSize );

                            if ( dstTexels == NULL )
                            {
                                throw RwException( "failed to determine native texture alpha due to memory allocation failure" );
                            }

                            try
                            {
                                usedFormatHandler->ConvertToRW( srcTexels, mipWidth, mipHeight, dstStride, srcDataSize, dstTexels );
                            }
                            catch( ... )
                            {
                                engineInterface->PixelFree( dstTexels );

                                throw;
                            }
                        }

                        bool hasMipmapAlpha = false;

                        try
                        {
                            hasMipmapAlpha =
                                rawMipmapCalculateHasAlpha(
                                    mipWidth, mipHeight, dstTexels, dstDataSize,
                                    mipRasterFormat, mipDepth, getD3DTextureDataRowAlignment(), mipColorOrder, dstPaletteType, paletteData, paletteSize
                                );
                        }
                        catch( ... )
                        {
                            if ( srcTexels != dstTexels )
                            {
                                engineInterface->PixelFree( dstTexels );
                            }

                            throw;
                        }

                        // Free texels if we allocated any.
                        if ( srcTexels != dstTexels )
                        {
                            engineInterface->PixelFree( dstTexels );
                        }

                        if ( hasMipmapAlpha )
                        {
                            // If anything has alpha, we can break.
                            hasAlphaByTexels = true;
                            break;
                        }
                    }
                }
            }

            // Give data back to the runtime.
            shouldHaveAlpha = hasAlphaByTexels;
        }

        // Store properties about this texture.
        nativeTex->rasterFormat = dstRasterFormat;
        nativeTex->depth = ( hasBitDepth ? dstDepth : 0 );
        nativeTex->colorOrdering = dstColorOrder;
        nativeTex->paletteType = dstPaletteType;
        nativeTex->palette = paletteData;
        nativeTex->paletteSize = paletteSize;

        nativeTex->d3dFormat = d3dFormat;

        nativeTex->d3dRasterFormatLink = d3dRasterFormatLink;
        nativeTex->anonymousFormatLink = usedFormatHandler;

        nativeTex->isOriginalRWCompatible = originalRWCompat;

        nativeTex->hasAlpha = shouldHaveAlpha;  // is not always correct, but most of the time.
        nativeTex->isCubeTexture = false;
        nativeTex->autoMipmaps = false;
        nativeTex->dxtCompression = dxtType;
        nativeTex->rasterType = 4;
    }
    catch( ... )
    {
        if ( paletteData )
        {
            engineInterface->PixelFree( paletteData );

            paletteData = NULL;
        }

        throw;
    }

    // Success!
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9