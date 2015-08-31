#pragma once

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QCheckBox>
#include <QPainter>

#include <functional>

class MainWindow;

class TexAddDialog : public QDialog
{
    struct pixelPreviewWidget : public QWidget
    {
        inline pixelPreviewWidget( TexAddDialog *owner )
        {
            this->setContentsMargins( 0, 0, 0, 0 );

            this->owner = owner;
        }

        void paintEvent( QPaintEvent *evt ) override
        {
            QPainter thePainter( this );

            thePainter.drawPixmap( 0, 0, this->width(), this->height(), owner->pixelsToAdd );

            QWidget::paintEvent( evt );
        }

        QSize sizeHint( void ) const override
        {
            // Calculate the size we should have.
            double aspectRatio = (double)recommendedWidth / recommendedHeight;

            // Get the width we can scale down to safely.
            double safeScaledWidth = aspectRatio * this->requiredHeight;

            int actualWidth = (int)floor( safeScaledWidth );

            return QSize( actualWidth, 0 );
        }

        TexAddDialog *owner;

        int recommendedWidth;
        int recommendedHeight;

        int requiredHeight;
    };

public:
    struct texAddOperation
    {
        QString imgPath;

        // Selected texture properties.
        std::string texName;
        std::string maskName;

        rw::eRasterFormat rasterFormat;
        rw::ePaletteType paletteType;
        rw::eCompressionType compressionType;

        bool generateMipmaps;
    };

    typedef std::function <void (const texAddOperation&)> operationCallback_t;

    TexAddDialog( MainWindow *mainWnd, QString pathToImage, operationCallback_t func );

private:
    void UpdateAccessability( void );

public slots:
    void OnTextureAddRequest( bool checked );
    void OnCloseRequest( bool checked );

    void OnPlatformSelect( const QString& newText );

    void OnPlatformFormatTypeToggle( bool checked );

private:
    MainWindow *mainWnd;

    QPixmap pixelsToAdd;

    QLineEdit *textureNameEdit;
    QLineEdit *textureMaskNameEdit;

    QGroupBox *platformPropsGroup;
    QFormLayout *platformPropForm;
    QWidget *platformRawRasterProp;
    QComboBox *platformCompressionSelectProp;
    QComboBox *platformPaletteSelectProp;
    QComboBox *platformPixelFormatSelectProp;

    QRadioButton *platformRawRasterToggle;
    QRadioButton *platformCompressionToggle;
    QRadioButton *platformPaletteToggle;

    // General properties.
    QCheckBox *propGenerateMipmaps;

    operationCallback_t cb;

    QString imgPath;
};