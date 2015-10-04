#include "mainwindow.h"
#include <QMovie>

AboutDialog::AboutDialog( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    setWindowTitle( "About us" );
    setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    setAttribute( Qt::WA_DeleteOnClose );

    // We should display all kinds of copyright information here.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    this->setFixedWidth( 580 );

    QVBoxLayout *headerGroup = new QVBoxLayout();

    QLabel *mainLogoLabel = new QLabel();
    mainLogoLabel->setAlignment( Qt::AlignCenter );

    QMovie *stars = new QMovie();

    if(mainWnd->actionThemeDark->isChecked())
        stars->setFileName(mainWnd->makeAppPath("resources\\dark\\stars2.gif"));
    else
        stars->setFileName(mainWnd->makeAppPath("resources\\light\\stars2.gif"));
    mainLogoLabel->setMovie(stars);
    stars->start();
    
    headerGroup->addWidget( mainLogoLabel );

    QLabel *labelCopyrightHolders = new QLabel( "created by DK22Pac and The_GTA" );

    labelCopyrightHolders->setObjectName( "labelCopyrightHolders" );

    labelCopyrightHolders->setAlignment( Qt::AlignCenter );

    headerGroup->addWidget( labelCopyrightHolders );

    //headerGroup->setContentsMargins( 0, 0, 0, 20 );

    rootLayout->addLayout( headerGroup );

    QLabel *labelDoNotUseCommercially = new QLabel();
    labelDoNotUseCommercially->setWordWrap(true);
    labelDoNotUseCommercially->setText(
            "This tool MUST NOT be used commercially without the explicit, written "
            "authorization of all third-parties that contributed to this tool. See the "
            "licenses for more information.\n\n" \
            "The Magic.TXD team welcomes your support!"
        );

    labelDoNotUseCommercially->setAlignment(Qt::AlignCenter);

    rootLayout->addSpacing(10);

    rootLayout->addWidget( labelDoNotUseCommercially );

    rootLayout->addSpacing(30);

    // Add icons of important vendors.
    QHBoxLayout *vendorRowOne = new QHBoxLayout();

    vendorRowOne->setAlignment( Qt::AlignCenter );

    vendorRowOne->setSpacing(10);

    QLabel *rwLogo = new QLabel();
    rwLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/renderwarelogo.png")));
    rwLogo->setToolTip("RenderWare Graphics");

    vendorRowOne->addWidget(rwLogo);

    QLabel *amdLogo = new QLabel();
    amdLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/amdlogo.png")));
    amdLogo->setToolTip("AMD");

    vendorRowOne->addWidget(amdLogo);

    QLabel *powervrLogo = new QLabel();
    powervrLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/powervrlogo.png")));
    powervrLogo->setToolTip("PowerVR");

    vendorRowOne->addWidget(powervrLogo);

    rootLayout->addLayout(vendorRowOne);

    QHBoxLayout *vendorRowTwo = new QHBoxLayout();

    vendorRowTwo->setAlignment( Qt::AlignCenter );

    vendorRowTwo->setSpacing(10);

    QLabel *xboxLogo = new QLabel();
    xboxLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/xboxlogo.png")));
    xboxLogo->setToolTip("XBOX");

    vendorRowTwo->addWidget(xboxLogo);

    QLabel *pngquantLogo = new QLabel();
    pngquantLogo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/pngquantlogo.png")));
    pngquantLogo->setToolTip("pngquant");

    vendorRowTwo->addWidget(pngquantLogo);

    QLabel *libsquishLogo = new QLabel();
    libsquishLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/squishlogo.png" ) ) );
    libsquishLogo->setToolTip("libsquish");

    vendorRowTwo->addWidget(libsquishLogo);

    QLabel *ps2Logo = new QLabel();
    ps2Logo->setPixmap(QPixmap(mainWnd->makeAppPath("resources/about/ps2logo.png")));
    ps2Logo->setToolTip("Playstation 2");

    vendorRowTwo->addWidget(ps2Logo);

    QLabel *qtLogo = new QLabel();
    qtLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/qtlogo.png" ) ) );
    qtLogo->setToolTip("Qt");

    vendorRowTwo->addWidget(qtLogo);

    rootLayout->addLayout( vendorRowTwo );

    rootLayout->addSpacing(10);

    this->setLayout( rootLayout );

    // There can be only one.
    mainWnd->aboutDlg = this;
}

AboutDialog::~AboutDialog( void )
{
    mainWnd->aboutDlg = NULL;
}