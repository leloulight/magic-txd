#include "mainwindow.h"

AboutDialog::AboutDialog( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    setWindowTitle( "About us" );
    setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    setAttribute( Qt::WA_DeleteOnClose );

    // We should display all kinds of copyright information here.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    this->setMinimumWidth( 350 );

    rootLayout->setSizeConstraint( QLayout::SetFixedSize );

    QVBoxLayout *headerGroup = new QVBoxLayout();

    QLabel *mainLogoLabel = new QLabel();
    mainLogoLabel->setAlignment( Qt::AlignCenter );

    mainLogoLabel->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/magictxd.png" ) ).scaled( QSize( 200, 200 ) ) );

    headerGroup->addWidget( mainLogoLabel );

    QLabel *labelCopyrightHolders = new QLabel( "created by DK22Pac and The_GTA" );

    labelCopyrightHolders->setObjectName( "labelCopyrightHolders" );

    labelCopyrightHolders->setAlignment( Qt::AlignCenter );

    headerGroup->addWidget( labelCopyrightHolders );

    headerGroup->setContentsMargins( 0, 0, 0, 20 );

    rootLayout->addLayout( headerGroup );

    QLabel *labelDoNotUseCommercially =
        new QLabel(
            "This tool MUST NOT be used commercially without the explicit, written\n"
            "authorization of all third-parties that contributed to this tool. See the\n"
            "licenses for more information.\n\n" \
            "The Magic.TXD team welcomes your support!"
        );

    labelDoNotUseCommercially->setContentsMargins( 0, 0, 0, 15 );

    labelDoNotUseCommercially->setObjectName( "labelDoNotUseCommercially" );

    rootLayout->addWidget( labelDoNotUseCommercially );

    // Add icons of important vendors.
    QHBoxLayout *vendorRowOne = new QHBoxLayout();

    const int rowHeight = 60;

    QLabel *ps2Logo = new QLabel();
    ps2Logo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/ps2logo.png" ) ).scaled( 150, rowHeight ) );

    vendorRowOne->addWidget( ps2Logo );

    QLabel *xboxLogo = new QLabel();
    xboxLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/xboxlogo.png" ) ).scaled( rowHeight, rowHeight ) );
    
    vendorRowOne->addWidget( xboxLogo );

    QLabel *amdLogo = new QLabel();
    amdLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/amdlogo.png" ) ).scaled( rowHeight, rowHeight ) );

    vendorRowOne->addWidget( amdLogo );

    QLabel *powervrLogo = new QLabel();
    powervrLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/powervrlogo.png" ) ).scaled( 150, rowHeight ) );

    vendorRowOne->addWidget( powervrLogo );

    QLabel *pngquantLogo = new QLabel();
    pngquantLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/pngquantlogo.png" ) ).scaled( rowHeight, rowHeight ) );

    vendorRowOne->addWidget( pngquantLogo );

    rootLayout->addLayout( vendorRowOne );
    
    QHBoxLayout *vendorRowTwo = new QHBoxLayout();

    vendorRowTwo->setAlignment( Qt::AlignCenter );

    QLabel *libsquishLogo = new QLabel();
    libsquishLogo->setPixmap( QPixmap( mainWnd->makeAppPath( "resources/about/squishlogo.png" ) ).scaled( rowHeight * 3, rowHeight ) );

    vendorRowTwo->addWidget( libsquishLogo );

    rootLayout->addLayout( vendorRowTwo );

    this->setLayout( rootLayout );

    // There can be only one.
    mainWnd->aboutDlg = this;
}

AboutDialog::~AboutDialog( void )
{
    mainWnd->aboutDlg = NULL;
}