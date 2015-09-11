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

#include "textureViewport.h"
#include "rwversiondialog.h"
#include "texnamewindow.h"
#include "renderpropwindow.h"
#include "resizewindow.h"

#include "qtrwutils.hxx"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    rwWarnMan( this )
{
    // Initialize variables.
    this->currentTXD = NULL;
    this->txdNameLabel = NULL;
    this->currentSelectedTexture = NULL;
    this->txdLog = NULL;
    this->verDlg = NULL;
    this->texNameDlg = NULL;
    this->renderPropDlg = NULL;
    this->resizeDlg = NULL;
    this->rwVersionButton = NULL;

    this->drawMipmapLayers = false;
	this->showBackground = false;

    this->hasOpenedTXDFileInfo = false;

    // Initialize the RenderWare engine.
    rw::LibraryVersion engineVersion;

    // This engine version is the default version we create resources in.
    // Resources can change their version at any time, so we do not have to change this.
    engineVersion.rwLibMajor = 3;
    engineVersion.rwLibMinor = 6;
    engineVersion.rwRevMajor = 0;
    engineVersion.rwRevMinor = 3;

    this->rwEngine = rw::CreateEngine( engineVersion );

    if ( this->rwEngine == NULL )
    {
        throw std::exception( "failed to initialize the RenderWare engine" );
    }

    // Set some typical engine properties.
    this->rwEngine->SetIgnoreSerializationBlockRegions( true );
    this->rwEngine->SetIgnoreSecureWarnings( false );

    this->rwEngine->SetWarningLevel( 3 );
    this->rwEngine->SetWarningManager( &this->rwWarnMan );

    this->rwEngine->SetDXTRuntime( rw::DXTRUNTIME_SQUISH );
    this->rwEngine->SetPaletteRuntime( rw::PALRUNTIME_PNGQUANT );

    // Give RenderWare some info about us!
    rw::softwareMetaInfo metaInfo;
    metaInfo.applicationName = "Magic.TXD";
    metaInfo.applicationVersion = MTXD_VERSION_STRING;
    metaInfo.description = "by DK22Pac and The_GTA (https://github.com/quiret/magic-txd)";

    this->rwEngine->SetApplicationInfo( metaInfo );

    try
    {
	    /* --- Window --- */
        updateWindowTitle();
        setMinimumSize(460, 300);
	    resize(900, 680);

		/* --- Log --- */
		this->txdLog = new TxdLog(this);

	    /* --- List --- */
	    QListWidget *listWidget = new QListWidget();
	    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
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
	    QSplitter *splitter = new QSplitter;
	    splitter->addWidget(listWidget);
		splitter->addWidget(imageView);
	    QList<int> sizes;
	    sizes.push_back(200);
	    sizes.push_back(splitter->size().width() - 200);
	    splitter->setSizes(sizes);
	    splitter->setChildrenCollapsible(false);

	    /* --- Top panel --- */
	    QWidget *txdNameBackground = new QWidget();
	    txdNameBackground->setFixedHeight(60);
	    txdNameBackground->setObjectName("txdNameBackground");
	    QLabel *txdName = new QLabel();
	    txdName->setObjectName("label36px");
	    txdName->setAlignment(Qt::AlignCenter);

        this->txdNameLabel = txdName;

	    QGridLayout *txdNameLayout = new QGridLayout();
	    QLabel *starsBox = new QLabel;
	    QMovie *stars = new QMovie;
	    stars->setFileName("resources\\dark\\stars.gif");
	    starsBox->setMovie(stars);
	    stars->start();
	    txdNameLayout->addWidget(starsBox, 0, 0);
	    txdNameLayout->addWidget(txdName, 0, 0);
	    txdNameLayout->setContentsMargins(0, 0, 0, 0);
	    txdNameLayout->setMargin(0);
	    txdNameLayout->setSpacing(0);
	    txdNameBackground->setLayout(txdNameLayout);
	
	    QWidget *txdOptionsBackground = new QWidget();
	    txdOptionsBackground->setFixedHeight(54);
	    txdOptionsBackground->setObjectName("txdOptionsBackground");

	    /* --- Menu --- */
	    QMenuBar *menu = new QMenuBar;
	    QMenu *fileMenu = menu->addMenu(tr("&File"));
        QAction *actionNew = new QAction("&New", this);
        fileMenu->addAction(actionNew);
        
        connect( actionNew, &QAction::triggered, this, &MainWindow::onCreateNewTXD );

	    QAction *actionOpen = new QAction("&Open", this);
	    fileMenu->addAction(actionOpen);

        connect( actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile );

	    QAction *actionSave = new QAction("&Save", this);
	    fileMenu->addAction(actionSave);

        connect( actionSave, &QAction::triggered, this, &MainWindow::onRequestSaveTXD );

	    QAction *actionSaveAs = new QAction("&Save as...", this);
	    fileMenu->addAction(actionSaveAs);

        connect( actionSaveAs, &QAction::triggered, this, &MainWindow::onRequestSaveAsTXD );

	    QAction *closeCurrent = new QAction("&Close current", this);
	    fileMenu->addAction(closeCurrent);
	    fileMenu->addSeparator();

        connect( closeCurrent, &QAction::triggered, this, &MainWindow::onCloseCurrent );

	    QAction *actionQuit = new QAction("&Quit", this);
	    fileMenu->addAction(actionQuit);

	    QMenu *editMenu = menu->addMenu(tr("&Edit"));
	    QAction *actionAdd = new QAction("&Add", this);
	    editMenu->addAction(actionAdd);

        connect( actionAdd, &QAction::triggered, this, &MainWindow::onAddTexture );

	    QAction *actionReplace = new QAction("&Replace", this);
	    editMenu->addAction(actionReplace);

        connect( actionReplace, &QAction::triggered, this, &MainWindow::onReplaceTexture );

	    QAction *actionRemove = new QAction("&Remove", this);
	    editMenu->addAction(actionRemove);

        connect( actionRemove, &QAction::triggered, this, &MainWindow::onRemoveTexture );

	    QAction *actionRename = new QAction("&Rename", this);
	    editMenu->addAction(actionRename);

        connect( actionRename, &QAction::triggered, this, &MainWindow::onRenameTexture );

	    QAction *actionResize = new QAction("&Resize", this);
	    editMenu->addAction(actionResize);

        connect( actionResize, &QAction::triggered, this, &MainWindow::onResizeTexture );

        QAction *actionManipulate = new QAction("&Manipulate", this);
        editMenu->addAction(actionManipulate);

        connect( actionManipulate, &QAction::triggered, this, &MainWindow::onManipulateTexture );

	    QAction *actionSetPixelFormat = new QAction("&Setup pixel format", this);
	    editMenu->addAction(actionSetPixelFormat);
	    QAction *actionSetupMipLevels = new QAction("&Setup mip-levels", this);
	    editMenu->addAction(actionSetupMipLevels);

        connect( actionSetupMipLevels, &QAction::triggered, this, &MainWindow::onSetupMipmapLayers );

        QAction *actionClearMipLevels = new QAction("&Clear mip-levels", this);
        editMenu->addAction(actionClearMipLevels);

        connect( actionClearMipLevels, &QAction::triggered, this, &MainWindow::onClearMipmapLayers );

	    QAction *actionSetupRenderingProperties = new QAction("&Setup rendering properties", this);
	    editMenu->addAction(actionSetupRenderingProperties);

        connect( actionSetupRenderingProperties, &QAction::triggered, this, &MainWindow::onSetupRenderingProps );

	    editMenu->addSeparator();
	    QAction *actionViewAllChanges = new QAction("&View all changes", this);
	    editMenu->addAction(actionViewAllChanges);
	    QAction *actionCancelAllChanges = new QAction("&Cancel all changes", this);
	    editMenu->addAction(actionCancelAllChanges);
	    editMenu->addSeparator();
	    QAction *actionAllTextures = new QAction("&All textures", this);
	    editMenu->addAction(actionAllTextures);
	    editMenu->addSeparator();
	    QAction *actionSetupTxdVersion = new QAction("&Setup TXD version", this);
	    editMenu->addAction(actionSetupTxdVersion);

		connect(actionSetupTxdVersion, &QAction::triggered, this, &MainWindow::onSetupTxdVersion);

	    QMenu *exportMenu = menu->addMenu(tr("&Export"));

        this->addTextureFormatExportLinkToMenu( exportMenu, "PNG", "Portable Network Graphics" );
        this->addTextureFormatExportLinkToMenu( exportMenu, "DDS", "Direct Draw Surface" );
        this->addTextureFormatExportLinkToMenu( exportMenu, "BMP", "Raw Bitmap" );

        // Add remaining formats that rwlib supports.
        {
            rw::registered_image_formats_t regFormats;
            
            rw::GetRegisteredImageFormats( this->rwEngine, regFormats );

            for ( rw::registered_image_formats_t::const_iterator iter = regFormats.cbegin(); iter != regFormats.cend(); iter++ )
            {
                const rw::registered_image_format& theFormat = *iter;

                if ( stricmp( theFormat.defaultExt, "PNG" ) != 0 &&
                     stricmp( theFormat.defaultExt, "DDS" ) != 0 &&
                     stricmp( theFormat.defaultExt, "BMP" ) != 0 )
                {
                    this->addTextureFormatExportLinkToMenu( exportMenu, theFormat.defaultExt, theFormat.formatName );
                }

                // We want to cache the available formats.
                registered_image_format imgformat;

                imgformat.formatName = theFormat.formatName;
                imgformat.defaultExt = theFormat.defaultExt;
                imgformat.isNativeFormat = false;

                this->reg_img_formats.push_back( std::move( imgformat ) );
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

                    this->reg_img_formats.push_back( std::move( imgformat ) );
                }
            }
        }

	    QAction *actionExportTTXD = new QAction("&Text-based TXD", this);
	    exportMenu->addAction(actionExportTTXD);
	    exportMenu->addSeparator();
	    QAction *actionExportAll = new QAction("&Export all", this);
	    exportMenu->addAction(actionExportAll);

	    QMenu *viewMenu = menu->addMenu(tr("&View"));
	    QAction *actionBackground = new QAction("&Background", this);
		actionBackground->setCheckable(true);
	    viewMenu->addAction(actionBackground);

		connect(actionBackground, &QAction::triggered, this, &MainWindow::onToggleShowBackground);

	    QAction *action3dView = new QAction("&3D View", this);
		action3dView->setCheckable(true);
	    viewMenu->addAction(action3dView);
	    QAction *actionShowMipLevels = new QAction("&Display mip-levels", this);
		actionShowMipLevels->setCheckable(true);
	    viewMenu->addAction(actionShowMipLevels);

        connect( actionShowMipLevels, &QAction::triggered, this, &MainWindow::onToggleShowMipmapLayers );
        
        QAction *actionShowLog = new QAction("&Show Log", this);
        viewMenu->addAction(actionShowLog);

        connect( actionShowLog, &QAction::triggered, this, &MainWindow::onToggleShowLog );

	    viewMenu->addSeparator();
	    QAction *actionSetupTheme = new QAction("&Setup theme", this);
	    viewMenu->addAction(actionSetupTheme);

	    actionQuit->setShortcut(QKeySequence::Quit);
	    connect(actionQuit, SIGNAL(triggered()), this, SLOT(close()));

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
	    txdOptionsBackground2->setObjectName("txdOptionsBackground");
	
	    QVBoxLayout *bottomLayout = new QVBoxLayout;
	    bottomLayout->addWidget(hLineBackground2);
	    bottomLayout->addWidget(txdOptionsBackground2);
	    bottomLayout->setContentsMargins(0, 0, 0, 0);
	    bottomLayout->setMargin(0);
	    bottomLayout->setSpacing(0);

	    /* --- Main layout --- */
	    QVBoxLayout *mainLayout = new QVBoxLayout;
	    mainLayout->addLayout(topLayout);
	    mainLayout->addWidget(splitter);
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
    }
    catch( ... )
    {
        // Delete our engine again.
        rw::DeleteEngine( this->rwEngine );

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

    // Shutdown the native format handlers.
    this->shutdownNativeFormats();

    // Destroy the engine again.
    rw::DeleteEngine( this->rwEngine );

    this->rwEngine = NULL;

	delete txdLog;
}

class TextureExportAction : public QAction
{
public:
    TextureExportAction( QString defaultExt, QString formatName, QWidget *parent ) : QAction( QString( "&" ) + defaultExt, parent )
    {
        this->defaultExt = defaultExt;
        this->formatName = formatName;
    }

    QString defaultExt;
    QString formatName;
};

void MainWindow::addTextureFormatExportLinkToMenu( QMenu *theMenu, const char *defaultExt, const char *formatName )
{
    TextureExportAction *formatActionExport = new TextureExportAction( defaultExt, QString( formatName ), this );
    theMenu->addAction( formatActionExport );

    formatActionExport->setData( QString( defaultExt ) );

    // Connect it to the export signal handler.
    connect( formatActionExport, &QAction::triggered, this, &MainWindow::onExportTexture );
}

void MainWindow::setCurrentTXD( rw::TexDictionary *txdObj )
{
    if ( this->currentTXD == txdObj )
        return;

    if ( this->currentTXD != NULL )
    {
        // Make sure we have no more texture in our viewport.
		clearViewImage();

        this->currentSelectedTexture = NULL;

        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = NULL;

        // Clear anything in the GUI that represented the previous TXD.
        this->textureListWidget->clear();
    }

    if ( txdObj != NULL )
    {
        this->currentTXD = txdObj;

        this->updateTextureList();
    }
}

void MainWindow::updateTextureList( void )
{
    rw::TexDictionary *txdObj = this->currentTXD;

    QListWidget *listWidget = this->textureListWidget;

    listWidget->clear();
    
    if ( txdObj )
    {
	    bool selected = false;

	    for ( rw::TexDictionary::texIter_t iter( txdObj->GetTextureIterator() ); iter.IsEnd() == false; iter.Increment() )
	    {
            rw::TextureBase *texItem = iter.Resolve();

	        QListWidgetItem *item = new QListWidgetItem;
	        listWidget->addItem(item);
	        listWidget->setItemWidget(item, new TexInfoWidget(item, texItem) );
		    item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), 54));
		    // select first item in a list
		    if (!selected)
		    {
			    item->setSelected(true);
			    selected = true;
		    }
	    }

        // We have no more selected texture item.
        this->currentSelectedTexture = NULL;
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

void MainWindow::onOpenFile( bool checked )
{
	this->txdLog->beforeTxdLoading();

    QString fileName = QFileDialog::getOpenFileName( this, tr( "Open TXD file..." ), QString(), tr( "RW Texture Archive (*.txd);;Any File (*.*)" ) );

    if ( fileName.length() != 0 )
    {
        // We got a file name, try to load that TXD file into our editor.
        std::wstring unicodeFileName = fileName.toStdWString();

        rw::streamConstructionFileParamW_t fileOpenParam( unicodeFileName.c_str() );

        rw::Stream *txdFileStream = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &fileOpenParam );

        // If the opening succeeded, process things.
        if ( txdFileStream )
        {
            this->txdLog->addLogMessage( QString( "loading TXD: " ) + fileName );

            // Parse the input file.
            rw::RwObject *parsedObject = NULL;

            try
            {
                parsedObject = this->rwEngine->Deserialize( txdFileStream );
            }
            catch( rw::RwException& except )
            {
				this->txdLog->showError(QString("failed to load the TXD archive: %1").arg(QString::fromStdString(except.message)));
            }

            if ( parsedObject )
            {
                // Try to cast it to a TXD. If it fails we did not get a TXD.
                rw::TexDictionary *newTXD = rw::ToTexDictionary( this->rwEngine, parsedObject );

                if ( newTXD )
                {
                    // Okay, we got a new TXD.
                    // Set it as our current object in the editor.
                    this->setCurrentTXD( newTXD );

                    this->setCurrentFilePath( fileName );
                }
                else
                {
                    const char *objTypeName = this->rwEngine->GetObjectTypeName( parsedObject );

                    this->txdLog->addLogMessage( QString( "found %1 but expected a texture dictionary" ).arg( objTypeName ), LOGMSG_WARNING );

                    // Get rid of the object that is not a TXD.
                    this->rwEngine->DeleteRwObject( parsedObject );
                }
            }
            // if parsedObject is NULL, the RenderWare implementation should have error'ed us already.

            // Remember to close the stream again.
            this->rwEngine->DeleteStream( txdFileStream );
        }
    }

	this->txdLog->afterTxdLoading();
}

void MainWindow::onCloseCurrent( bool checked )
{
    this->currentSelectedTexture = NULL;

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

    this->currentSelectedTexture = texItem;

    this->updateTextureView();
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
		imageWidget->setStyleSheet("background-image: url(\"resources/viewBackground.png\");");
	else
		imageWidget->setStyleSheet("background-color: rgba(255, 255, 255, 0);");
}

void MainWindow::onToggleShowLog( bool checked )
{
    // Make sure the log is visible.
	this->txdLog->show();
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
        QString newSaveLocation = QFileDialog::getSaveFileName( this, "Save TXD as...", QString(), tr( "RW Texture Dictionary (*.txd)" ) );

        if ( newSaveLocation.length() != 0 )
        {
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
    rw::Raster *newRaster = params.raster;

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
                newTexture->SetName( params.texName.c_str() );
                newTexture->SetMaskName( params.maskName.c_str() );

                // Now put it into the TXD.
                newTexture->AddToDictionary( currentTXD );

                // Update the texture list.
                this->updateTextureList();
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

        imgExtensionSelect += QString( "*." ) + QString( entry.defaultExt.c_str() ).toLower();

        hasExtEntry = true;
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

        imgExtensionSelect += QString( entry.formatName.c_str() ) + QString( " (*." ) + QString( entry.defaultExt.c_str() ).toLower() + QString( ")" );

        hasEntry = true;
    }

    // Add any file.
    if ( hasEntry )
    {
        imgExtensionSelect += ";;";
    }

    imgExtensionSelect += "Any file (*.*)";

    hasEntry = true;

    return QFileDialog::getOpenFileName( this, "Import Texture...", QString(), imgExtensionSelect );
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

            texAddTask->move( 200, 250 );
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
                // Replace stuff.
                rw::TextureBase *tex = curSelTexItem->GetTextureHandle();

                // We have to update names.
                tex->SetName( params.texName.c_str() );
                tex->SetMaskName( params.maskName.c_str() );
                
                // Update raster handle.
                tex->SetRaster( params.raster );

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
        if ( this->textureListWidget->count() == 0 )
        {
            this->clearViewImage();
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
            // Update the stored raster.
            rw::TextureBase *tex = curSelTexItem->GetTextureHandle();

            // Update names.
            tex->SetName( params.texName.c_str() );
            tex->SetMaskName( params.maskName.c_str() );

            // Replace raster handle.
            tex->SetRaster( params.raster );

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
                const QString& exportFunction = senderAction->defaultExt;
                const QString& formatName = senderAction->formatName;

                std::string ansiExportFunction = exportFunction.toStdString();

                const QString actualExt = exportFunction.toLower();
            
                // Construct a default filename for the object.
                QString defaultFileName = QString( texHandle->GetName().c_str() ) + "." + actualExt;

                // Request a filename and do the export.
                QString finalFilePath =
                    QFileDialog::getSaveFileName(
                        this, QString( "Save " ) + exportFunction + QString( " as..." ), defaultFileName,
                        formatName + " (*." + actualExt + ")"
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
                            // Fetch a bitmap and serialize it.
                            rw::Raster *texRaster = texHandle->GetRaster();

                            if ( texRaster )
                            {
                                serializeRaster( imageStream, texRaster, ansiExportFunction.c_str() );
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

void MainWindow::clearViewImage()
{
	imageWidget->clear();
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
            rw::ConvertRasterTo( texRaster, platform );
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