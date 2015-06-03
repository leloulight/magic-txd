namespace rw
{

// Sample template for mipmap manager (in this case D3D).
#if 0
struct d3dMipmapManager
{
    NativeTextureD3D *nativeTex;

    inline d3dMipmapManager( NativeTextureD3D *nativeTex )
    {
        this->nativeTex = nativeTex;
    }

    inline void GetLayerDimensions(
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& layerWidth, uint32& layerHeight
    )
    {
        layerWidth = mipLayer.layerWidth;
        layerHeight = mipLayer.layerHeight;
    }

    inline void Deinternalize(
        Interface *engineInterface,
        const NativeTextureD3D::mipmapLayer& mipLayer,
        uint32& widthOut, uint32& heightOut, uint32& layerWidthOut, uint32& layerHeightOut,
        eRasterFormat& dstRasterFormat, eColorOrdering& dstColorOrder, uint32& dstDepth,
        ePaletteType& dstPaletteType, void*& dstPaletteData, uint32& dstPaletteSize,
        eCompressionType& dstCompressionType, bool& hasAlpha,
        void*& dstTexelsOut, uint32& dstDataSizeOut,
        bool& isNewlyAllocatedOut, bool& isPaletteNewlyAllocated
    )
    {

    }

    inline void Internalize(
        Interface *engineInterface,
        NativeTextureD3D::mipmapLayer& mipLayer,
        uint32 width, uint32 height, uint32 layerWidth, uint32 layerHeight, void *srcTexels, uint32 dataSize,
        eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth,
        ePaletteType paletteType, void *paletteData, uint32 paletteSize,
        eCompressionType compressionType, bool hasAlpha,
        bool& hasDirectlyAcquiredOut
    )
    {

    }
};
#endif

template <typename mipDataType, typename mipListType, typename mipManagerType>
inline bool virtualGetMipmapLayer(
    Interface *engineInterface, mipManagerType& mipManager,
    uint32 mipIndex,
    const mipListType& mipmaps, rawMipmapLayer& layerOut
)
{
    if ( mipIndex >= mipmaps.size() )
        return false;

    const mipDataType& mipLayer = mipmaps[ mipIndex ];

    uint32 mipWidth, mipHeight;
    uint32 layerWidth, layerHeight;

    eRasterFormat rasterFormat;
    eColorOrdering colorOrder;
    uint32 depth;

    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;

    eCompressionType compressionType;

    bool hasAlpha;

    void *texels;
    uint32 dataSize;

    bool isNewlyAllocated;
    bool isPaletteNewlyAllocated;

    mipManager.Deinternalize(
        engineInterface,
        mipLayer,
        mipWidth, mipHeight, layerWidth, layerHeight,
        rasterFormat, colorOrder, depth,
        paletteType, paletteData, paletteSize,
        compressionType, hasAlpha,
        texels, dataSize,
        isNewlyAllocated, isPaletteNewlyAllocated
    );
    
    if ( isNewlyAllocated )
    {
        // Ensure that, if required, our palette is really newly allocated.
        if ( paletteType != PALETTE_NONE && isPaletteNewlyAllocated == false )
        {
            const void *paletteSource = paletteData;

            uint32 palRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

            uint32 palDataSize = getRasterDataSize( paletteSize, palRasterDepth );

            paletteData = engineInterface->PixelAllocate( palDataSize );

            memcpy( paletteData, paletteSource, palDataSize );
        }
    }

    layerOut.rasterFormat = rasterFormat;
    layerOut.depth = depth;
    layerOut.colorOrder = colorOrder;

    layerOut.paletteType = paletteType;
    layerOut.paletteData = paletteData;
    layerOut.paletteSize = paletteSize;

    layerOut.compressionType = compressionType;

    layerOut.hasAlpha = hasAlpha;

    layerOut.mipData.width = mipWidth;
    layerOut.mipData.height = mipHeight;

    layerOut.mipData.mipWidth = layerWidth;
    layerOut.mipData.mipHeight = layerHeight;

    layerOut.mipData.texels = texels;
    layerOut.mipData.dataSize = dataSize;

    layerOut.isNewlyAllocated = isNewlyAllocated;

    return true;
}

inline bool getMipmapLayerDimensions( uint32 mipLevel, uint32 baseWidth, uint32 baseHeight, uint32& layerWidth, uint32& layerHeight )
{
    mipGenLevelGenerator mipGen( baseWidth, baseHeight );

    if ( mipGen.isValidLevel() == false )
        return false;

    for ( uint32 n = 0; n < mipLevel; n++ )
    {
        bool couldIncrement = mipGen.incrementLevel();

        if ( couldIncrement == false )
        {
            return false;
        }
    }

    layerWidth = mipGen.getLevelWidth();
    layerHeight = mipGen.getLevelHeight();

    return true;
}

template <typename mipDataType, typename mipListType, typename mipManagerType>
inline bool virtualAddMipmapLayer(
    Interface *engineInterface, mipManagerType& mipManager,
    mipListType& mipmaps, const rawMipmapLayer& layerIn,
    texNativeTypeProvider::acquireFeedback_t& feedbackOut
)
{
    uint32 newMipIndex = mipmaps.size();

    uint32 layerWidth, layerHeight;

    mipDataType newLayer;

    if ( newMipIndex > 0 )
    {
        uint32 baseLayerWidth, baseLayerHeight;

        mipManager.GetLayerDimensions( mipmaps[ 0 ], baseLayerWidth, baseLayerHeight );

        bool couldFetchDimensions =
            getMipmapLayerDimensions(
                newMipIndex,
                baseLayerWidth, baseLayerHeight,
                layerWidth, layerHeight
            );

        if ( !couldFetchDimensions )
            return false;

        // Make sure the mipmap layer dimensions match.
        if ( layerWidth != layerIn.mipData.mipWidth ||
             layerHeight != layerIn.mipData.mipHeight )
        {
            return false;
        }

        bool hasDirectlyAcquired;
        bool hasAcquiredPalette = false;    // TODO?

        try
        {
            mipManager.Internalize(
                engineInterface,
                newLayer,
                layerIn.mipData.width, layerIn.mipData.height, layerIn.mipData.mipWidth, layerIn.mipData.mipHeight, layerIn.mipData.texels, layerIn.mipData.dataSize,
                layerIn.rasterFormat, layerIn.colorOrder, layerIn.depth,
                layerIn.paletteType, layerIn.paletteData, layerIn.paletteSize,
                layerIn.compressionType, layerIn.hasAlpha,
                hasDirectlyAcquired
            );
        }
        catch( RwException& )
        {
            // Alright, we failed.
            return false;
        }

        feedbackOut.hasDirectlyAcquired = hasDirectlyAcquired;
    }
    else
    {
        // TODO: add support for adding mipmaps if the texture is empty.
        // we potentially have to convert the texels to a compatible format first.
        return false;
    }

    // Add this layer.
    mipmaps.push_back( newLayer );

    return true;
}

template <typename mipDataType, typename mipListType>
inline void virtualClearMipmaps( Interface *engineInterface, mipListType& mipmaps )
{
    uint32 mipmapCount = mipmaps.size();

    if ( mipmapCount > 1 )
    {
        for ( uint32 n = 1; n < mipmapCount; n++ )
        {
            mipDataType& mipLayer = mipmaps[ n ];

            if ( void *texels = mipLayer.texels )
            {
                engineInterface->PixelFree( texels );

                mipLayer.texels = NULL;
            }
        }

        mipmaps.resize( 1 );
    }
}

}