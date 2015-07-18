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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    rwWarnMan( this )
{
    // Initialize variables.
    this->currentTXD = NULL;
    this->txdNameLabel = NULL;
    this->currentSelectedTexture = NULL;
    this->txdLog = NULL;

    this->drawMipmapLayers = false;
	this->showBackground = false;

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

    try
    {
    
#if 1
        // Test something.
        rw::streamConstructionFileParam_t fileParam( "C:/Users/The_GTA/Desktop/image format samples/png/graybird.png" );

        rw::Stream *imgStream = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE, rw::RWSTREAMMODE_READONLY, &fileParam );

        if ( imgStream )
        {
            rw::Bitmap theBmp;

            bool success = rw::DeserializeImage( imgStream, theBmp );

            if ( success )
            {
#if 0
                // Put this into a new texture.
                rw::Raster *newRaster = rw::CreateRaster( this->rwEngine );

                if ( newRaster )
                {
                    newRaster->newNativeData( "Direct3D9" );

                    newRaster->setImageData( theBmp );

                    // Now lets generate mipmaps for it.
                    newRaster->generateMipmaps( 9 );

                    // Delete it again.
                    rw::DeleteRaster( newRaster );
                }
#endif

#if 1
                // Serialize it again.
                rw::streamConstructionFileParam_t newFileparam( "out_test.png" );

                rw::Stream *outStr = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE, rw::RWSTREAMMODE_CREATE, &newFileparam );

                if ( outStr )
                {
                    // Do it.
                    rw::SerializeImage( outStr, "PNG", theBmp );

                    this->rwEngine->DeleteStream( outStr );
                }
#endif
            }

            this->rwEngine->DeleteStream( imgStream );
        }
#endif

	    /* --- Window --- */
        updateWindowTitle();
        setMinimumSize(380, 300);
	    resize(900, 680);

		/* --- Log --- */
		this->txdLog = new TxdLog(this);

	    /* --- List --- */
	    QListWidget *listWidget = new QListWidget();
	    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
		listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
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
	    txdName->setObjectName("txdName");
	    txdName->setAlignment(Qt::AlignCenter);

        this->txdNameLabel = txdName;

	    QGridLayout *txdNameLayout = new QGridLayout();
	    QLabel *starsBox = new QLabel;
	    QMovie *stars = new QMovie;
	    stars->setFileName("resources\\styles\\stars_blue.gif");
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
	    QAction *actionReplace = new QAction("&Replace", this);
	    editMenu->addAction(actionReplace);
	    QAction *actionRemove = new QAction("&Remove", this);
	    editMenu->addAction(actionRemove);
	    QAction *actionRename = new QAction("&Rename", this);
	    editMenu->addAction(actionRename);
	    QAction *actionResize = new QAction("&Resize", this);
	    editMenu->addAction(actionResize);
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

	    QWidget *hLineBackground = new QWidget();
	    hLineBackground->setFixedHeight(1);
	    hLineBackground->setObjectName("hLineBackground");

	    QVBoxLayout *topLayout = new QVBoxLayout;
	    topLayout->addWidget(txdNameBackground);
	    topLayout->addWidget(txdOptionsBackground);
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

        QListWidget *listWidget = this->textureListWidget;

		bool selected = false;

	    for ( rw::TexDictionary::texIter_t iter( txdObj->GetTextureIterator() ); iter.IsEnd() == false; iter.Increment() )
	    {
            rw::TextureBase *texItem = iter.Resolve();

	        QListWidgetItem *item = new QListWidgetItem;
	        listWidget->addItem(item);
	        listWidget->setItemWidget(item, new TexInfoWidget(texItem) );
		    item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), 54));
			// select first item in a list
			if (!selected)
			{
				item->setSelected(true);
				selected = true;
			}
	    }
    }
}

void MainWindow::updateWindowTitle( void )
{
    QString windowTitleString = tr( "Magic.TXD" );

#ifdef _DEBUG
    windowTitleString += " DEBUG";
#endif

    if ( rw::TexDictionary *txd = this->currentTXD )
    {
        windowTitleString += " (" + QString( this->openedTXDFileInfo.absoluteFilePath() ) + ")";
    }

    setWindowTitle( windowTitleString );

    // Also update the top label.
    if ( this->txdNameLabel )
    {
        if ( rw::TexDictionary *txd = this->currentTXD )
        {
            this->txdNameLabel->setText(
                QString( this->openedTXDFileInfo.fileName() ) + QString( " [rwver: " ) + txd->GetEngineVersion().toString().c_str() + QString( "]" )
            );
        }
        else
        {
            this->txdNameLabel->clear();
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

void MainWindow::onOpenFile( bool checked )
{
	this->txdLog->beforeTxdLoading();

    QString fileName = QFileDialog::getOpenFileName( this, tr( "Open TXD file..." ), QString(), tr( "RW Texture Archive (*.txd)" ) );

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
				this->txdLog->showError(QString("failed to load the TXD archive: %1").arg(except.message.c_str()));
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

                    this->openedTXDFileInfo = QFileInfo( fileName );

                    this->updateWindowTitle();
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

inline QImage convertRWBitmapToQImage( const rw::Bitmap& rasterBitmap )
{
	rw::uint32 width, height;
	rasterBitmap.getSize(width, height);

	QImage texImage(width, height, QImage::Format::Format_ARGB32);

	// Copy scanline by scanline.
	for (int y = 0; y < height; y++)
	{
		uchar *scanLineContent = texImage.scanLine(y);

		QRgb *colorItems = (QRgb*)scanLineContent;

		for (int x = 0; x < width; x++)
		{
			QRgb *colorItem = (colorItems + x);

			unsigned char r, g, b, a;

			rasterBitmap.browsecolor(x, y, r, g, b, a);

			*colorItem = qRgba(r, g, b, a);
		}
	}

    return texImage;
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

                if ( this->drawMipmapLayers )
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
        QString txdFullPath = this->openedTXDFileInfo.absoluteFilePath();

        if ( txdFullPath.length() != 0 )
        {
            this->saveCurrentTXDAt( txdFullPath );
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

                        throw;
                    }

                    // Close the stream again.
                    this->rwEngine->DeleteStream( imageStream );
                }
            }
        }
    }
}

void MainWindow::clearViewImage()
{
	imageWidget->clear();
	imageWidget->hide();
}