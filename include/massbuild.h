#pragma once

#include <QDialog>

struct MassBuildWindow : public QDialog
{
private:
    friend struct massbuildEnv;

public:
    MassBuildWindow( MainWindow *mainWnd );
    ~MassBuildWindow( void );

public slots:
    void OnRequestBuild( bool checked );
    void OnRequestCancel( bool checked );

private:
    void serialize( void );

    MainWindow *mainWnd;

    QLineEdit *editGameRoot;
    QLineEdit *editOutputRoot;
    QComboBox *selPlatformBox;
    QComboBox *selGameBox;
    QCheckBox *propGenMipmaps;
    QLineEdit *propGenMipmapsMax;
    QCheckBox *propCompressTextures;

    RwListEntry <MassBuildWindow> node;
};