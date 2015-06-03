#ifndef _D3D_GENERIC_MIPMAPS_
#define _D3D_GENERIC_MIPMAPS_

namespace rw
{

namespace genmip
{

struct mipmapLayer
{
    inline mipmapLayer( void )
    {
        this->width = 0;
        this->height = 0;
        this->dataSize = 0;

        this->layerWidth = 0;
        this->layerHeight = 0;

        this->texels = NULL;
    }

    // Data dimensions.
    uint32 width, height;
    uint32 dataSize;
    
    // Real dimensions of this layer.
    uint32 layerWidth, layerHeight;

    // Color data.
    void *texels;
};

template <typename containerType>
inline void copyMipmapLayers( Interface *engineInterface, const containerType& srcLayers, containerType& dstLayers )
{
    size_t numTexels = srcLayers.size();

    // Allocate the mipmap array at the new texture.
    dstLayers.resize( numTexels );

    for (uint32 i = 0; i < numTexels; i++)
    {
        const mipmapLayer& srcLayer = srcLayers[ i ];

        // Create a new mipmap layer.
        mipmapLayer newLayer;

        uint32 dataSize = srcLayer.dataSize;

        newLayer.width = srcLayer.width;
        newLayer.height = srcLayer.height;
        newLayer.dataSize = dataSize;

        newLayer.layerWidth = srcLayer.layerWidth;
        newLayer.layerHeight = srcLayer.layerHeight;

        // Copy over the texels.
        void *newtexels = engineInterface->PixelAllocate( dataSize );

        const void *srctexels = srcLayer.texels;

        memcpy(newtexels, srctexels, dataSize);

        newLayer.texels = newtexels;

        // Put the layer.
        dstLayers[ i ] = newLayer;
    }
}

template <typename containerType>
inline void deleteMipmapLayers( Interface *engineInterface, containerType& mipmaps )
{
    uint32 mipmapCount = mipmaps.size();

    for (uint32 i = 0; i < mipmapCount; i++)
    {
        mipmapLayer& layer = mipmaps[ i ];

        engineInterface->PixelFree( layer.texels );

	    layer.texels = NULL;
    }
}

};

};

#endif //_D3D_GENERIC_MIPMAPS_