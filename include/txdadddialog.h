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
#include <QResizeEvent>

#include <functional>

class MainWindow;

class TexAddDialog : public QDialog, public SystemEventHandlerWidget
{
    struct pixelPreviewWidget : public QWidget
    {
        inline pixelPreviewWidget( TexAddDialog *owner, QWidget *parent = NULL ) : QWidget( parent )
        {
            this->setContentsMargins( 0, 0, 0, 0 );

            this->owner = owner;

            this->wantsRasterUpdate = false;
            this->hasSizeProperties = false;

            this->requiredHeight = -1;

            this->hasWidthChanged = false;
        }

        void paintEvent( QPaintEvent *evt ) override;

        QSize sizeHint( void ) const override
        {
            // Take margin into account.
            QMargins marginDimm = this->parentWidget()->contentsMargins();

            QMargins layMarginDimm = this->parentWidget()->layout()->contentsMargins();

            int actualMarginHeight = ( marginDimm.bottom() + marginDimm.top() + layMarginDimm.bottom() + layMarginDimm.top() );

            // Calculate the size we should have.
            double aspectRatio = (double)recommendedWidth / recommendedHeight;

            // Get the width we can scale down to safely.
            double safeScaledWidth = aspectRatio * ( this->requiredHeight - actualMarginHeight );

            int actualWidth = (int)floor( safeScaledWidth );

            return QSize( actualWidth, -1 );
        }

        void resizeEvent( QResizeEvent *evt ) override
        {
            if ( owner->isSystemResize )
            {
                const QSize& oldSize = evt->oldSize();
                const QSize& newSize = evt->size();

                bool hasWidthChanged = ( oldSize.width() != newSize.width() );

                if ( hasWidthChanged )
                {
                    this->hasWidthChanged = true;
                }
            }

            QWidget::resizeEvent( evt );
        }

        void fixWidth( void )
        {
            // If the user has already changed the width of the GUI,
            // we do not adjust it anymore.
            if ( this->hasWidthChanged )
                return;

            // Fixing the width means we resize it to its recommended size.
            // This will improve it's scale.
            this->requiredHeight = owner->propertiesWidget->sizeHint().height();

            this->updateGeometry();

            owner->previewGroupWidget->updateGeometry();

            QSize preferredNewSize = owner->layout()->sizeHint();

            owner->resize( preferredNewSize );
        }

        void update( void )
        {
            QWidget::update();

            this->wantsRasterUpdate = true;
        }

        TexAddDialog *owner;

        int recommendedWidth;
        int recommendedHeight;
        int requiredHeight;

        bool hasWidthChanged;

        bool hasSizeProperties;

        bool wantsRasterUpdate;
    };

public:
    enum eCreationType
    {
        CREATE_IMGPATH,
        CREATE_RASTER
    };

    struct dialogCreateParams
    {
        inline dialogCreateParams( void )
        {
            this->overwriteTexName = NULL;
        }

        QString actionName;
        eCreationType type;
        struct
        {
            QString imgPath;
        } img_path;
        struct
        {
            rw::TextureBase *tex;
        } orig_raster;

        // Optional properties.
        const QString *overwriteTexName;
    };

    struct texAddOperation
    {
        // Selected texture properties.
        std::string texName;
        std::string maskName;

        rw::Raster *raster;
    };

    typedef std::function <void (const texAddOperation&)> operationCallback_t;

    TexAddDialog( MainWindow *mainWnd, const dialogCreateParams& create_params, operationCallback_t func );
    ~TexAddDialog( void );

    void loadPlatformOriginal( void );
    void updatePreviewWidget( void );

    void createRasterForConfiguration( void );

private:
    void releaseConvRaster( void );
    
    inline rw::Raster* GetDisplayRaster( void )
    {
        if ( rw::Raster *convRaster = this->convRaster )
        {
            return convRaster;
        }

        if ( this->hasPlatformOriginal )
        {
            if ( rw::Raster *origRaster = this->platformOrigRaster )
            {
                return origRaster;
            }
        }

        return NULL;
    }

    void UpdateAccessability( void );

    QString GetCurrentPlatform( void );

public slots:
    void OnTextureAddRequest( bool checked );
    void OnCloseRequest( bool checked );

    void OnPlatformSelect( const QString& newText );

    void OnPlatformFormatTypeToggle( bool checked );

    void OnTextureCompressionSeelct( const QString& newCompression );
    void OnTexturePaletteTypeSelect( const QString& newPaletteType );
    void OnTexturePixelFormatSelect( const QString& newPixelFormat );

    void beginSystemEvent( QEvent *evt )
    {
        if ( evt->type() == QEvent::Resize )
        {
            if ( evt->spontaneous() )
            {
                this->isSystemResize = true;
            }
        }
    }

    void endSystemEvent( QEvent *evt )
    {
        if ( isSystemResize )
        {
            this->isSystemResize = false;
        }
    }

private:
    MainWindow *mainWnd;

    eCreationType dialog_type;

    rw::Raster *platformOrigRaster;
    rw::Raster *convRaster;
    bool hasPlatformOriginal;
    QPixmap pixelsToAdd;
    QWidget *propertiesWidget;

    bool wantsGoodPlatformSetting;

    QLineEdit *textureNameEdit;
    QLineEdit *textureMaskNameEdit;
    QWidget *platformSelectWidget;

    QGroupBox *platformPropsGroup;
    QFormLayout *platformPropForm;
    QWidget *platformRawRasterProp;
    QComboBox *platformCompressionSelectProp;
    QComboBox *platformPaletteSelectProp;
    QComboBox *platformPixelFormatSelectProp;

    bool enableRawRaster;
    bool enableCompressSelect;
    bool enablePaletteSelect;
    bool enablePixelFormatSelect;

    bool isSystemResize;

    QRadioButton *platformOriginalToggle;
    QRadioButton *platformRawRasterToggle;
    QRadioButton *platformCompressionToggle;
    QRadioButton *platformPaletteToggle;

    // General properties.
    QCheckBox *propGenerateMipmaps;

    // Preview widget stuff.
    QWidget *previewGroupWidget;

    // The buttons.
    QPushButton *cancelButton;
    QPushButton *applyButton;

    pixelPreviewWidget *previewWidget;

    operationCallback_t cb;

    QString imgPath;
};