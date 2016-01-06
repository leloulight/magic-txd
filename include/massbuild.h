#pragma once

#include <QDialog>
#include "languages.h"

struct MassBuildWindow : public QDialog, public magicTextLocalizationItem
{
private:
    friend struct massbuildEnv;

public:
    MassBuildWindow( MainWindow *mainWnd );
    ~MassBuildWindow( void );

    void updateContent( MainWindow *mainWnd ) override;

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