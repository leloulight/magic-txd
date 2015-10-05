#include "StdInc.h"

#include "txdread.size.hxx"

#include "txdread.rasterplg.hxx"

namespace rw
{

inline void GetNativeTextureBaseDimensions( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider, uint32& baseWidth, uint32& baseHeight )
{
    nativeTextureBatchedInfo info;

    texTypeProvider->GetTextureInfo( engineInterface, nativeTexture, info );

    baseWidth = info.baseWidth;
    baseHeight = info.baseHeight;
}

// Resize filtering plugin, used to store filtering plugions.
struct resizeFilteringEnv
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        LIST_CLEAR( this->filters.root );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister all filters.
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            item->~filterPluginEntry();

        LIST_FOREACH_END

        LIST_CLEAR( this->filters.root );
    }

    // Registration entry for a filtering plugin.
    struct filterPluginEntry
    {
        std::string filterName;

        rasterResizeFilterInterface *intf;

        RwListEntry <filterPluginEntry> node;
    };

    RwList <filterPluginEntry> filters;
    
    inline filterPluginEntry* FindPluginByName( const char *name ) const
    {
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            if ( item->filterName == name )
            {
                return item;
            }

        LIST_FOREACH_END

        return NULL;
    }

    inline filterPluginEntry* FindPluginByInterface( rasterResizeFilterInterface *intf ) const
    {
        LIST_FOREACH_BEGIN( filterPluginEntry, this->filters.root, node )

            if ( item->intf == intf )
            {
                return item;
            }

        LIST_FOREACH_END

        return NULL;
    }
};

static PluginDependantStructRegister <resizeFilteringEnv, RwInterfaceFactory_t> resizeFilteringEnvRegister;

bool RegisterResizeFiltering( EngineInterface *engineInterface, const char *filterName, rasterResizeFilterInterface *intf )
{
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.GetPluginStruct( engineInterface );

    bool success = false;

    if ( filterEnv )
    {
        // Only register if a plugin by that name does not exist already.
        resizeFilteringEnv::filterPluginEntry *alreadyExists = filterEnv->FindPluginByName( filterName );

        if ( alreadyExists == NULL )
        {
            // Add it.
            void *entryMem = engineInterface->MemAllocate( sizeof( resizeFilteringEnv::filterPluginEntry ) );

            if ( entryMem )
            {
                resizeFilteringEnv::filterPluginEntry *filter_entry = new (entryMem) resizeFilteringEnv::filterPluginEntry();

                filter_entry->filterName = filterName;
                filter_entry->intf = intf;
                
                LIST_INSERT( filterEnv->filters.root, filter_entry->node );

                success = true;
            }
        }
    }

    return success;
}

bool UnregisterResizeFiltering( EngineInterface *engineInterface, rasterResizeFilterInterface *intf )
{
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.GetPluginStruct( engineInterface );

    bool success = false;

    if ( filterEnv )
    {
        // If we find it, we remove it.
        resizeFilteringEnv::filterPluginEntry *foundPlugin = filterEnv->FindPluginByInterface( intf );

        if ( foundPlugin )
        {
            // Remove and delete it.
            LIST_REMOVE( foundPlugin->node );

            foundPlugin->~filterPluginEntry();

            // Now free the memory.
            engineInterface->MemFree( foundPlugin );

            success = true;
        }
    }

    return success;
}

enum class eSamplingType
{
    SAME,
    UPSCALING,
    DOWNSAMPLING
};

inline eSamplingType determineSamplingType( uint32 origDimm, uint32 newDimm )
{
    if ( origDimm == newDimm )
    {
        return eSamplingType::SAME;
    }
    else if ( origDimm < newDimm )
    {
        return eSamplingType::UPSCALING;
    }
    else if ( origDimm > newDimm )
    {
        return eSamplingType::DOWNSAMPLING;
    }

    return eSamplingType::SAME;
}

struct mipmapLayerResizeColorPipeline : public resizeColorPipeline
{
private:
    uint32 depth;
    uint32 rowAlignment;
    uint32 rowSize;

    uint32 layerWidth, layerHeight;
    void *texelSource;

    colorModelDispatcher <void> dispatch;

public:
    uint32 coord_mult_x, coord_mult_y;

    inline mipmapLayerResizeColorPipeline(
        eRasterFormat rasterFormat, uint32 depth, uint32 rowAlignment, eColorOrdering colorOrder,
        ePaletteType paletteType, const void *paletteData, uint32 paletteSize
    ) : dispatch( rasterFormat, colorOrder, depth, paletteData, paletteSize, paletteType )
    {
        this->depth = depth;
        this->rowAlignment = rowAlignment;

        this->rowSize = 0;
        this->layerWidth = 0;
        this->layerHeight = 0;
        this->texelSource = NULL;

        this->coord_mult_x = 1;
        this->coord_mult_y = 1;
    }

    inline mipmapLayerResizeColorPipeline( const mipmapLayerResizeColorPipeline& right )
        : dispatch( right.dispatch )
    {
        this->depth = right.depth;
        this->rowAlignment = right.rowAlignment;
        this->rowSize = right.rowSize;
        this->layerWidth = right.layerWidth;
        this->layerHeight = right.layerHeight;
       
        this->rowSize = 0;
        this->layerWidth = 0;
        this->layerHeight = 0;
        this->texelSource = NULL;
    }

    inline void SetMipmapData( void *texelSource, uint32 layerWidth, uint32 layerHeight )
    {
        this->rowSize = getRasterDataRowSize( layerWidth, depth, rowAlignment );

        this->layerWidth = layerWidth;
        this->layerHeight = layerHeight;
        this->texelSource = texelSource;
    }

    eColorModel getColorModel( void ) const override
    {
        return dispatch.getColorModel();
    }
    
    bool fetchcolor( uint32 x, uint32 y, abstractColorItem& colorOut ) const override
    {
        bool gotColor = false;

        x *= this->coord_mult_x;
        y *= this->coord_mult_y;

        uint32 layerWidth = this->layerWidth;
        uint32 layerHeight = this->layerHeight;

        if ( x < layerWidth && y < layerHeight )
        {
            void *srcRow = getTexelDataRow( this->texelSource, this->rowSize, y );

            dispatch.getColor( srcRow, x, colorOut );

            gotColor = true;
        }

        return gotColor;
    }

    bool putcolor( uint32 x, uint32 y, const abstractColorItem& colorIn ) override
    {
        bool putColor = false;

        x *= this->coord_mult_x;
        y *= this->coord_mult_y;

        uint32 layerWidth = this->layerWidth;
        uint32 layerHeight = this->layerHeight;

        if ( x < layerWidth && y < layerHeight )
        {
            void *dstRow = getTexelDataRow( this->texelSource, this->rowSize, y );

            dispatch.setColor( dstRow, x, colorIn );

            putColor = true;
        }

        return putColor;
    }
};

struct filterDimmProcess
{
    inline filterDimmProcess( double dimmAdvance, uint32 origDimmLimit )
    {
        this->dimmAdvance = dimmAdvance;

        this->dimmIter = 0;
        this->preciseDimmIter = 0;

        this->origDimmIter = 0;
        this->origDimmLimit = origDimmLimit;
    }

    inline bool IsEnd( void ) const
    {
        return ( origDimmIter == origDimmLimit );
    }

    static uint32 FilterIter( double iter )
    {
        return (uint32)round( iter );
    }

    inline void Increment( void )
    {
        this->preciseDimmIter += this->dimmAdvance;
        this->dimmIter = FilterIter( this->preciseDimmIter );

        this->origDimmIter++;
    }

    inline void Resolve( uint32& origDimmIter, uint32& filterStart, uint32& filterSize ) const
    {
        uint32 _filterStart = this->dimmIter;
        uint32 filterEnd = FilterIter( this->preciseDimmIter + this->dimmAdvance );

        filterStart = _filterStart;
        filterSize = filterEnd - _filterStart;

        assert( filterSize >= 1 );

        origDimmIter = this->origDimmIter;
    }

private:
    uint32 dimmIter;
    double preciseDimmIter;
    double dimmAdvance;

    uint32 origDimmIter;
    uint32 origDimmLimit;
};

template <typename filteringProcessor>
AINLINE void filteringDispatcher2D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double widthProcessRatio, double heightProcessRatio,
    filteringProcessor& processor
)
{
    filterDimmProcess heightProcess( heightProcessRatio, surfProcHeight );

    while ( heightProcess.IsEnd() == false )
    {
        uint32 dstY;

        uint32 heightFilterStart, heightFilterSize;
        heightProcess.Resolve( dstY, heightFilterStart, heightFilterSize );

        filterDimmProcess widthProcess( widthProcessRatio, surfProcWidth );

        while ( widthProcess.IsEnd() == false )
        {
            uint32 dstX;

            uint32 widthFilterStart, widthFilterSize;
            widthProcess.Resolve( dstX, widthFilterStart, widthFilterSize );

            // Do the filtering.
            processor.Process(
                widthFilterStart, heightFilterStart,
                widthFilterSize, heightFilterSize,
                dstX, dstY
            );

            widthProcess.Increment();
        }

        heightProcess.Increment();
    }
}

template <typename filteringProcessor>
AINLINE void filteringDispatcherWidth1D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double widthProcessRatio,
    filteringProcessor& processor
)
{
    for ( uint32 y = 0; y < surfProcHeight; y++ )
    {
        filterDimmProcess widthProcess( widthProcessRatio, surfProcWidth );

        while ( widthProcess.IsEnd() == false )
        {
            uint32 dstX;

            uint32 widthFilterStart, widthFilterSize;
            widthProcess.Resolve( dstX, widthFilterStart, widthFilterSize );

            // Do the filtering.
            processor.Process(
                widthFilterStart, y,
                widthFilterSize, 1,
                dstX, y
            );

            widthProcess.Increment();
        }
    }
}

template <typename filteringProcessor>
AINLINE void filteringDispatcherHeight1D(
    uint32 surfProcWidth, uint32 surfProcHeight,
    double heightProcessRatio,
    filteringProcessor& processor
)
{
    filterDimmProcess heightProcess( heightProcessRatio, surfProcHeight );

    while ( heightProcess.IsEnd() == false )
    {
        uint32 dstY;

        uint32 heightFilterStart, heightFilterSize;
        heightProcess.Resolve( dstY, heightFilterStart, heightFilterSize );

        for ( uint32 x = 0; x < surfProcWidth; x++ )
        {
            // Do the filtering.
            processor.Process(
                x, heightFilterStart,
                1, heightFilterSize,
                x, dstY
            );
        }

        heightProcess.Increment();
    }
}

struct minifyFiltering2D
{
    AINLINE minifyFiltering2D(
        const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
        rasterResizeFilterInterface *downsampleFilter
    ) : srcColorPipe( srcColorPipe ), dstColorPipe( dstColorPipe )
    {
        this->downsampleFilter = downsampleFilter;
    }

    AINLINE void Process(
        uint32 widthFilterStart, uint32 heightFilterStart,
        uint32 widthFilterSize, uint32 heightFilterSize,
        uint32 dstX, uint32 dstY
    )
    {
        abstractColorItem resultColorItem;

        downsampleFilter->MinifyFiltering(
            srcColorPipe,
            widthFilterStart, heightFilterStart,
            widthFilterSize, heightFilterSize,
            resultColorItem
        );

        // Store the result color.
        dstColorPipe.putcolor( dstX, dstY, resultColorItem );
    }

private:
    const mipmapLayerResizeColorPipeline& srcColorPipe;
    mipmapLayerResizeColorPipeline& dstColorPipe;

    rasterResizeFilterInterface *downsampleFilter;
};

struct magnifyFiltering2D
{
    AINLINE magnifyFiltering2D(
        const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
        rasterResizeFilterInterface *upscaleFilter
    ) : srcColorPipe( srcColorPipe ), dstColorPipe( dstColorPipe )
    {
        this->upscaleFilter = upscaleFilter;
    }

    AINLINE void Process(
        uint32 widthFilterStart, uint32 heightFilterStart,
        uint32 widthFilterSize, uint32 heightFilterSize,
        uint32 srcX, uint32 srcY
    )
    {
        upscaleFilter->MagnifyFiltering(
            srcColorPipe,
            widthFilterStart, heightFilterStart,
            widthFilterSize, heightFilterSize,
            dstColorPipe,
            srcX, srcY
        );
    }

private:
    const mipmapLayerResizeColorPipeline& srcColorPipe;
    mipmapLayerResizeColorPipeline& dstColorPipe;

    rasterResizeFilterInterface *upscaleFilter;
};

AINLINE void performFiltering1D(
    EngineInterface *engineInterface,
    mipmapLayerResizeColorPipeline& dstColorPipe,   // cached thing.
    void *transMipData,                             // buffer we expect the final result in
    uint32 sampleDepth,                             // format of the (raw raster) result buffer.
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight, void *rawOrigTexels,
    eRasterFormat rasterFormat, eColorOrdering colorOrder, uint32 depth, uint32 rowAlignment,
    ePaletteType paletteType, void *paletteData, uint32 paletteSize,
    eSamplingType mipHoriSampling, eSamplingType mipVertSampling,
    rasterResizeFilterInterface *upscaleFilter, rasterResizeFilterInterface *downsamplingFilter
)
{
    // We need to do unoptimized filtering.
    // This is splitting up a potentially 2D filtering into two 1D operations.
    uint32 currentWidth = rawOrigLayerWidth;
    uint32 currentHeight = rawOrigLayerHeight;

    void *currentTexels = rawOrigTexels;

    bool currentTexelsHasAllocated = false;

    uint32 currentDepth = depth;
    ePaletteType currentPaletteType = paletteType;
    void *currentPaletteData = paletteData;
    uint32 currentPaletteSize = paletteSize;

    try
    {
        if ( mipHoriSampling != eSamplingType::SAME )
        {
            // We need a new target buffer.
            uint32 redirTargetWidth = targetLayerWidth;
            uint32 redirTargetHeight = currentHeight;

            void *redirTargetTexels = NULL;

            bool redirHasAllocated = false;

            if ( redirTargetWidth == targetLayerWidth && redirTargetHeight == targetLayerHeight )
            {
                redirTargetTexels = transMipData;
            }
            else
            {
                uint32 redirTargetRowSize = getRasterDataRowSize( redirTargetWidth, sampleDepth, rowAlignment );

                uint32 redirTargetDataSize = getRasterDataSizeByRowSize( redirTargetRowSize, redirTargetHeight );

                redirTargetTexels = engineInterface->PixelAllocate( redirTargetDataSize );

                redirHasAllocated = true;
            }

            if ( !redirTargetTexels )
            {
                throw RwException( "failed to allocate temporary filtering transformation buffer for resizing" );
            }

            try
            {
                // Set up the appropriate targets and sources.
                dstColorPipe.SetMipmapData( redirTargetTexels, redirTargetWidth, redirTargetHeight );

                mipmapLayerResizeColorPipeline srcDynamicPipe(
                    rasterFormat, currentDepth, rowAlignment, colorOrder, 
                    currentPaletteType, currentPaletteData, currentPaletteSize
                );

                srcDynamicPipe.SetMipmapData(
                    currentTexels, currentWidth, currentHeight
                );

                if ( mipHoriSampling == eSamplingType::UPSCALING )
                {
                    magnifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        upscaleFilter
                    );

                    double widthProcessRatio = (double)redirTargetWidth / (double)currentWidth;
                                    
                    filteringDispatcherWidth1D(
                        currentWidth, currentHeight,
                        widthProcessRatio, filterProc
                    );
                }
                else if ( mipHoriSampling == eSamplingType::DOWNSAMPLING )
                {
                    minifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        downsamplingFilter
                    );

                    double widthProcessRatio = (double)currentWidth / (double)redirTargetWidth;

                    filteringDispatcherWidth1D(
                        redirTargetWidth, redirTargetHeight,
                        widthProcessRatio, filterProc
                    );
                }
            }
            catch( ... )
            {
                if ( redirHasAllocated )
                {
                    engineInterface->PixelFree( redirTargetTexels );
                }

                throw;
            }

            if ( currentTexelsHasAllocated )
            {
                engineInterface->PixelFree( currentTexels );
            }

            // We use those texels as current texels now.
            currentTexels = redirTargetTexels;

            currentWidth = redirTargetWidth;
            currentHeight = redirTargetHeight;

            currentTexelsHasAllocated = redirHasAllocated;

            // meh: after resizing we definitely know that we cannot have a palettized
            // currentTexels buffer. that is why we update properties here.
            currentPaletteType = PALETTE_NONE;
            currentPaletteData = NULL;
            currentPaletteSize = 0;
            currentDepth = sampleDepth;
        }

        if ( mipVertSampling != eSamplingType::SAME )
        {
            // We need a new target buffer.
            uint32 redirTargetWidth = currentWidth;
            uint32 redirTargetHeight = targetLayerHeight;

            void *redirTargetTexels = NULL;

            bool redirHasAllocated = false;

            if ( redirTargetWidth == targetLayerWidth && redirTargetHeight == targetLayerHeight )
            {
                redirTargetTexels = transMipData;
            }
            else
            {
                uint32 redirTargetRowSize = getRasterDataRowSize( redirTargetWidth, sampleDepth, rowAlignment );

                uint32 redirTargetDataSize = getRasterDataSizeByRowSize( redirTargetRowSize, redirTargetHeight );

                redirTargetTexels = engineInterface->PixelAllocate( redirTargetDataSize );

                redirHasAllocated = true;
            }

            if ( !redirTargetTexels )
            {
                throw RwException( "failed to allocate temporary filtering transformation buffer for resizing" );
            }

            try
            {
                // Set up the appropriate targets and sources.
                dstColorPipe.SetMipmapData( redirTargetTexels, redirTargetWidth, redirTargetHeight );

                mipmapLayerResizeColorPipeline srcDynamicPipe(
                    rasterFormat, currentDepth, rowAlignment, colorOrder, 
                    currentPaletteType, currentPaletteData, currentPaletteSize
                );

                srcDynamicPipe.SetMipmapData(
                    currentTexels, currentWidth, currentHeight
                );

                if ( mipVertSampling == eSamplingType::UPSCALING )
                {
                    magnifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        upscaleFilter
                    );

                    double heightProcessRatio = (double)redirTargetHeight / (double)currentHeight;
                                    
                    filteringDispatcherHeight1D(
                        currentWidth, currentHeight,
                        heightProcessRatio, filterProc
                    );
                }
                else if ( mipVertSampling == eSamplingType::DOWNSAMPLING )
                {
                    minifyFiltering2D filterProc(
                        srcDynamicPipe, dstColorPipe,
                        downsamplingFilter
                    );

                    double heightProcessRatio = (double)currentHeight / (double)redirTargetHeight;

                    filteringDispatcherHeight1D(
                        redirTargetWidth, redirTargetHeight,
                        heightProcessRatio, filterProc
                    );
                }
            }
            catch( ... )
            {
                if ( redirHasAllocated )
                {
                    engineInterface->PixelFree( redirTargetTexels );
                }

                throw;
            }

            if ( currentTexelsHasAllocated )
            {
                engineInterface->PixelFree( currentTexels );
            }

            // We use those texels as current texels now.
            currentTexels = redirTargetTexels;

            currentWidth = redirTargetWidth;
            currentHeight = redirTargetHeight;

            currentTexelsHasAllocated = redirHasAllocated;

            // meh: after resizing we definitely know that we cannot have a palettized
            // currentTexels buffer. that is why we update properties here.
            currentPaletteType = PALETTE_NONE;
            currentPaletteData = NULL;
            currentPaletteSize = 0;
            currentDepth = sampleDepth;
        }
    }
    catch( ... )
    {
        if ( currentTexelsHasAllocated )
        {
            engineInterface->PixelFree( currentTexels );
        }

        throw;
    }
}

AINLINE void performMinifyFiltering2D(
    const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight,
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    rasterResizeFilterInterface *downsampleFilter
)
{
    minifyFiltering2D filterProc(
        srcColorPipe, dstColorPipe,
        downsampleFilter
    );

    double widthProcessRatio = (double)rawOrigLayerWidth / (double)targetLayerWidth;
    double heightProcessRatio = (double)rawOrigLayerHeight / (double)targetLayerHeight;

    filteringDispatcher2D(
        targetLayerWidth, targetLayerHeight,
        widthProcessRatio,
        heightProcessRatio,
        filterProc
    );
}

AINLINE void performMagnifyFiltering2D(
    const mipmapLayerResizeColorPipeline& srcColorPipe, mipmapLayerResizeColorPipeline& dstColorPipe,
    uint32 rawOrigLayerWidth, uint32 rawOrigLayerHeight,
    uint32 targetLayerWidth, uint32 targetLayerHeight,
    rasterResizeFilterInterface *upscaleFilter
)
{
    magnifyFiltering2D filterProc(
        srcColorPipe, dstColorPipe,
        upscaleFilter
    );

    double widthProcessRatio = (double)targetLayerWidth / (double)rawOrigLayerWidth;
    double heightProcessRatio    = (double)targetLayerHeight / (double)rawOrigLayerHeight;

    filteringDispatcher2D(
        rawOrigLayerWidth, rawOrigLayerHeight,
        widthProcessRatio,
        heightProcessRatio,
        filterProc
    );
}

void Raster::resize(uint32 newWidth, uint32 newHeight, const char *downsampleMode, const char *upscaleMode)
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    EngineInterface *engineInterface = (EngineInterface*)this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    // TODO: improve this check by doing it for every mipmap level!
    // I know this can be skipped, but better write idiot-proof code.
    // who knows, maybe we would be the idiots later on.

    // Determine whether we have to do upscaling or downsampling on either axis.
    uint32 baseWidth, baseHeight;

    GetNativeTextureBaseDimensions( engineInterface, platformTex, texProvider, baseWidth, baseHeight );

    eSamplingType horiSampling = determineSamplingType( baseWidth, newWidth );
    eSamplingType vertSampling = determineSamplingType( baseHeight, newHeight );

    // If there is nothing to do, we can quit here.
    if ( horiSampling == eSamplingType::SAME && vertSampling == eSamplingType::SAME )
        return;

    // Verify that the dimensions are even accepted by the native texture.
    // If they are, we can safely go on. Otherwise we have to tell the user to pick better dimensions!
    {
        nativeTextureSizeRules sizeRules;

        texProvider->GetTextureSizeRules( platformTex, sizeRules );

        if ( sizeRules.IsMipmapSizeValid( newWidth, newHeight ) == false )
        {
            throw RwException( "dimensions given for raster resize routine are invalid for that native texture type" );
        }
    }

    // Get the filter plugin environment.
    resizeFilteringEnv *filterEnv = resizeFilteringEnvRegister.GetPluginStruct( engineInterface );

    if ( !filterEnv )
    {
        throw RwException( "filtering environment unavailable" );
    }

    // If the user has not decided for a filtering plugin yet, choose the default.
    if ( !downsampleMode )
    {
        downsampleMode = "blur";
    }

    if ( !upscaleMode )
    {
        upscaleMode = "linear";
    }

    rasterResizeFilterInterface *downsamplingFilter = NULL;
    rasterResizeFilterInterface *upscaleFilter = NULL;
    {
        // Determine the sampling plugin that the runtime wants us to use.
        resizeFilteringEnv::filterPluginEntry *samplingPluginCont = filterEnv->FindPluginByName( downsampleMode );

        if ( !samplingPluginCont )
        {
            throw RwException( "failed to find resize downsample filtering plugin" );
        }

        downsamplingFilter = samplingPluginCont->intf;

        resizeFilteringEnv::filterPluginEntry *upscalePluginCont = filterEnv->FindPluginByName( upscaleMode );

        if ( !upscalePluginCont )
        {
            throw RwException( "failed to find resize upscale filtering plugin" );
        }

        upscaleFilter = upscalePluginCont->intf;
    }

    resizeFilteringCaps downsamplingCaps;
    resizeFilteringCaps upscaleCaps;

    downsamplingFilter->GetSupportedFiltering( downsamplingCaps );
    upscaleFilter->GetSupportedFiltering( upscaleCaps );

    if ( downsamplingCaps.supportsMinification == false )
    {
        throw RwException( "selected downsampling filter does not support minification" );
    }

    if ( upscaleCaps.supportsMagnification == false )
    {
        throw RwException( "selected upscaling filter does not support magnification" );
    }

    // Get the pixel data of the original raster that we will operate on.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    // Since we will replace all mipmaps, free the data in the texture.
    texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

    pixelData.SetStandalone();

    // We will set the same pixel storage data to the texture.
    texNativeTypeProvider::acquireFeedback_t acquireFeedback;

    try
    {
        // Alright. We shall allocate a new target buffer for the filtering operation.
        // This buffer should have the same format as the original texels, kinda.
        eRasterFormat rasterFormat = pixelData.rasterFormat;
        uint32 depth = pixelData.depth;
        eColorOrdering colorOrder = pixelData.colorOrder;
        uint32 rowAlignment = pixelData.rowAlignment;

        // If the original texture was a palette or was compressed, we will unfortunately
        // have to recompress or remap the pixels, basically putting it into the original format.
        ePaletteType paletteType = pixelData.paletteType;
        void *paletteData = pixelData.paletteData;
        uint32 paletteSize = pixelData.paletteSize;
        eCompressionType compressionType = pixelData.compressionType;

        // If the raster data is not currently in raw raster mode, we have to use a temporary raster that is full color.
        // Otherwise we have no valid configuration.
        eRasterFormat tmpRasterFormat;
        eColorOrdering tmpColorOrder;
        uint32 tmpItemDepth;

        if ( compressionType != RWCOMPRESS_NONE )
        {
            tmpRasterFormat = RASTER_8888;
            tmpColorOrder = COLOR_BGRA;
            tmpItemDepth = Bitmap::getRasterFormatDepth( tmpRasterFormat );
        }
        else
        {
            tmpRasterFormat = rasterFormat;
            tmpColorOrder = colorOrder;
            tmpItemDepth = depth;
        }

        // Since we will have a raw raster transformation buffer, we need to fetch the sample depth.
        uint32 sampleDepth = Bitmap::getRasterFormatDepth( tmpRasterFormat );

        mipmapLayerResizeColorPipeline dstColorPipe(
            tmpRasterFormat, sampleDepth, rowAlignment, tmpColorOrder,
            PALETTE_NONE, NULL, 0
        );

        // Perform for every mipmap.
        size_t origMipmapCount = pixelData.mipmaps.size();

        mipGenLevelGenerator mipGen( newWidth, newHeight );

        if ( !mipGen.isValidLevel() )
        {
            throw RwException( "invalid mipmap dimensions given for resizing" );
        }

        size_t mipIter = 0;

        for ( ; mipIter < origMipmapCount; mipIter++ )
        {
            pixelDataTraversal::mipmapResource& dstMipLayer = pixelData.mipmaps[ mipIter ];

            uint32 origMipWidth = dstMipLayer.width;
            uint32 origMipHeight = dstMipLayer.height;
            
            uint32 origMipLayerWidth = dstMipLayer.mipWidth;
            uint32 origMipLayerHeight = dstMipLayer.mipHeight;

            void *origTexels = dstMipLayer.texels;
            uint32 origDataSize = dstMipLayer.dataSize;

            // Determine the size that this mipmap will have if resized.
            bool hasEstablishedLevel = true;

            if ( mipIter != 0 )
            {
                hasEstablishedLevel = mipGen.incrementLevel();
            }

            if ( !hasEstablishedLevel )
            {
                // We cannot generate any more mipmaps.
                // The remaining ones will be cleared, if available.
                break;
            }

            uint32 targetLayerWidth = mipGen.getLevelWidth();
            uint32 targetLayerHeight = mipGen.getLevelHeight();

            // Verify certain sampling properties.
            eSamplingType mipHoriSampling = determineSamplingType( origMipLayerWidth, targetLayerWidth );
            eSamplingType mipVertSampling = determineSamplingType( origMipLayerHeight, targetLayerHeight );

            assert( mipHoriSampling == eSamplingType::SAME || horiSampling == mipHoriSampling );
            assert( mipVertSampling == eSamplingType::SAME || vertSampling == mipVertSampling );

            // We will have to read texels from this mip surface.
            // That is why we should make it raw.
            void *rawOrigTexels = NULL;
            uint32 rawOrigDataSize = 0;

            uint32 rawOrigLayerWidth, rawOrigLayerHeight;

            bool rawOrigHasChanged =
                ConvertMipmapLayerNative(
                    engineInterface, origMipWidth, origMipHeight, origMipLayerWidth, origMipLayerHeight, origTexels, origDataSize,
                    rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                    tmpRasterFormat, tmpItemDepth, rowAlignment, tmpColorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                    true,
                    rawOrigLayerWidth, rawOrigLayerHeight,
                    rawOrigTexels, rawOrigDataSize
                );

            try
            {
                // We call this temporary buffer that will be used for scaling the "transformation buffer".
                // It shall be encoded as raw raster.
                uint32 transMipRowSize = getRasterDataRowSize( targetLayerWidth, sampleDepth, rowAlignment );

                uint32 transMipSize = getRasterDataSizeByRowSize( transMipRowSize, targetLayerHeight );

                void *transMipData = engineInterface->PixelAllocate( transMipSize );

                if ( !transMipData )
                {
                    throw RwException( "failed to allocate memory for resize filtering transformation" );
                }

                try
                {
                    // Pretty much ready to do things.
                    // We need to put texels into the buffer, so lets also create a destination pipe.
                    dstColorPipe.SetMipmapData( transMipData, targetLayerWidth, targetLayerHeight );

                    bool hasDoneOptimizedFiltering = false;

                    if ( mipHoriSampling == eSamplingType::DOWNSAMPLING && mipVertSampling == eSamplingType::DOWNSAMPLING )
                    {
                        // Check for support first.
                        if ( downsamplingCaps.minify2D )
                        {
                            // Prepare the virtual surface pipeline.
                            mipmapLayerResizeColorPipeline srcColorPipe(
                                tmpRasterFormat, tmpItemDepth, rowAlignment, tmpColorOrder,
                                paletteType, paletteData, paletteSize
                            );

                            srcColorPipe.SetMipmapData( rawOrigTexels, rawOrigLayerWidth, rawOrigLayerHeight );

                            performMinifyFiltering2D(
                                srcColorPipe, dstColorPipe,
                                rawOrigLayerWidth, rawOrigLayerHeight,
                                targetLayerWidth, targetLayerHeight,
                                downsamplingFilter
                            );

                            hasDoneOptimizedFiltering = true;
                        }
                    }
                    else if ( mipHoriSampling == eSamplingType::UPSCALING && mipVertSampling == eSamplingType::UPSCALING )
                    {
                        if ( upscaleCaps.magnify2D )
                        {
                            // Prepare the virtual surface pipeline.
                            mipmapLayerResizeColorPipeline srcColorPipe(
                                tmpRasterFormat, tmpItemDepth, rowAlignment, tmpColorOrder,
                                paletteType, paletteData, paletteSize
                            );

                            srcColorPipe.SetMipmapData( rawOrigTexels, rawOrigLayerWidth, rawOrigLayerHeight );

                            performMagnifyFiltering2D(
                                srcColorPipe, dstColorPipe,
                                rawOrigLayerWidth, rawOrigLayerHeight,
                                targetLayerWidth, targetLayerHeight,
                                upscaleFilter
                            );

                            hasDoneOptimizedFiltering = true;
                        }
                    }
                    
                    if ( !hasDoneOptimizedFiltering )
                    {
                        // Pretty complicated.
                        // Please report any bugs if you find them.
                        performFiltering1D(
                            engineInterface, dstColorPipe,
                            transMipData, sampleDepth,
                            targetLayerWidth, targetLayerHeight,
                            rawOrigLayerWidth, rawOrigLayerHeight, rawOrigTexels,
                            tmpRasterFormat, tmpColorOrder, tmpItemDepth, rowAlignment,
                            paletteType, paletteData, paletteSize,
                            mipHoriSampling, mipVertSampling,
                            upscaleFilter, downsamplingFilter
                        );
                    }

                    // Encode the raw mipmap into a correct mipmap that the architecture expects.
                    void *encodedTexels = NULL;
                    uint32 encodedDataSize = 0;

                    uint32 encodedWidth, encodedHeight;

                    bool hasChanged =
                        ConvertMipmapLayerNative(
                            engineInterface, targetLayerWidth, targetLayerHeight, targetLayerWidth, targetLayerHeight, transMipData, transMipSize,
                            tmpRasterFormat, sampleDepth, rowAlignment, tmpColorOrder, PALETTE_NONE, NULL, 0, RWCOMPRESS_NONE,
                            rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                            true,
                            encodedWidth, encodedHeight,
                            encodedTexels, encodedDataSize
                        );

                    // If we have new texels now, we can actually free the old ones.
                    if ( encodedTexels != transMipData )
                    {
                        engineInterface->PixelFree( transMipData );
                    }

                    // Update the mipmap layer.
                    engineInterface->PixelFree( origTexels );

                    dstMipLayer.width = encodedWidth;
                    dstMipLayer.height = encodedHeight;
                    dstMipLayer.mipWidth = targetLayerWidth;
                    dstMipLayer.mipHeight = targetLayerHeight;
                    dstMipLayer.texels = encodedTexels;
                    dstMipLayer.dataSize = encodedDataSize;

                    // Success! On to the next.
                }
                catch( ... )
                {
                    // Well, something went horribly wrong.
                    // Let us make sure by freeing the buffer.
                    engineInterface->PixelFree( transMipData );

                    throw;
                }
            }
            catch( ... )
            {
                if ( rawOrigTexels != origTexels )
                {
                    engineInterface->PixelFree( rawOrigTexels );
                }

                throw;
            }

            // We do not need the temporary transformed layer anymore.
            if ( rawOrigTexels != origTexels )
            {
                engineInterface->PixelFree( rawOrigTexels );
            }
        }

        size_t actualMipmapCount = mipIter;

        if ( actualMipmapCount != origMipmapCount )
        {
            // Clear remaining mipmaps.
            for ( ; mipIter < origMipmapCount; mipIter++ )
            {
                pixelDataTraversal::FreeMipmap( engineInterface, pixelData.mipmaps[ mipIter ] );
            }

            pixelData.mipmaps.resize( actualMipmapCount );
        }

        // Store the new mipmaps.
        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    // Free pixels if necessary.
    if ( acquireFeedback.hasDirectlyAcquired == false )
    {
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void Raster::getSize(uint32& width, uint32& height) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    GetNativeTextureBaseDimensions( engineInterface, platformTex, texProvider, width, height );
}

// Filtering plugins.
extern void registerRasterSizeBlurPlugin( void );
extern void registerRasterResizeLinearPlugin( void );

void registerResizeFilteringEnvironment( void )
{
    resizeFilteringEnvRegister.RegisterPlugin( engineFactory );

    // TODO: register all filtering plugins.
    registerRasterSizeBlurPlugin();
    registerRasterResizeLinearPlugin();
}

};