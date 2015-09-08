#include <cstring>
#include <assert.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#include "pluginutil.hxx"

#include "txdread.common.hxx"

#include "pixelformat.hxx"

#include "pixelutil.hxx"

#include "txdread.nativetex.hxx"

namespace rw
{

struct resizeFilteringCaps
{
    bool supportsMagnification;
    bool supportsMinification;

    bool magnify2D;
    bool minify2D;
};

struct resizeColorPipeline abstract
{
    virtual eColorModel getColorModel( void ) const = 0;

    virtual bool fetchcolor( uint32 x, uint32 y, abstractColorItem& colorOut ) const = 0;
    virtual bool putcolor( uint32 x, uint32 y, const abstractColorItem& colorIn ) = 0;
};

struct rasterResizeFilterInterface abstract
{
    virtual void GetSupportedFiltering( resizeFilteringCaps& filterOut ) const = 0;

    virtual void MagnifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 magX, uint32 magY, uint32 magScaleX, uint32 magScaleY,
        resizeColorPipeline& dstBmp, uint32 dstX, uint32 dstY
    ) const = 0;
    virtual void MinifyFiltering(
        const resizeColorPipeline& srcBmp, uint32 minX, uint32 minY, uint32 minScaleX, uint32 minScaleY,
        abstractColorItem& reducedColor
    ) const = 0;
};

// Filtering API interface. Used to register native filtering plugins.
bool RegisterResizeFiltering( EngineInterface *engineInterface, const char *filterName, rasterResizeFilterInterface *intf );
bool UnregisterResizeFiltering( EngineInterface *engineInterface, rasterResizeFilterInterface *intf );

};