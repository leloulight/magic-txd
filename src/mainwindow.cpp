#include "mainwindow.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include "texinfoitem.h"
#include <QCommonStyle>
#include <QMenu>
#include <QMenuBar>
#include <QHBoxLayout>
#include <qsplitter.h>
#include <qmovie.h>
#include <QFileDialog>
#include <QDir>
#include <QDesktopServices>

#include "styles.h"
#include "textureViewport.h"
#include "rwversiondialog.h"
#include "texnamewindow.h"
#include "renderpropwindow.h"
#include "resizewindow.h"
#include "platformselwindow.h"
#include "massconvert.h"
#include "exportallwindow.h"
#include "massexport.h"
#include "optionsdialog.h"

#include "tools/txdgen.h"

#include "qtrwutils.hxx"

MainWindow::MainWindow(QString appPath, rw::Interface *engineInterface, CFileSystem *fsHandle, QWidget *parent) :
    QMainWindow(parent),
    rwWarnMan( this )
{
    m_appPath = appPath;
    m_appPathForStyleSheet = appPath;
    m_appPathForStyleSheet.replace('\\', '/');
    // Initialize variables.
    this->currentTXD = NULL;
    this->txdNameLabel = NULL;
    this->currentSelectedTexture = NULL;
    this->txdLog = NULL;
    this->verDlg = NULL;
    this->texNameDlg = NULL;
    this->renderPropDlg = NULL;
    this->resizeDlg = NULL;
    this->platformDlg = NULL;
    this->aboutDlg = NULL;
    this->optionsDlg = NULL;
    this->rwVersionButton = NULL;
    this->recheckingThemeItem = false;

    this->areFriendlyIconsVisible = false;
    this->shouldShowFriendlyIcons = true;

    // Initialize configuration to default.
    {
        this->lastTXDOpenDir = QDir::current().absolutePath();
        this->lastTXDSaveDir = this->lastTXDOpenDir;
        this->lastImageFileOpenDir = this->makeAppPath( "" );
        this->addImageGenMipmaps = true;
        this->lockDownTXDPlatform = true;
        this->adjustTextureChunksOnImport = true;
        this->showLogOnWarning = true;
        this->lastUsedAllExportFormat = "PNG";
        this->lastAllExportTarget = this->makeAppPath( "" ).toStdWString();
    }

    this->drawMipmapLayers = false;
	this->showBackground = false;

    this->hasOpenedTXDFileInfo = false;

    this->rwEngine = engineInterface;

    // Set-up the warning manager.
    this->rwEngine->SetWarningManager( &this->rwWarnMan );

    this->fileSystem = fsHandle;

    try
    {
	    /* --- Window --- */
        updateWindowTitle();
        setMinimumSize(560, 300);
	    resize(900, 680);

	    /* --- Log --- */
	    this->txdLog = new TxdLog(this, this->m_appPath, this);

	    /* --- List --- */
	    QListWidget *listWidget = new QListWidget();
	    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		//listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        listWidget->setMaximumWidth(350);
	    //listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

        connect( listWidget, &QListWidget::currentItemChanged, this, &MainWindow::onTextureItemChanged );

        // We will store all our texture names in this.
        this->textureListWidget = listWidget;

	    /* --- Viewport --- */
		imageView = new QScrollArea;
		imageView->setFrameShape(QFrame::NoFrame);
		imageView->setObjectName("textureViewBackground");
		imageWidget = new QLabel;
		//imageWidget->setObjectName("transparentBackground"); // "chessBackground" > grid background
		imageWidget->setStyleSheet("background-color: rgba(255, 255, 255, 0);");
		imageView->setWidget(imageWidget);
		imageView->setAlignment(Qt::AlignCenter);

	    /* --- Splitter --- */
        mainSplitter = new QSplitter;
	    mainSplitter->addWidget(listWidget);
		mainSplitter->addWidget(imageView);
	    QList<int> sizes;
	    sizes.push_back(200);
	    sizes.push_back(mainSplitter->size().width() - 200);
        mainSplitter->setSizes(sizes);
        mainSplitter->setChildrenCollapsible(false);

	    /* --- Top panel --- */
	    QWidget *txdNameBackground = new QWidget();
	    txdNameBackground->setFixedHeight(60);
	    txdNameBackground->setObjectName("background_0");
	    QLabel *txdName = new QLabel();
	    txdName->setObjectName("label36px");
	    txdName->setAlignment(Qt::AlignCenter);

        this->txdNameLabel = txdName;

        QGridLayout *txdNameLayout = new QGridLayout();
        QLabel *starsBox = new QLabel;
        starsMovie = new QMovie;

        // set default theme movie
        starsMovie->setFileName(makeAppPath("resources\\dark\\stars.gif"));
        starsBox->setMovie(starsMovie);
        starsMovie->start();
        txdNameLayout->addWidget(starsBox, 0, 0);
        txdNameLayout->addWidget(txdName, 0, 0);
        txdNameLayout->setContentsMargins(0, 0, 0, 0);
        txdNameLayout->setMargin(0);
        txdNameLayout->setSpacing(0);
        txdNameBackground->setLayout(txdNameLayout);
	
	    QWidget *txdOptionsBackground = new QWidget();
	    txdOptionsBackground->setFixedHeight(54);
	    txdOptionsBackground->setObjectName("background_1");

	    /* --- Menu --- */
	    QMenuBar *menu = new QMenuBar;
	    QMenu *fileMenu = menu->addMenu(tr("&File"));
        QAction *actionNew = new QAction("&New", this);
        fileMenu->addAction(actionNew);

        this->actionNewTXD = actionNew;
        
        connect( actionNew, &QAction::triggered, this, &MainWindow::onCreateNewTXD );

	    QAction *actionOpen = new QAction("&Open", this);
	    fileMenu->addAction(actionOpen);

        this->actionOpenTXD = actionOpen;

        connect( actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile );

	    QAction *actionSave = new QAction("&Save", this);
        actionSave->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_S ) );
	    fileMenu->addAction(actionSave);

        this->actionSaveTXD = actionSave;

        connect( actionSave, &QAction::triggered, this, &MainWindow::onRequestSaveTXD );

	    QAction *actionSaveAs = new QAction("&Save as...", this);
	    fileMenu->addAction(actionSaveAs);

        this->actionSaveTXDAs = actionSaveAs;

        connect( actionSaveAs, &QAction::triggered, this, &MainWindow::onRequestSaveAsTXD );

	    QAction *closeCurrent = new QAction("&Close current", this);
	    fileMenu->addAction(closeCurrent);
	    fileMenu->addSeparator();

        this->actionCloseTXD = closeCurrent;

        connect( closeCurrent, &QAction::triggered, this, &MainWindow::onCloseCurrent );

	    QAction *actionQuit = new QAction("&Quit", this);
	    fileMenu->addAction(actionQuit);

	    QMenu *editMenu = menu->addMenu(tr("&Edit"));
	    QAction *actionAdd = new QAction("&Add", this);
        actionAdd->setShortcut( Qt::Key_Insert );
	    editMenu->addAction(actionAdd);

        this->actionAddTexture = actionAdd;

        connect( actionAdd, &QAction::triggered, this, &MainWindow::onAddTexture );

	    QAction *actionReplace = new QAction("&Replace", this);
        actionReplace->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_R ) );
	    editMenu->addAction(actionReplace);

        this->actionReplaceTexture = actionReplace;

        connect( actionReplace, &QAction::triggered, this, &MainWindow::onReplaceTexture );

	    QAction *actionRemove = new QAction("&Remove", this);
        actionRemove->setShortcut( Qt::Key_Delete );
	    editMenu->addAction(actionRemove);

        this->actionRemoveTexture = actionRemove;

        connect( actionRemove, &QAction::triggered, this, &MainWindow::onRemoveTexture );

	    QAction *actionRename = new QAction("&Rename", this);
        actionRename->setShortcut( Qt::Key_N );
	    editMenu->addAction(actionRename);

        this->actionRenameTexture = actionRename;

        connect( actionRename, &QAction::triggered, this, &MainWindow::onRenameTexture );

	    QAction *actionResize = new QAction("&Resize", this);
	    editMenu->addAction(actionResize);

        this->actionResizeTexture = actionResize;

        connect( actionResize, &QAction::triggered, this, &MainWindow::onResizeTexture );

        QAction *actionManipulate = new QAction("&Manipulate", this);
        actionManipulate->setShortcut( Qt::Key_M );
        editMenu->addAction(actionManipulate);

        this->actionManipulateTexture = actionManipulate;

        connect( actionManipulate, &QAction::triggered, this, &MainWindow::onManipulateTexture );

	    QAction *actionSetPixelFormat = new QAction("&Platform", this);
        actionSetPixelFormat->setShortcut( Qt::Key_P );
	    editMenu->addAction(actionSetPixelFormat);

        this->actionPlatformSelect = actionSetPixelFormat;

        connect( actionSetPixelFormat, &QAction::triggered, this, &MainWindow::onSelectPlatform );

	    QAction *actionSetupMipLevels = new QAction("&Setup mip-levels", this);
        actionSetupMipLevels->setShortcut( Qt::CTRL | Qt::Key_M );
	    editMenu->addAction(actionSetupMipLevels);

        this->actionSetupMipmaps = actionSetupMipLevels;

        connect( actionSetupMipLevels, &QAction::triggered, this, &MainWindow::onSetupMipmapLayers );

        QAction *actionClearMipLevels = new QAction("&Clear mip-levels", this);
        actionClearMipLevels->setShortcut( Qt::CTRL | Qt::Key_C );
        editMenu->addAction(actionClearMipLevels);

        this->actionClearMipmaps = actionClearMipLevels;

        connect( actionClearMipLevels, &QAction::triggered, this, &MainWindow::onClearMipmapLayers );

	    QAction *actionSetupRenderingProperties = new QAction("&Setup rendering properties", this);
	    editMenu->addAction(actionSetupRenderingProperties);

        this->actionRenderProps = actionSetupRenderingProperties;

        connect( actionSetupRenderingProperties, &QAction::triggered, this, &MainWindow::onSetupRenderingProps );

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

	    editMenu->addSeparator();
	    QAction *actionViewAllChanges = new QAction("&View all changes", this);
	    editMenu->addAction(actionViewAllChanges);

        this->actionViewAllChanges = actionViewAllChanges;

	    QAction *actionCancelAllChanges = new QAction("&Cancel all changes", this);
	    editMenu->addAction(actionCancelAllChanges);

        this->actionCancelAllChanges = actionCancelAllChanges;

	    editMenu->addSeparator();
	    QAction *actionAllTextures = new QAction("&All textures", this);
	    editMenu->addAction(actionAllTextures);

        this->actionAllTextures = actionAllTextures;

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

	    editMenu->addSeparator();
	    QAction *actionSetupTxdVersion = new QAction("&Setup TXD version", this);
	    editMenu->addAction(actionSetupTxdVersion);

        this->actionSetupTXDVersion = actionSetupTxdVersion;

		connect(actionSetupTxdVersion, &QAction::triggered, this, &MainWindow::onSetupTxdVersion);

        editMenu->addSeparator();

        QAction *actionShowOptions = new QAction("&Options", this);
        editMenu->addAction(actionShowOptions);

        this->actionShowOptions = actionShowOptions;
        
        connect(actionShowOptions, &QAction::triggered, this, &MainWindow::onShowOptions);

        QMenu *toolsMenu = menu->addMenu(tr("&Tools"));

        QAction *actionMassConvert = new QAction("&Mass convert", this);
        toolsMenu->addAction(actionMassConvert);

        connect( actionMassConvert, &QAction::triggered, this, &MainWindow::onRequestMassConvert );

        QAction *actionMassExport = new QAction("&Mass export", this);
        toolsMenu->addAction(actionMassExport);

        connect( actionMassExport, &QAction::triggered, this, &MainWindow::onRequestMassExport );

	    QMenu *exportMenu = menu->addMenu(tr("&Export"));

        this->addTextureFormatExportLinkToMenu( exportMenu, "PNG", "PNG", "Portable Network Graphics" );
        this->addTextureFormatExportLinkToMenu( exportMenu, "RWTEX", "RWTEX", "RW Texture Chunk" );
        this->addTextureFormatExportLinkToMenu( exportMenu, "DDS", "DDS", "Direct Draw Surface" );
        this->addTextureFormatExportLinkToMenu( exportMenu, "BMP", "BMP", "Raw Bitmap" );

        // Add remaining formats that rwlib supports.
        {
            rw::registered_image_formats_t regFormats;
            
            rw::GetRegisteredImageFormats( this->rwEngine, regFormats );

            for ( rw::registered_image_formats_t::const_iterator iter = regFormats.cbegin(); iter != regFormats.cend(); iter++ )
            {
                const rw::registered_image_format& theFormat = *iter;

                rw::uint32 num_ext = theFormat.num_ext;
                const rw::imaging_filename_ext *ext_array = theFormat.ext_array;

                // Decide what the most friendly name of this format is.
                // The friendly name is the longest extension available.
                const char *displayName =
                    rw::GetLongImagingFormatExtension( num_ext, ext_array );

                const char *defaultExt = NULL;

                bool gotDefaultExt = rw::GetDefaultImagingFormatExtension( theFormat.num_ext, theFormat.ext_array, defaultExt );

                if ( gotDefaultExt && displayName != NULL )
                {
                    if ( stricmp( defaultExt, "PNG" ) != 0 &&
                         stricmp( defaultExt, "DDS" ) != 0 &&
                         stricmp( defaultExt, "BMP" ) != 0 )
                    {
                        this->addTextureFormatExportLinkToMenu( exportMenu, displayName, defaultExt, theFormat.formatName );
                    }

                    // We want to cache the available formats.
                    registered_image_format imgformat;

                    imgformat.formatName = theFormat.formatName;
                    imgformat.defaultExt = defaultExt;

                    for ( rw::uint32 n = 0; n < theFormat.num_ext; n++ )
                    {
                        imgformat.ext_array.push_back( std::string( theFormat.ext_array[ n ].ext ) );
                    }

                    imgformat.isNativeFormat = false;

                    this->reg_img_formats.push_back( std::move( imgformat ) );
                }
            }

            // Also add image formats from native texture types.
            rw::platformTypeNameList_t platformTypes = rw::GetAvailableNativeTextureTypes( this->rwEngine );

            for ( rw::platformTypeNameList_t::const_iterator iter = platformTypes.begin(); iter != platformTypes.end(); iter++ )
            {
                const std::string& nativeName = *iter;

                // Check the driver for a native name.
                const char *nativeExt = rw::GetNativeTextureImageFormatExtension( this->rwEngine, nativeName.c_str() );

                if ( nativeExt )
                {
                    registered_image_format imgformat;

                    if ( strcmp( nativeExt, "DDS" ) == 0 )
                    {
                        imgformat.formatName = "DirectDraw Surface";
                    }
                    else
                    {
                        imgformat.formatName = nativeExt;   // could improve this.
                    }
                    imgformat.defaultExt = nativeExt;
                    imgformat.isNativeFormat = true;
                    imgformat.nativeType = nativeName;

                    imgformat.ext_array.push_back( nativeExt );

                    this->reg_img_formats.push_back( std::move( imgformat ) );
                }
            }
        }

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

	    QAction *actionExportTTXD = new QAction("&Text-based TXD", this);
	    exportMenu->addAction(actionExportTTXD);

        this->actionsExportImage.push_back( actionExportTTXD );

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

	    exportMenu->addSeparator();
	    QAction *actionExportAll = new QAction("&Export all", this);
	    exportMenu->addAction(actionExportAll);

        this->exportAllImages = actionExportAll;

        connect( actionExportAll, &QAction::triggered, this, &MainWindow::onExportAllTextures );

	    QMenu *viewMenu = menu->addMenu(tr("&View"));
	    QAction *actionBackground = new QAction("&Background", this);
        actionBackground->setShortcut( Qt::Key_F5 );
		actionBackground->setCheckable(true);
	    viewMenu->addAction(actionBackground);

		connect(actionBackground, &QAction::triggered, this, &MainWindow::onToggleShowBackground);

#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE

	    QAction *action3dView = new QAction("&3D View", this);
		action3dView->setCheckable(true);
	    viewMenu->addAction(action3dView);

#endif //_FEATURES_NOT_IN_CURRENT_RELEASE

	    QAction *actionShowMipLevels = new QAction("&Display mip-levels", this);
        actionShowMipLevels->setShortcut( Qt::Key_F6 );
		actionShowMipLevels->setCheckable(true);
	    viewMenu->addAction(actionShowMipLevels);

        connect( actionShowMipLevels, &QAction::triggered, this, &MainWindow::onToggleShowMipmapLayers );
        
        QAction *actionShowLog = new QAction("&Show Log", this);
        actionShowLog->setShortcut( Qt::Key_F7 );
        viewMenu->addAction(actionShowLog);

        connect( actionShowLog, &QAction::triggered, this, &MainWindow::onToggleShowLog );

	    viewMenu->addSeparator();
	        
        this->actionThemeDark = new QAction("&Dark theme", this);
        this->actionThemeDark->setCheckable(true);
        this->actionThemeLight = new QAction("&Light theme", this);
        this->actionThemeLight->setCheckable(true);

        // enable needed theme in menu before connecting a slot
        actionThemeDark->setChecked(true);

        connect(this->actionThemeDark, &QAction::triggered, this, &MainWindow::onToogleDarkTheme);
        connect(this->actionThemeLight, &QAction::triggered, this, &MainWindow::onToogleLightTheme);

        viewMenu->addAction(this->actionThemeDark);
        viewMenu->addAction(this->actionThemeLight);

	    actionQuit->setShortcut(QKeySequence::Quit);
	    connect(actionQuit, &QAction::triggered, this, &MainWindow::close);

        QMenu *infoMenu = menu->addMenu(tr("&Info"));

        QAction *actionOpenWebsite = new QAction("&Website", this);
        infoMenu->addAction(actionOpenWebsite);

        connect( actionOpenWebsite, &QAction::triggered, this, &MainWindow::onRequestOpenWebsite );

        infoMenu->addSeparator();

        QAction *actionAbout = new QAction("&About", this);
        infoMenu->addAction(actionAbout);

        connect( actionAbout, &QAction::triggered, this, &MainWindow::onAboutUs );

	    QHBoxLayout *hlayout = new QHBoxLayout;
	    txdOptionsBackground->setLayout(hlayout);
	    hlayout->setMenuBar(menu);

        // Layout for rw version, with right-side alignment
        QHBoxLayout *rwVerLayout = new QHBoxLayout;
        rwVersionButton = new QPushButton;
        rwVersionButton->setObjectName("rwVersionButton");
        rwVersionButton->hide();
        rwVerLayout->addWidget(rwVersionButton);
        rwVerLayout->setAlignment(Qt::AlignRight);

        connect( rwVersionButton, &QPushButton::clicked, this, &MainWindow::onSetupTxdVersion );

        // Layout to mix menu and rw version label/button
        QGridLayout *menuVerLayout = new QGridLayout();
        menuVerLayout->addWidget(txdOptionsBackground, 0, 0);
        menuVerLayout->addLayout(rwVerLayout, 0, 0, Qt::AlignRight);
        menuVerLayout->setContentsMargins(0, 0, 0, 0);
        menuVerLayout->setMargin(0);
        menuVerLayout->setSpacing(0);

	    QWidget *hLineBackground = new QWidget();
	    hLineBackground->setFixedHeight(1);
	    hLineBackground->setObjectName("hLineBackground");

	    QVBoxLayout *topLayout = new QVBoxLayout;
	    topLayout->addWidget(txdNameBackground);
        topLayout->addLayout(menuVerLayout);
	    topLayout->addWidget(hLineBackground);
	    topLayout->setContentsMargins(0, 0, 0, 0);
	    topLayout->setMargin(0);
	    topLayout->setSpacing(0);

	    /* --- Bottom panel --- */
	    QWidget *hLineBackground2 = new QWidget;
	    hLineBackground2->setFixedHeight(1);
	    hLineBackground2->setObjectName("hLineBackground");
	    QWidget *txdOptionsBackground2 = new QWidget;
	    txdOptionsBackground2->setFixedHeight(59);
	    txdOptionsBackground2->setObjectName("background_1");

        /* --- Friendly Icons --- */
        QHBoxLayout *friendlyIconRow = new QHBoxLayout();
        friendlyIconRow->setContentsMargins( 0, 0, 15, 0 );
        friendlyIconRow->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

        this->friendlyIconRow = friendlyIconRow;

        QLabel *friendlyIconGame = new QLabel();
        friendlyIconGame->setVisible( false );

        this->friendlyIconGame = friendlyIconGame;

        friendlyIconRow->addWidget( friendlyIconGame );

        QWidget *friendlyIconSeparator = new QWidget();
        friendlyIconSeparator->setFixedWidth(1);
        friendlyIconSeparator->setObjectName( "friendlyIconSeparator" );
        friendlyIconSeparator->setVisible( false );

        this->friendlyIconSeparator = friendlyIconSeparator;

        friendlyIconRow->addWidget( friendlyIconSeparator );

        QLabel *friendlyIconPlatform = new QLabel();
        friendlyIconPlatform->setVisible( false );

        this->friendlyIconPlatform = friendlyIconPlatform;

        friendlyIconRow->addWidget( friendlyIconPlatform );

        txdOptionsBackground2->setLayout( friendlyIconRow );
	
	    QVBoxLayout *bottomLayout = new QVBoxLayout;
	    bottomLayout->addWidget(hLineBackground2);
	    bottomLayout->addWidget(txdOptionsBackground2);
	    bottomLayout->setContentsMargins(0, 0, 0, 0);
	    bottomLayout->setMargin(0);
	    bottomLayout->setSpacing(0);

	    /* --- Main layout --- */
	    QVBoxLayout *mainLayout = new QVBoxLayout;
	    mainLayout->addLayout(topLayout);
	    mainLayout->addWidget(mainSplitter);
	    mainLayout->addLayout(bottomLayout);

	    mainLayout->setContentsMargins(0, 0, 0, 0);
	    mainLayout->setMargin(0);
	    mainLayout->setSpacing(0);

	    QWidget *window = new QWidget();
	    window->setLayout(mainLayout);

	    setCentralWidget(window);

		imageWidget->hide();

		// Initialize our native formats.
		this->initializeNativeFormats();

        // Initialize the GUI.
        this->UpdateAccessibility();
    }
    catch( ... )
    {
        rwEngine->SetWarningManager( NULL );

        throw;
    }
}

MainWindow::~MainWindow()
{
    // If we have a loaded TXD, get rid of it.
    if ( this->currentTXD )
    {
        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = NULL;
    }

    delete txdLog;

    // Remove the warning manager again.
    this->rwEngine->SetWarningManager( NULL );

    // Shutdown the native format handlers.
    this->shutdownNativeFormats();
}

void MainWindow::addTextureFormatExportLinkToMenu( QMenu *theMenu, const char *displayName, const char *defaultExt, const char *formatName )
{
    TextureExportAction *formatActionExport = new TextureExportAction( defaultExt, displayName, QString( formatName ), this );
    theMenu->addAction( formatActionExport );

    this->actionsExportItems.push_back( formatActionExport );

    // Connect it to the export signal handler.
    connect( formatActionExport, &QAction::triggered, this, &MainWindow::onExportTexture );
}

void MainWindow::UpdateExportAccessibility( void )
{
    // Export options are available depending on what texture has been selected.
    bool has_txd = ( this->currentTXD != NULL );
   
    for ( TextureExportAction *exportAction : this->actionsExportItems )
    {
        bool shouldEnable = has_txd;

        if ( shouldEnable )
        {
            // We should only enable if the currently selected texture actually supports us.
            bool hasSupport = false;

            if ( !hasSupport )
            {
                if ( TexInfoWidget *curSelTex = this->currentSelectedTexture )
                {
                    if ( rw::Raster *texRaster = curSelTex->GetTextureHandle()->GetRaster() )
                    {
                        std::string ansiMethodName = exportAction->displayName.toStdString();

                        if ( stricmp( ansiMethodName.c_str(), "RWTEX" ) == 0 )
                        {
                            hasSupport = true;
                        }
                        else
                        {
                            hasSupport = texRaster->supportsImageMethod( ansiMethodName.c_str() );
                        }
                    }
                }
            }
            
            if ( !hasSupport )
            {
                // No texture item selected means we cannot export anyway.
                shouldEnable = false;
            }
        }

        exportAction->setDisabled( !shouldEnable );
    }

    this->exportAllImages->setDisabled( !has_txd );
}

void MainWindow::UpdateAccessibility( void )
{
    // If we have no TXD available, we should not allow the user to pick TXD related options.
    bool has_txd = ( this->currentTXD != NULL );

    this->actionSaveTXD->setDisabled( !has_txd );
    this->actionSaveTXDAs->setDisabled( !has_txd );
    this->actionCloseTXD->setDisabled( !has_txd );
    this->actionAddTexture->setDisabled( !has_txd );
    this->actionReplaceTexture->setDisabled( !has_txd );
    this->actionRemoveTexture->setDisabled( !has_txd );
    this->actionRenameTexture->setDisabled( !has_txd );
    this->actionResizeTexture->setDisabled( !has_txd );
    this->actionManipulateTexture->setDisabled( !has_txd );
    this->actionPlatformSelect->setDisabled( !has_txd );
    this->actionSetupMipmaps->setDisabled( !has_txd );
    this->actionClearMipmaps->setDisabled( !has_txd );
    this->actionRenderProps->setDisabled( !has_txd );
#ifndef _FEATURES_NOT_IN_CURRENT_RELEASE
    this->actionViewAllChanges->setDisabled( !has_txd );
    this->actionCancelAllChanges->setDisabled( !has_txd );
    this->actionAllTextures->setDisabled( !has_txd );
#endif //_FEATURES_NOT_IN_CURRENT_RELEASE
    this->actionSetupTXDVersion->setDisabled( !has_txd );

    this->UpdateExportAccessibility();
}

void MainWindow::setCurrentTXD( rw::TexDictionary *txdObj )
{
    if ( this->currentTXD == txdObj )
        return;

    if ( this->currentTXD != NULL )
    {
        // Make sure we have no more texture in our viewport.
		clearViewImage();

        // Since we have no selected texture, we can hide the friendly icons.
        this->hideFriendlyIcons();

        this->currentSelectedTexture = NULL;

        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = NULL;

        // Clear anything in the GUI that represented the previous TXD.
        this->textureListWidget->clear();
    }

    if ( txdObj != NULL )
    {
        this->currentTXD = txdObj;

        this->updateTextureList(false);
    }

    // We should update how we let the user access the GUI.
    this->UpdateAccessibility();
}

void MainWindow::updateTextureList( bool selectLastItemInList )
{
    rw::TexDictionary *txdObj = this->currentTXD;

    QListWidget *listWidget = this->textureListWidget;

    listWidget->clear();

    // We have no more selected texture item.
    this->currentSelectedTexture = NULL;

    this->hideFriendlyIcons();
    
    if ( txdObj )
    {
        TexInfoWidget *texInfoToSelect = NULL;

	    for ( rw::TexDictionary::texIter_t iter( txdObj->GetTextureIterator() ); iter.IsEnd() == false; iter.Increment() )
	    {
            rw::TextureBase *texItem = iter.Resolve();

	        QListWidgetItem *item = new QListWidgetItem;
	        listWidget->addItem(item);
            TexInfoWidget *texInfoWidget = new TexInfoWidget(item, texItem);
	        listWidget->setItemWidget(item, texInfoWidget);
		    item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), 54));

		    // select first or last item in a list
		    if (!texInfoToSelect || selectLastItemInList)
                texInfoToSelect = texInfoWidget;
	    }

        if (texInfoToSelect)
            listWidget->setCurrentItem(texInfoToSelect->listItem);
    }
}

void MainWindow::updateWindowTitle( void )
{
    QString windowTitleString = tr( "Magic.TXD" );

#ifdef _M_AMD64
    windowTitleString += " x64";
#endif

#ifdef _DEBUG
    windowTitleString += " DEBUG";
#endif

    if ( rw::TexDictionary *txd = this->currentTXD )
    {
        if ( this->hasOpenedTXDFileInfo )
        {
            windowTitleString += " (" + QString( this->openedTXDFileInfo.absoluteFilePath() ) + ")";
        }
    }

    setWindowTitle( windowTitleString );

    // Also update the top label.
    if (this->txdNameLabel)
    {
        if (rw::TexDictionary *txd = this->currentTXD)
        {
            QString topLabelDisplayString;

            if ( this->hasOpenedTXDFileInfo )
            {
                topLabelDisplayString = this->openedTXDFileInfo.fileName();
            }
            else
            {
                topLabelDisplayString = "New";
            }

            this->txdNameLabel->setText( topLabelDisplayString );
        }
        else
        {
            this->txdNameLabel->clear();
        }
    }

    // Update version button
    if (this->rwVersionButton)
    {
        if (rw::TexDictionary *txd = this->currentTXD)
        {
            this->rwVersionButton->setText(QString(txd->GetEngineVersion().toString().c_str()));
            this->rwVersionButton->show();
        }
        else
        {
            this->rwVersionButton->hide();
        }
    }
}

void MainWindow::updateTextureMetaInfo( void )
{
    if ( TexInfoWidget *infoWidget = this->currentSelectedTexture )
    {
        // Update it.
        infoWidget->updateInfo();

        // We also want to update the exportability, as the format may have changed.
        this->UpdateExportAccessibility();

        // We also want to update the friendly icons.
        this->updateFriendlyIcons();
    }
}

void MainWindow::updateAllTextureMetaInfo( void )
{
    QListWidget *textureList = this->textureListWidget;

    int rowCount = textureList->count();

    for ( int row = 0; row < rowCount; row++ )
    {
        QListWidgetItem *item = textureList->item( row );

        TexInfoWidget *texInfo = dynamic_cast <TexInfoWidget*> ( textureList->itemWidget( item ) );

        if ( texInfo )
        {
            texInfo->updateInfo();
        }
    }

    // Also update our friendly icons.
    this->updateFriendlyIcons();

    // Make sure we update exportability.
    this->UpdateExportAccessibility();
}

void MainWindow::onCreateNewTXD( bool checked )
{
    // Just create an empty TXD.
    rw::TexDictionary *newTXD = NULL;

    try
    {
        newTXD = rw::CreateTexDictionary( this->rwEngine );
    }
    catch( rw::RwException& except )
    {
        this->txdLog->showError( QString( "failed to create TXD: " ) + QString::fromStdString( except.message ) );

        // We failed.
        return;
    }

    if ( newTXD == NULL )
    {
        this->txdLog->showError( "unknown error in TXD creation" );

        return;
    }

    this->setCurrentTXD( newTXD );

    this->clearCurrentFilePath();
}

#include "tools/dirtools.h"

static CFile* OpenGlobalFile( MainWindow *mainWnd, const filePath& path, const filePath& mode )
{
    CFile *theFile = NULL;

    CFileTranslator *accessPoint = mainWnd->fileSystem->CreateSystemMinimumAccessPoint( path );

    if ( accessPoint )
    {
        theFile = accessPoint->Open( path, mode );

        if ( theFile )
        {
            theFile = CreateDecompressedStream( mainWnd, theFile );
        }

        delete accessPoint;
    }

    return theFile;
}

void MainWindow::openTxdFile(QString fileName) {
    this->txdLog->beforeTxdLoading();

    if (fileName.length() != 0)
    {
        // We got a file name, try to load that TXD file into our editor.
        std::wstring unicodeFileName = fileName.toStdWString();

        CFile *fileStream = OpenGlobalFile( this, unicodeFileName.c_str(), L"rb" );

        if ( fileStream )
        {
            try
            {
                rw::Stream *txdFileStream = RwStreamCreateTranslated( this->rwEngine, fileStream );

                // If the opening succeeded, process things.
                if (txdFileStream)
                {
                    this->txdLog->addLogMessage(QString("loading TXD: ") + fileName);

                    // Parse the input file.
                    rw::RwObject *parsedObject = NULL;

                    try
                    {
                        parsedObject = this->rwEngine->Deserialize(txdFileStream);
                    }
                    catch (rw::RwException& except)
                    {
                        this->txdLog->showError(QString("failed to load the TXD archive: %1").arg(QString::fromStdString(except.message)));
                    }

                    if (parsedObject)
                    {
                        // Try to cast it to a TXD. If it fails we did not get a TXD.
                        rw::TexDictionary *newTXD = rw::ToTexDictionary(this->rwEngine, parsedObject);

                        if (newTXD)
                        {
                            // Okay, we got a new TXD.
                            // Set it as our current object in the editor.
                            this->setCurrentTXD(newTXD);

                            this->setCurrentFilePath(fileName);
                        }
                        else
                        {
                            const char *objTypeName = this->rwEngine->GetObjectTypeName(parsedObject);

                            this->txdLog->addLogMessage(QString("found %1 but expected a texture dictionary").arg(objTypeName), LOGMSG_WARNING);

                            // Get rid of the object that is not a TXD.
                            this->rwEngine->DeleteRwObject(parsedObject);
                        }
                    }
                    // if parsedObject is NULL, the RenderWare implementation should have error'ed us already.

                    // Remember to close the stream again.
                    this->rwEngine->DeleteStream(txdFileStream);
                }
            }
            catch( ... )
            {
                delete fileStream;
                
                throw;
            }

            delete fileStream;
        }
    }

    this->txdLog->afterTxdLoading();
}

void MainWindow::onOpenFile( bool checked )
{
    QString fileName = QFileDialog::getOpenFileName( this, tr( "Open TXD file..." ), this->lastTXDOpenDir, tr( "RW Texture Archive (*.txd);;Any File (*.*)" ) );

    if ( fileName.length() != 0 )
    {
        // Store the new dir
        this->lastTXDOpenDir = QFileInfo( fileName ).absoluteDir().absolutePath();

        this->openTxdFile(fileName);
    }
}

void MainWindow::onCloseCurrent( bool checked )
{
    this->currentSelectedTexture = NULL;
    this->hasOpenedTXDFileInfo = false;

	clearViewImage();

    // Make sure we got no TXD active.
    this->setCurrentTXD( NULL );

    this->updateWindowTitle();
}

void MainWindow::onTextureItemChanged(QListWidgetItem *listItem, QListWidgetItem *prevTexInfoItem)
{
    QListWidget *texListWidget = this->textureListWidget;

    QWidget *listItemWidget = texListWidget->itemWidget( listItem );

    TexInfoWidget *texItem = dynamic_cast <TexInfoWidget*> ( listItemWidget );

    if ( this->currentSelectedTexture == NULL && texItem != NULL )
    {
        this->showFriendlyIcons();
    }
    if ( this->currentSelectedTexture != NULL && texItem == NULL )
    {
        this->hideFriendlyIcons();
    }

    this->currentSelectedTexture = texItem;

    this->updateFriendlyIcons();

    this->updateTextureView();

    // Change what textures we can export to.
    this->UpdateExportAccessibility();
}

void MainWindow::updateTextureView( void )
{
    TexInfoWidget *texItem = this->currentSelectedTexture;

    if ( texItem != NULL )
    {
		// Get the actual texture we are associated with and present it on the output pane.
		rw::TextureBase *theTexture = texItem->GetTextureHandle();
		rw::Raster *rasterData = theTexture->GetRaster();
		if (rasterData)
		{
            try
            {
			    // Get a bitmap to the raster.
			    // This is a 2D color component surface.
			    rw::Bitmap rasterBitmap( 32, rw::RASTER_8888, rw::COLOR_BGRA );

                if ( this->drawMipmapLayers && rasterData->getMipmapCount() > 1 )
                {
                    rasterBitmap.setBgColor( 1.0, 1.0, 1.0, 0.0 );

                    rw::DebugDrawMipmaps( this->rwEngine, rasterData, rasterBitmap );
                }
                else
                {
                    rasterBitmap = rasterData->getBitmap();
                }

			    QImage texImage = convertRWBitmapToQImage( rasterBitmap );

			    imageWidget->setPixmap(QPixmap::fromImage(texImage));
			    imageWidget->setFixedSize(QSize(texImage.width(), texImage.height()));
			    imageWidget->show();
            }
            catch( rw::RwException& except )
            {
				this->txdLog->addLogMessage(QString("failed to get bitmap from texture: ") + except.message.c_str(), LOGMSG_WARNING);

                // We hide the image widget.
                this->clearViewImage();
            }
		}
    }
}

void MainWindow::onToggleShowMipmapLayers( bool checked )
{
    this->drawMipmapLayers = !( this->drawMipmapLayers );

    // Update the texture view.
    this->updateTextureView();
}

void MainWindow::onToggleShowBackground(bool checked)
{
	this->showBackground = !(this->showBackground);
	if (showBackground)
		imageWidget->setStyleSheet("background-image: url(\"" + this->m_appPathForStyleSheet + "/resources/viewBackground.png\");");
	else
		imageWidget->setStyleSheet("background-color: rgba(255, 255, 255, 0);");
}

void MainWindow::onToggleShowLog( bool checked )
{
    // Make sure the log is visible.
	this->txdLog->show();
}

void MainWindow::onToogleDarkTheme(bool checked) {
    if (checked && !this->recheckingThemeItem) {
        this->actionThemeLight->setChecked(false);
        this->starsMovie->stop();
        this->setStyleSheet(styles::get(this->m_appPath, "resources\\dark.shell"));
        this->starsMovie->setFileName(makeAppPath("resources\\dark\\stars.gif"));
        this->starsMovie->start();
    }
    else {
        this->recheckingThemeItem = true;
        this->actionThemeDark->setChecked(true);
        this->recheckingThemeItem = false;
    }
}

void MainWindow::onToogleLightTheme(bool checked) {
    if (checked && !this->recheckingThemeItem) {
        this->actionThemeDark->setChecked(false);
        this->starsMovie->stop();
        this->setStyleSheet(styles::get(this->m_appPath, "resources\\light.shell"));
        this->starsMovie->setFileName(makeAppPath("resources\\light\\stars.gif"));
        this->starsMovie->start();
    }
    else {
        this->recheckingThemeItem = true;
        this->actionThemeLight->setChecked(true);
        this->recheckingThemeItem = false;
    }
}

void MainWindow::onSelectPlatform( bool checked )
{
    // Show the window with a combo box of all available platforms.

    if ( this->currentTXD == NULL )
        return;

    // No point in setting a platform if there are no textures.
    if ( this->currentTXD->GetTextureCount() == 0 )
        return;

    if ( PlatformSelWindow *curDlg = this->platformDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        PlatformSelWindow *dialog = new PlatformSelWindow( this );
        dialog->setVisible( true );
    }
}

void MainWindow::onSetupMipmapLayers( bool checked )
{
    // We just generate up to the top mipmap level for now.
    if ( TexInfoWidget *texInfo = this->currentSelectedTexture )
    {
        rw::TextureBase *texture = texInfo->GetTextureHandle();

        // Generate mipmaps of raster.
        if ( rw::Raster *texRaster = texture->GetRaster() )
        {
            texRaster->generateMipmaps( 32, rw::MIPMAPGEN_DEFAULT );

            // Fix texture filtering modes.
            texture->fixFiltering();
        }
    }

    // Make sure we update the info.
    this->updateTextureMetaInfo();

    // Update the texture view.
    this->updateTextureView();
}

void MainWindow::onClearMipmapLayers( bool checked )
{
    // Here is a quick way to clear mipmap layers from a texture.
    if ( TexInfoWidget *texInfo = this->currentSelectedTexture )
    {
        rw::TextureBase *texture = texInfo->GetTextureHandle();

        // Clear the mipmaps from the raster.
        if ( rw::Raster *texRaster = texture->GetRaster() )
        {
            texRaster->clearMipmaps();

            // Fix the filtering.
            texture->fixFiltering();
        }
    }

    // Update the info.
    this->updateTextureMetaInfo();

    // Update the texture view.
    this->updateTextureView();
}

void MainWindow::saveCurrentTXDAt( QString txdFullPath )
{
    if ( rw::TexDictionary *currentTXD = this->currentTXD )
    {
        // We serialize what we have at the location we loaded the TXD from.
        std::wstring unicodeFullPath = txdFullPath.toStdWString();

        rw::streamConstructionFileParamW_t fileOpenParam( unicodeFullPath.c_str() );

        rw::Stream *newTXDStream = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_CREATE, &fileOpenParam );

        if ( newTXDStream )
        {
            // Write the TXD into it.
            try
            {
                this->rwEngine->Serialize( currentTXD, newTXDStream );

                // Success, so lets update our target filename.
                this->setCurrentFilePath( txdFullPath );
            }
            catch( rw::RwException& except )
            {
				this->txdLog->addLogMessage(QString("failed to save the TXD archive: %1").arg(except.message.c_str()), LOGMSG_ERROR);
            }

            // Close the stream.
            this->rwEngine->DeleteStream( newTXDStream );
        }
    }
}

void MainWindow::onRequestSaveTXD( bool checked )
{
    if ( this->currentTXD != NULL )
    {
        if ( this->hasOpenedTXDFileInfo )
        {
            QString txdFullPath = this->openedTXDFileInfo.absoluteFilePath();

            if ( txdFullPath.length() != 0 )
            {
                this->saveCurrentTXDAt( txdFullPath );
            }
        }
        else
        {
            this->onRequestSaveAsTXD( checked );
        }
    }
}

void MainWindow::onRequestSaveAsTXD( bool checked )
{
    if ( this->currentTXD != NULL )
    {
        QString newSaveLocation = QFileDialog::getSaveFileName( this, "Save TXD as...", this->lastTXDSaveDir, tr( "RW Texture Dictionary (*.txd)" ) );

        if ( newSaveLocation.length() != 0 )
        {
            // Save location.
            this->lastTXDSaveDir = QFileInfo( newSaveLocation ).absoluteDir().absolutePath();

            this->saveCurrentTXDAt( newSaveLocation );
        }
    }
}

static void serializeRaster( rw::Stream *outputStream, rw::Raster *texRaster, const char *method )
{
    // TODO: add DDS file writer functionality, by checking method for "DDS"

    // Serialize it.
    texRaster->writeImage( outputStream, method );
}

void MainWindow::DoAddTexture( const TexAddDialog::texAddOperation& params )
{
    TexAddDialog::texAddOperation::eAdditionType add_type = params.add_type;

    if ( add_type == TexAddDialog::texAddOperation::ADD_TEXCHUNK )
    {
        // This is just adding the texture chunk to our TXD.
        rw::TextureBase *texHandle = (rw::TextureBase*)rw::AcquireObject( params.add_texture.texHandle );

        texHandle->AddToDictionary( this->currentTXD );

        // Update the texture list.
        this->updateTextureList(true);
    }
    else if ( add_type == TexAddDialog::texAddOperation::ADD_RASTER )
    {
        rw::Raster *newRaster = params.add_raster.raster;

        if ( newRaster )
        {
            try
            {
                // We want to create a texture and put it into our TXD.
                rw::TextureBase *newTexture = rw::CreateTexture( this->rwEngine, newRaster );

                if ( newTexture )
                {
                    // We need to set default texture rendering properties.
                    newTexture->SetFilterMode( rw::RWFILTER_LINEAR );
                    newTexture->SetUAddressing( rw::RWTEXADDRESS_WRAP );
                    newTexture->SetVAddressing( rw::RWTEXADDRESS_WRAP );

                    // Give it a name.
                    newTexture->SetName( params.add_raster.texName.c_str() );
                    newTexture->SetMaskName( params.add_raster.maskName.c_str() );

                    // Now put it into the TXD.
                    newTexture->AddToDictionary( currentTXD );

                    // Update the texture list.
                    this->updateTextureList(true);
                }
                else
                {
                    this->txdLog->showError( "failed to create texture" );
                }
            }
            catch( rw::RwException& except )
            {
                this->txdLog->showError( QString( "failed to add texture: " ) + QString::fromStdString( except.message ) );

                // Just continue.
            }
        }
    }
}

QString MainWindow::requestValidImagePath( void )
{
    // Get the name of a texture to add.
    // For that we want to construct a list of all possible image extensions.
    QString imgExtensionSelect;

    bool hasEntry = false;

    const imageFormats_t& avail_formats = this->reg_img_formats;

    // Add any image file.
    if ( hasEntry )
    {
        imgExtensionSelect += ";;";
    }

    imgExtensionSelect += "Image file (";

    bool hasExtEntry = false;

    for ( imageFormats_t::const_iterator iter = avail_formats.begin(); iter != avail_formats.end(); iter++ )
    {
        if ( hasExtEntry )
        {
            imgExtensionSelect += ";";
        }

        const registered_image_format& entry = *iter;

        bool needsExtSep = false;

        for ( const std::string& extName : entry.ext_array )
        {
            if ( needsExtSep )
            {
                imgExtensionSelect += ";";
            }

            imgExtensionSelect += QString( "*." ) + QString::fromStdString( extName ).toLower();

            needsExtSep = true;
        }

        hasExtEntry = true;
    }

    // TEX CHUNK.
    {
        if ( hasExtEntry )
        {
            imgExtensionSelect += ";";
        }

        imgExtensionSelect += QString( "*.rwtex" );
    }

    imgExtensionSelect += ")";

    hasEntry = true;

    for ( imageFormats_t::const_iterator iter = avail_formats.begin(); iter != avail_formats.end(); iter++ )
    {
        if ( hasEntry )
        {
            imgExtensionSelect += ";;";
        }

        const registered_image_format& entry = *iter;

        imgExtensionSelect += QString::fromStdString( entry.formatName ) + QString( " (" );
        
        bool needsExtSep = false;

        for ( const std::string& extName : entry.ext_array )
        {
            if ( needsExtSep )
            {
                imgExtensionSelect += ";";
            }

            imgExtensionSelect +=
                QString( "*." ) + QString::fromStdString( extName ).toLower();

            needsExtSep = true;
        }
        
        imgExtensionSelect += QString( ")" );

        hasEntry = true;
    }

    // Add any file.
    if ( hasEntry )
    {
        imgExtensionSelect += ";;";
    }

    imgExtensionSelect += "RW Texture Chunk (*.rwtex);;Any file (*.*)";

    hasEntry = true;

    QString imagePath = QFileDialog::getOpenFileName( this, "Import Texture...", this->lastImageFileOpenDir, imgExtensionSelect );

    // Remember the directory.
    if ( imagePath.length() != 0 )
    {
        this->lastImageFileOpenDir = QFileInfo( imagePath ).absoluteDir().absolutePath();
    }

    return imagePath;
}

void MainWindow::onAddTexture( bool checked )
{
    // Allow importing of a texture.
    rw::TexDictionary *currentTXD = this->currentTXD;

    if ( currentTXD != NULL )
    {
        QString fileName = this->requestValidImagePath();

        if ( fileName.length() != 0 )
        {
            auto cb_lambda = [=] ( const TexAddDialog::texAddOperation& params )
            {
                this->DoAddTexture( params );
            };

            TexAddDialog::dialogCreateParams params;
            params.actionName = "Add";
            params.type = TexAddDialog::CREATE_IMGPATH;
            params.img_path.imgPath = fileName;

            TexAddDialog *texAddTask = new TexAddDialog( this, params, std::move( cb_lambda ) );

            //texAddTask->move( 200, 250 );
            texAddTask->setVisible( true );
        }
    }
}

void MainWindow::onReplaceTexture( bool checked )
{
    // Replacing a texture means that we search for another texture on disc.
    // We prompt the user to input a replacement that has exactly the same texture properties
    // (name, addressing mode, etc) but different raster properties (maybe).

    // We need to have a texture selected to replace.
    if ( TexInfoWidget *curSelTexItem = this->currentSelectedTexture )
    {
        QString replaceImagePath = this->requestValidImagePath();

        if ( replaceImagePath.length() != 0 )
        {
            auto cb_lambda = [=] ( const TexAddDialog::texAddOperation& params )
            {
                rw::Interface *rwEngine = this->GetEngine();

                // Replace stuff.
                TexAddDialog::texAddOperation::eAdditionType add_type = params.add_type;

                if ( add_type == TexAddDialog::texAddOperation::ADD_TEXCHUNK )
                {
                    // We just take the texture and replace our existing texture with it.
                    if ( rw::TextureBase *curTex = curSelTexItem->GetTextureHandle() )
                    {
                        curSelTexItem->SetTextureHandle( NULL );

                        rwEngine->DeleteRwObject( curTex );
                    }

                    rw::TextureBase *newTex = (rw::TextureBase*)rw::AcquireObject( params.add_texture.texHandle );

                    if ( newTex )
                    {
                        curSelTexItem->SetTextureHandle( newTex );

                        // Add the new texture to the dictionary.
                        newTex->AddToDictionary( this->currentTXD );
                    }
                }
                else if ( add_type == TexAddDialog::texAddOperation::ADD_RASTER )
                {
                    rw::TextureBase *tex = curSelTexItem->GetTextureHandle();

                    // We have to update names.
                    tex->SetName( params.add_raster.texName.c_str() );
                    tex->SetMaskName( params.add_raster.maskName.c_str() );
                
                    // Update raster handle.
                    tex->SetRaster( params.add_raster.raster );
                }

                // Update info.
                this->updateTextureMetaInfo();

                this->updateTextureView();
            };

            TexAddDialog::dialogCreateParams params;
            params.actionName = "Replace";
            params.type = TexAddDialog::CREATE_IMGPATH;
            params.img_path.imgPath = replaceImagePath;

            // Overwrite some properties.
            QString overwriteTexName = QString::fromStdString( curSelTexItem->GetTextureHandle()->GetName() );

            params.overwriteTexName = &overwriteTexName;

            TexAddDialog *texAddTask = new TexAddDialog( this, params, std::move( cb_lambda ) );

            texAddTask->move( 200, 250 );
            texAddTask->setVisible( true );
        }
    }
}

void MainWindow::onRemoveTexture( bool checked )
{
    // Pretty simple. We get rid of the currently selected texture item.

    if ( TexInfoWidget *curSelTexItem = this->currentSelectedTexture )
    {
        // Forget about this selected item.
        this->currentSelectedTexture = NULL;

        // We kill the texture in this item.
        rw::TextureBase *tex = curSelTexItem->GetTextureHandle();

        // First delete this item from the list.
        curSelTexItem->remove();

        // Now kill the texture.
        this->rwEngine->DeleteRwObject( tex );

        // If we have no more items in the list widget, we should hide our texture view page.
        if ( this->textureListWidget->selectedItems().count() == 0 )
        {
            this->clearViewImage();

            // We should also hide the friendly icons, since they only should show if a texture is selected.
            this->hideFriendlyIcons();
        }
    }
}

void MainWindow::onRenameTexture( bool checked )
{
    // Change the name of the currently selected texture.

    if ( this->texNameDlg )
        return;

    if ( TexInfoWidget *texInfo = this->currentSelectedTexture )
    {
        TexNameWindow *texNameDlg = new TexNameWindow( this, texInfo );

        texNameDlg->setVisible( true );
    }
}

void MainWindow::onResizeTexture( bool checked )
{
    // Change the texture dimensions.

    if ( TexInfoWidget *texInfo = this->currentSelectedTexture )
    {
        if ( TexResizeWindow *curDlg = this->resizeDlg )
        {
            curDlg->setFocus();
        }
        else
        {
            TexResizeWindow *dialog = new TexResizeWindow( this, texInfo );
            dialog->setVisible( true );
        }
    }
}

void MainWindow::onManipulateTexture( bool checked )
{
    // Manipulating a raster is taking that raster and creating a new copy that is more beautiful.
    // We can easily reuse the texture add dialog for this task.

    // For that we need a selected texture.
    if ( TexInfoWidget *curSelTexItem = this->currentSelectedTexture )
    {
        auto cb_lambda = [=] ( const TexAddDialog::texAddOperation& params )
        {
            assert( params.add_type == TexAddDialog::texAddOperation::ADD_RASTER );

            // Update the stored raster.
            rw::TextureBase *tex = curSelTexItem->GetTextureHandle();

            // Update names.
            tex->SetName( params.add_raster.texName.c_str() );
            tex->SetMaskName( params.add_raster.maskName.c_str() );

            // Replace raster handle.
            tex->SetRaster( params.add_raster.raster );

            // Update info.
            this->updateTextureMetaInfo();

            this->updateTextureView();
        };

        TexAddDialog::dialogCreateParams params;
        params.actionName = "Modify";
        params.type = TexAddDialog::CREATE_RASTER;
        params.orig_raster.tex = curSelTexItem->GetTextureHandle();

        TexAddDialog *texAddTask = new TexAddDialog( this, params, std::move( cb_lambda ) );

        texAddTask->move( 200, 250 );
        texAddTask->setVisible( true );
    }
}

void MainWindow::onExportTexture( bool checked )
{
    // We are always sent by a QAction object.
    TextureExportAction *senderAction = (TextureExportAction*)this->sender();

    // Make sure we have selected a texture in the texture list.
    // Get it.
    TexInfoWidget *selectedTexture = this->currentSelectedTexture;

    if ( selectedTexture != NULL )
    {
        rw::TextureBase *texHandle = selectedTexture->GetTextureHandle();

        if ( texHandle )
        {
            try
            {
                const QString& defaultExt = senderAction->defaultExt;
                const QString& exportFunction = senderAction->displayName;
                const QString& formatName = senderAction->formatName;

                std::string ansiExportFunction = exportFunction.toStdString();

                const QString actualExt = defaultExt.toLower();
            
                // Construct a default filename for the object.
                QString defaultFileName = QString( texHandle->GetName().c_str() ) + "." + actualExt;

                // Request a filename and do the export.
                QString finalFilePath =
                    QFileDialog::getSaveFileName(
                        this, QString( "Save " ) + exportFunction + QString( " as..." ), defaultFileName,
                        formatName + " (*." + actualExt + ");;Any (*.*)"
                    );

                if ( finalFilePath.length() != 0 )
                {
                    // Try to open that file for writing.
                    std::wstring unicodeImagePath = finalFilePath.toStdWString();
                
                    rw::streamConstructionFileParamW_t fileParam( unicodeImagePath.c_str() );

                    rw::Stream *imageStream = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_CREATE, &fileParam );

                    if ( imageStream )
                    {
                        try
                        {
                            // Directly write us.
                            if ( stricmp( ansiExportFunction.c_str(), "RWTEX" ) == 0 )
                            {
                                rwEngine->Serialize( texHandle, imageStream );
                            }
                            else
                            {
                                rw::Raster *texRaster = texHandle->GetRaster();

                                if ( texRaster )
                                {
                                    serializeRaster( imageStream, texRaster, ansiExportFunction.c_str() );
                                }
                            }
                        }
                        catch( ... )
                        {
                            this->rwEngine->DeleteStream( imageStream );

                            // Since we failed, we do not want that image stream anymore.
                            _wremove( unicodeImagePath.c_str() );

                            throw;
                        }

                        // Close the stream again.
                        this->rwEngine->DeleteStream( imageStream );
                    }
                }
            }
            catch( rw::RwException& except )
            {
                this->txdLog->showError( QString( "error during image output: " ) + except.message.c_str() );

                // We proceed.
            }
        }
    }
}

void MainWindow::onExportAllTextures( bool checked )
{
    if ( rw::TexDictionary *texDict = this->currentTXD )
    {
        // No point in exporting empty TXD.
        if ( texDict->GetTextureCount() != 0 )
        {
            ExportAllWindow *curDlg = new ExportAllWindow( this, texDict );

            curDlg->setVisible( true );
        }
    }
}

void MainWindow::clearViewImage()
{
	imageWidget->clear();
    imageWidget->setFixedSize(1, 1);
	imageWidget->hide();
}

const char* MainWindow::GetTXDPlatformString( rw::TexDictionary *txd )
{
    // We return the platform of the first texture.
    // Otherwise we return NULL.
    rw::TexDictionary::texIter_t texIter = txd->GetTextureIterator();

    if ( texIter.IsEnd() )
        return NULL;

    rw::Raster *texRaster = texIter.Resolve()->GetRaster();

    if ( !texRaster )
        return NULL;

    return texRaster->getNativeDataTypeName();
}

void MainWindow::SetTXDPlatformString( rw::TexDictionary *txd, const char *platform )
{
    // To change the platform of a TXD we have to set all of it's textures platforms.
    for ( rw::TexDictionary::texIter_t iter( txd->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
    {
        rw::TextureBase *texHandle = iter.Resolve();

        rw::Raster *texRaster = texHandle->GetRaster();

        if ( texRaster )
        {
            try
            {
                rw::ConvertRasterTo( texRaster, platform );
            }
            catch( rw::RwException& except )
            {
                this->txdLog->showError( QString::fromStdString( std::string( "failed to change platform of texture '" ) + texHandle->GetName() + "': " + except.message ) );

                // Continue changing platform.
            }
        }
    }
}

void MainWindow::onSetupRenderingProps( bool checked )
{
    if ( checked == true )
        return;

    if ( TexInfoWidget *texInfo = this->currentSelectedTexture )
    {
        if ( RenderPropWindow *curDlg = this->renderPropDlg )
        {
            curDlg->setFocus();
        }
        else
        {
            RenderPropWindow *dialog = new RenderPropWindow( this, texInfo );
            dialog->setVisible( true );
        }
    }
}

void MainWindow::onSetupTxdVersion(bool checked) {
    if ( checked == true )
        return;

    if ( RwVersionDialog *curDlg = this->verDlg )
    {
        curDlg->setFocus();
    }
    else
    {
	    RwVersionDialog *dialog = new RwVersionDialog( this );
	    dialog->setVisible( true );

        this->verDlg = dialog;
    }
}

void MainWindow::onShowOptions(bool checked)
{
    if ( QDialog *curDlg = this->optionsDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        OptionsDialog *optionsDlg = new OptionsDialog( this );

        optionsDlg->setVisible( true );
    }
}

void MainWindow::onRequestMassConvert(bool checked)
{
    MassConvertWindow *massconv = new MassConvertWindow( this );

    massconv->setVisible( true );
}

void MainWindow::onRequestMassExport(bool checked)
{
    MassExportWindow *massexport = new MassExportWindow( this );

    massexport->setVisible( true );
}

void MainWindow::onRequestOpenWebsite(bool checked)
{
    // TODO ;o
    QDesktopServices::openUrl( QUrl( "http://niceme.me" ) );
}

void MainWindow::onAboutUs(bool checked)
{
    if ( QDialog *curDlg = this->aboutDlg )
    {
        curDlg->setFocus();
    }
    else
    {
        AboutDialog *aboutDlg = new AboutDialog( this );

        aboutDlg->setVisible( true );
    }
}

QString MainWindow::makeAppPath(QString subPath) {
    return m_appPath + "\\" + subPath;
}