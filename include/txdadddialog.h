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

class TexAddDialog : public QDialog
{
    enum eImportExpectation
    {
        IMPORTE_IMAGE,
        IMPORTE_TEXCHUNK
    };

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

        QString actionDesc;
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
        enum eAdditionType
        {
            ADD_RASTER,
            ADD_TEXCHUNK
        };
        eAdditionType add_type;

        struct
        {
            // Selected texture properties.
            std::string texName;
            std::string maskName;

            rw::Raster *raster;
        } add_raster;
        struct
        {
            rw::TextureBase *texHandle;
        } add_texture;
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

    void SetCurrentPlatform( QString name );
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

private:
    MainWindow *mainWnd;

    bool isConstructing;        // we allow late-initialization of properties.

    eCreationType dialog_type;
    eImportExpectation img_exp;

    rw::Raster *platformOrigRaster;
    rw::TextureBase *texHandle;     // if not NULL, then this texture will be used for import.
    rw::Raster *convRaster;
    bool hasPlatformOriginal;
    QPixmap pixelsToAdd;

    bool hasConfidentPlatform;
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
    QCheckBox *backgroundForPreviewCheckBox;
    QLabel *previewInfoLabel;

    // The buttons.
    QPushButton *cancelButton;
    QPushButton *applyButton;

    operationCallback_t cb;

    QString imgPath;

    // Image import method manager.
    struct imageImportMethods
    {
        inline imageImportMethods( TexAddDialog *wnd )
        {
            this->dialog = wnd;
        }

        bool LoadPlatformOriginal( rw::Stream *stream ) const;

        typedef bool (TexAddDialog::* importMethod_t)( rw::Stream *stream );

        void RegisterImportMethod( const char *name, importMethod_t meth, eImportExpectation expImp );

        struct meth_reg
        {
            eImportExpectation img_exp;
            importMethod_t cb;
            const char *name;
        };

        TexAddDialog *dialog;

        std::vector <meth_reg> methods;
    };

    imageImportMethods impMeth;

    void clearTextureOriginal( void );

    rw::Raster* MakeRaster( void );

    // The methods use this function to set the platform original raster link.
    void setPlatformOrigRaster( rw::Raster *raster );

    // The methods to be used by imageImportMethods.
    bool impMeth_loadImage( rw::Stream *stream );
    bool impMeth_loadTexChunk( rw::Stream *stream );
};