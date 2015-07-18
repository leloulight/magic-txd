#pragma once

#include <renderware.h>

#define MAGIC_CORE
#include <MagicFormats.h>

enum MAGIC_RASTER_FORMAT;
enum MAGIC_COLOR_ORDERING;
enum MAGIC_PALETTE_TYPE;

#define MAGICAPI /*extern "C"*/ __declspec(dllexport)

MAGICAPI bool __stdcall MagicPutTexelRGBA(void *texelSource, unsigned int texelIndex, MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
	MAGIC_COLOR_ORDERING colorOrder, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);

MAGICAPI bool __stdcall MagicBrowseTexelRGBA(const void *texelSource, unsigned int texelIndex, MAGIC_RASTER_FORMAT rasterFormat, 
	unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
	unsigned char& redOut, unsigned char& greenOut, unsigned char& blueOut, unsigned char& alphaOut);