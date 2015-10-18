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

struct MassConvertWindow;

struct MassConvertWindow : public QDialog
{
    friend struct massconvEnv;

public:
    MassConvertWindow( MainWindow *mainwnd );
    ~MassConvertWindow();

    void customEvent( QEvent *evt ) override;

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

public:
    volatile rw::thread_t conversionThread;

    rw::rwlock *volatile convConsistencyLock;

    RwListEntry <MassConvertWindow> node;
};