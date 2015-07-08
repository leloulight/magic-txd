#include "MagicFormats.h"

inline unsigned char rgbToLuminance(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned int colorSumm = (r + g + b);

	return (colorSumm / 3);
}

class FormatA8L8 : public MagicFormat
{
	D3DFORMAT GetD3DFormat(void) const override
	{
		return D3DFMT_A8L8;
	}

	const char* GetFormatName(void) const override
	{
		return "A8L8";
	}

	struct pixel_t
	{
		unsigned char lum;
		unsigned char alpha;
	};

	size_t GetFormatTextureDataSize(unsigned int width, unsigned int height) const override
	{
		return (width * height * sizeof(pixel_t));
	}

	void GetTextureRWFormat(MAGIC_RASTER_FORMAT& rasterFormatOut, unsigned int& depthOut, MAGIC_COLOR_ORDERING& colorOrderOut) const
	{
		rasterFormatOut = RASTER_8888;
		depthOut = 32;
		colorOrderOut = COLOR_BGRA;
	}

	void ConvertToRW(
		const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t texDataSize,
		void *texOut
		) const override
	{
		const MAGIC_RASTER_FORMAT rasterFormat = RASTER_8888;
		const unsigned int depth = 32;
		const MAGIC_COLOR_ORDERING colorOrder = COLOR_BGRA;

		// do the conversion.
		unsigned int texItemCount = (texMipWidth * texMipHeight);

		const pixel_t *encodedColors = (pixel_t*)texData;

		for (unsigned int n = 0; n < texItemCount; n++)
		{
			// We are simply a pixel_t.
			const pixel_t *theTexel = encodedColors + n;

			MagicPutTexelRGBA(texOut, n, rasterFormat, depth, colorOrder, theTexel->lum, theTexel->lum, theTexel->lum, theTexel->alpha);
		}

		// Alright, we are done!
	}

	void ConvertFromRW(
		unsigned int texMipWidth, unsigned int texMipHeight, const void *texelSource, MAGIC_RASTER_FORMAT rasterFormat, 
		unsigned int depth, MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
		void *texOut
		) const override
	{
		// We write stuff.
		unsigned int texItemCount = (texMipWidth * texMipHeight);

		pixel_t *encodedColors = (pixel_t*)texOut;

		for (unsigned int n = 0; n < texItemCount; n++)
		{
			// Get the color as RGBA and convert to closely matching luminance value.
			unsigned char r, g, b, a;

			MagicBrowseTexelRGBA(
				texelSource, n,
				rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize,
				r, g, b, a
				);

			unsigned char lumVal = rgbToLuminance(r, g, b);

			pixel_t *theTexel = (encodedColors + n);

			theTexel->lum = lumVal;
			theTexel->alpha = a;
		}

		// Done!
	}
};

// If your format is not used by the engine, you can be sure that a more fitting standard way for it exists
// that is bound natively by the library ;)
static FormatA8L8 a8l8Format;

MAGICAPI MagicFormat * __MAGICCALL GetFormatInstance()
{
	return &a8l8Format;
}