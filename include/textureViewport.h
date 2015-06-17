#pragma once

#include <QPainter>

class TexViewportWidget : public QWidget
{
public:
    inline TexViewportWidget( void )
    {
        this->rwTextureHandle = NULL;
    }

    inline void setTextureHandle( rw::TextureBase *texHandle )
    {
        this->rwTextureHandle = texHandle;

        this->update();
    }

protected:
    void paintEvent( QPaintEvent *evtHandle ) override
    {
        QPainter painter( this );

        // Draw a background grid first.
        {
            int curWidth = this->width();
            int curHeight = this->height();

            int x = 0;
            int y = 0;

            int x_iter = 0;
            int y_iter = 0;

            const int quadDimm = 10;

            while ( true )
            {
                bool isBlack = ( ( x_iter % 2 ) == 0 ) ^ ( ( y_iter % 2 ) == 0 );

                int red = ( isBlack ? 120 : 255 );
                int green = ( isBlack ? 120 : 255 );
                int blue = ( isBlack ? 120 : 255 );

                QRect drawRect( x, y, quadDimm, quadDimm );

                painter.fillRect( drawRect, QColor( red, green, blue ) );

                x += quadDimm;
                x_iter++;

                if ( x >= curWidth )
                {
                    x = 0;
                    x_iter = 0;

                    y += quadDimm;
                    y_iter++;
                }

                if ( y >= curHeight )
                {
                    break;
                }
            }
        }

        // Now draw the texture on it, if we have one.
        if ( rw::TextureBase *texBase = this->rwTextureHandle )
        {
            rw::Raster *rasterData = texBase->GetRaster();

            if ( rasterData )
            {
                // Get a bitmap to the raster.
                // This is a 2D color component surface.
                rw::Bitmap rasterBitmap = rasterData->getBitmap();

                rw::uint32 width, height;
                rasterBitmap.getSize( width, height );

                QImage texImage( width, height, QImage::Format::Format_ARGB32 );

                // Copy scanline by scanline.
                for ( int y = 0; y < height; y++ )
                {
                    uchar *scanLineContent = texImage.scanLine( y );

                    QRgb *colorItems = (QRgb*)scanLineContent;

                    for ( int x = 0; x < width; x++ )
                    {
                        QRgb *colorItem = ( colorItems + x );

                        unsigned char r, g, b, a;

                        rasterBitmap.browsecolor( x, y, r, g, b, a );

                        *colorItem = qRgba( r, g, b, a );
                    }
                }

                // Draw this image.
                painter.drawImage( 0, 0, texImage );
            }
        }
    }

private:
    rw::TextureBase *rwTextureHandle;
};