#include "StdInc.h"

#include "txdread.d3d9.hxx"

namespace rw
{

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
};

bool d3d9NativeTextureTypeProvider::IsNativeImageFormat( Interface *engineInterface, Stream *inputStream ) const
{
    // We check kinda thoroughly.
    endian::little_endian <uint32> magic;

    uint32 magicReadCount = inputStream->read( &magic, sizeof( magic ) );

    if ( magicReadCount != sizeof( magic ) )
    {
        return false;
    }

    if ( magic != 'DDS ' )
    {
        return false;
    }

    dds_header header;

    uint32 headerReadCount = inputStream->read( &header, sizeof( header ) );

    if ( headerReadCount != sizeof( header ) )
    {
        return false;
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

    // Simply verify that the entire texture data is present.
    // That should be enough complexity.

    return true;
}

void d3d9NativeTextureTypeProvider::SerializeNativeImage( Interface *engineInterface, Stream *inputStream, void *objMem ) const
{
    // Simply write out this native texture's content.
    NativeTextureD3D9 *nativeTex = (NativeTextureD3D9*)objMem;

    // First, we write the magic number.
    endian::little_endian <uint32> magic( 'DDS ' );

    inputStream->write( &magic, sizeof( magic ) );

    // Write header information.
    uint32 mipmapCount = 0;
}

void d3d9NativeTextureTypeProvider::DeserializeNativeImage( Interface *engineInterface, Stream *outputStream, void *objMem ) const
{
    // TODO.
}

};