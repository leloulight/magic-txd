#include "MagicFormats.h"

class FormatV8U8 : public MagicFormat
{
	D3DFORMAT GetD3DFormat(void) const override
	{
		return D3DFMT_V8U8;
	}

	const char* GetFormatName(void) const override
	{
		return "V8U8";
	}

	struct pixel_t
	{
		unsigned char u;
		unsigned char v;
	};

	size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const override
	{
		return width * height * 2;
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
		const pixel_t *encodedColors = (pixel_t*)texData;
		for (unsigned int n = 0; n < texItemCount; n++)
		{
			const pixel_t *theTexel = encodedColors + n;
			MagicPutTexelRGBA(texOut, n, RASTER_8888, 32, COLOR_BGRA, theTexel->u, theTexel->v, 0, 255);
		}
	}

	void ConvertFromRW(unsigned int texMipWidth, unsigned int texMipHeight, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat,
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut) const override
	{
		unsigned int texItemCount = (texMipWidth * texMipHeight);
		pixel_t *encodedColors = (pixel_t*)texOut;
		for (unsigned int n = 0; n < texItemCount; n++)
		{
			unsigned char r, g, b, a;
			MagicBrowseTexelRGBA(texelSource, n, rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, r, g, b, a);
			pixel_t *theTexel = (encodedColors + n);
			theTexel->u = r;
			theTexel->v = g;
		}
	}
};

static FormatV8U8 v8u8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance()
{
	return &v8u8Format;
}