#include "MagicExport.h"

MAGICAPI bool __stdcall MagicPutTexelRGBA(void *texelSource, unsigned int texelIndex,
	MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth, MAGIC_COLOR_ORDERING colorOrder,
	unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
	return rw::PutTexelRGBA(texelSource, texelIndex, (rw::eRasterFormat)rasterFormat, depth, (rw::eColorOrdering)colorOrder, red, green, blue, alpha);
}

MAGICAPI bool __stdcall MagicBrowseTexelRGBA(const void *texelSource, unsigned int texelIndex, MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
	MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
	unsigned char& redOut, unsigned char& greenOut, unsigned char& blueOut, unsigned char& alphaOut)
{
	return rw::BrowseTexelRGBA(texelSource, texelIndex, (rw::eRasterFormat)rasterFormat, depth, (rw::eColorOrdering)colorOrder,
		(rw::ePaletteType)paletteType, paletteData, paletteSize, redOut, greenOut, blueOut, alphaOut);
}