#include "MagicFormats.h"

class FormatA8 : public MagicFormat
{
	D3DFORMAT GetD3DFormat(void) const override
	{
		return D3DFMT_A8;
	}

	const char* GetFormatName(void) const override
	{
		return "A8";
	}

	size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const override
	{
		return width * height;
	}

	void GetTextureRWFormat(MAGIC_RASTER_FORMAT& rasterFormatOut, unsigned int& depthOut, MAGIC_COLOR_ORDERING& colorOrderOut) const
	{
		rasterFormatOut = RASTER_8888;
		depthOut = 32;
		colorOrderOut = COLOR_BGRA;
	}

	void ConvertToRW(const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t texDataSize, void *texOut) const override
	{
		unsigned int texItemCount = (texMipWidth * texMipHeight);
		const unsigned char *encodedColors = (unsigned char*)texData;
		for (unsigned int n = 0; n < texItemCount; n++)
			MagicPutTexelRGBA(texOut, n, RASTER_8888, 32, COLOR_BGRA, 0, 0, 0, encodedColors[n]);
	}

	void ConvertFromRW(unsigned int texMipWidth, unsigned int texMipHeight, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat,
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut) const override
	{
		unsigned int texItemCount = (texMipWidth * texMipHeight);
		unsigned char *encodedColors = (unsigned char *)texOut;
		for (unsigned int n = 0; n < texItemCount; n++)
		{
			unsigned char r, g, b, a;
			MagicBrowseTexelRGBA(texelSource, n, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, r, g, b, a);
			encodedColors[n] = a;
		}
	}
};

static FormatA8 a8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance()
{
	return &a8Format;
}