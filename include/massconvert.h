#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPlainTextEdit>

#include <sdk/PluginHelpers.h>

#include "../src/tools/txdgen.h"

struct massconvEnv
{
    inline void Initialize( MainWindow *mainwnd )
    {
        return;
    }

    inline void Shutdown( MainWindow *mainwnd )
    {
        return;
    }

    TxdGenModule::run_config txdgenConfig;
};

typedef PluginDependantStructRegister <massconvEnv, mainWindowFactory_t> massconvEnvRegister_t;

extern massconvEnvRegister_t massconvEnvRegister;

struct MassConvertWindow : public QDialog
{
private:
    struct PathBrowseButton : public QPushButton
    {
        inline PathBrowseButton( QString text, QLineEdit *lineEdit ) : QPushButton( text )
        {
            this->theEdit = lineEdit;

            connect( this, &QPushButton::clicked, this, &PathBrowseButton::OnBrowsePath );
        }

    public slots:
        void OnBrowsePath( bool checked )
        {
            filePath curPathAbs;

            bool hasPath = fileRoot->GetFullPath( "/", false, curPathAbs );

            QString selPath =
                QFileDialog::getExistingDirectory(
                    this, "Browse Directory...",
                    hasPath ? QString::fromStdWString( curPathAbs.convert_unicode() ) : QString()
                );

            if ( selPath.isEmpty() == false )
            {
                this->theEdit->setText( selPath );
            }
        }

    private:
        QLineEdit *theEdit;
    };

    inline QLayout* createPathSelectGroup( QString begStr, QLineEdit*& editOut )
    {
        QHBoxLayout *pathRow = new QHBoxLayout();

        QLineEdit *pathEdit = new QLineEdit( begStr );

        pathRow->addWidget( pathEdit );

        QPushButton *pathBrowseButton = new PathBrowseButton( "Browse", pathEdit );

        pathRow->addWidget( pathBrowseButton );

        editOut = pathEdit;

        return pathRow;
    }

public:
    MassConvertWindow( MainWindow *mainwnd );
    ~MassConvertWindow();

    bool event( QEvent *evt ) override;

    void postLogMessage( QString msg );

    MainWindow *mainwnd;

public slots:
    void OnRequestConvert( bool checked );
    void OnRequestCancel( bool checked );

private:
    void serialize( void );

    // Widget pointers.
    QLineEdit *editGameRoot;
    QLineEdit *editOutputRoot;
    QComboBox *selPlatformBox;
    QComboBox *selGameBox;
    QCheckBox *propClearMipmaps;
    QCheckBox *propGenMipmaps;
    QLineEdit *propGenMipmapsMax;
    QCheckBox *propImproveFiltering;
    QCheckBox *propCompressTextures;
    QCheckBox *propReconstructIMG;
    QCheckBox *propCompressedIMG;

    QPlainTextEdit *logEdit;
    QPushButton *buttonConvert;

    // Configuration of the engine we should remember.
    struct
    {
        rw::WarningManagerInterface *warningManager;
        rw::ePaletteRuntimeType palRuntimeType;
        rw::eDXTCompressionMethod dxtRuntimeType;
        bool fixIncompatibleRasters;
        bool dxtPackedDecompression;
        bool ignoreSerializationRegions;
        int warningLevel;
        bool ignoreSecureWarnings;
    } rwconf;

public:
    rw::thread_t conversionThread;

    rw::rwlock *convConsistencyLock;

    bool terminate;
};