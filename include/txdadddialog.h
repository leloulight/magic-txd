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
public:
    enum eCreationType
    {
        CREATE_IMGPATH,
        CREATE_RASTER
    };

    struct dialogCreateParams
    {
        inline dialogCreateParams(void)
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

    typedef std::function <void(const texAddOperation&)> operationCallback_t;

    TexAddDialog(MainWindow *mainWnd, const dialogCreateParams& create_params, operationCallback_t func);
    ~TexAddDialog(void);

    void loadPlatformOriginal(void);
    void updatePreviewWidget(void);

    void createRasterForConfiguration(void);

    static QComboBox* createPlatformSelectComboBox(MainWindow *mainWnd);

private:
    void UpdatePreview();
    void ClearPreview();

    void releaseConvRaster(void);

    inline rw::Raster* GetDisplayRaster(void)
    {
        if (rw::Raster *convRaster = this->convRaster)
        {
            return convRaster;
        }

        if (this->hasPlatformOriginal)
        {
            if (rw::Raster *origRaster = this->platformOrigRaster)
            {
                return origRaster;
            }
        }

        return NULL;
    }

    void UpdateAccessability(void);

    QString GetCurrentPlatform(void);

public slots:
    void OnTextureAddRequest(bool checked);
    void OnCloseRequest(bool checked);

    void OnPlatformSelect(const QString& newText);

    void OnPlatformFormatTypeToggle(bool checked);

    void OnTextureCompressionSeelct(const QString& newCompression);
    void OnTexturePaletteTypeSelect(const QString& newPaletteType);
    void OnTexturePixelFormatSelect(const QString& newPixelFormat);

    void OnPreviewBackgroundStateChanged(int state);
    void OnScalePreviewStateChanged(int state);
    void OnFillPreviewStateChanged(int state);

    void beginSystemEvent(QEvent *evt)
    {
        if (evt->type() == QEvent::Resize)
        {
            if (evt->spontaneous())
            {
                this->isSystemResize = true;
            }
        }
    }

    void endSystemEvent(QEvent *evt)
    {
        if (isSystemResize)
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

    bool wantsGoodPlatformSetting;

    QLineEdit *textureNameEdit;
    QLineEdit *textureMaskNameEdit;
    QWidget *platformSelectWidget;

    QFormLayout *platformPropForm;
    QLabel *platformHeaderLabel;
    QWidget *platformRawRasterProp;
    QComboBox *platformCompressionSelectProp;
    QComboBox *platformPaletteSelectProp;
    QComboBox *platformPixelFormatSelectProp;

    bool enableOriginal;
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
    QLabel *previewLabel;
    QScrollArea *previewScrollArea;
    QCheckBox *scaledPreviewCheckBox;
    QCheckBox *fillPreviewCheckBox;
    QLabel *previewInfoLabel;

    // The buttons.
    QPushButton *cancelButton;
    QPushButton *applyButton;

    operationCallback_t cb;

    QString imgPath;
};