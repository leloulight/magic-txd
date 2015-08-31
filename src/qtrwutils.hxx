// Utilities for interfacing between Qt and rwlib.
// Should not be included into the global headers, this is an on-demand component.

inline QImage convertRWBitmapToQImage( const rw::Bitmap& rasterBitmap )
{
	rw::uint32 width, height;
	rasterBitmap.getSize(width, height);

	QImage texImage(width, height, QImage::Format::Format_ARGB32);

	// Copy scanline by scanline.
	for (int y = 0; y < height; y++)
	{
		uchar *scanLineContent = texImage.scanLine(y);

		QRgb *colorItems = (QRgb*)scanLineContent;

		for (int x = 0; x < width; x++)
		{
			QRgb *colorItem = (colorItems + x);

			unsigned char r, g, b, a;

			rasterBitmap.browsecolor(x, y, r, g, b, a);

			*colorItem = qRgba(r, g, b, a);
		}
	}

    return texImage;
}

inline QPixmap convertRWBitmapToQPixmap( const rw::Bitmap& rasterBitmap )
{
	return QPixmap::fromImage(
        convertRWBitmapToQImage( rasterBitmap )
    );
}