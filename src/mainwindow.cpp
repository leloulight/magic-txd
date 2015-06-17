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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Initialize variables.
    this->currentTXD = NULL;

    // Initialize the RenderWare engine.
    rw::LibraryVersion engineVersion;

    // This engine version is the default version we create resources in.
    // Resources can change their version at any time, so we do not have to change this.
    engineVersion.rwLibMajor = 3;
    engineVersion.rwLibMinor = 6;
    engineVersion.rwRevMajor = 0;
    engineVersion.rwRevMinor = 4;

    this->rwEngine = rw::CreateEngine( engineVersion );

    if ( this->rwEngine == NULL )
    {
        throw std::exception( "failed to initialize the RenderWare engine" );
    }

    // Set some typical engine properties.
    this->rwEngine->SetIgnoreSerializationBlockRegions( true );

    try
    {
	    /* --- Window --- */
        setWindowTitle(tr("Magic.TXD (test)"));
        setMinimumSize(380, 300);
	    resize(900, 680);

	    /* --- List --- */
	    QListWidget *listWidget = new QListWidget();
	    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

        this->connect( listWidget, &QListWidget::itemClicked, this, &MainWindow::onTextureItemSelected );

        // We will store all our texture names in this.
        this->textureListWidget = listWidget;

	    /* --- Viewport --- */
	    TexViewportWidget *textureViewBackground = new TexViewportWidget();
	    textureViewBackground->setObjectName("textureViewBackground");

        // We shall render our things into this.
        this->textureViewport = textureViewBackground;

	    /* --- Splitter --- */
	    QSplitter *splitter = new QSplitter;
	    splitter->addWidget(listWidget);
	    splitter->addWidget(textureViewBackground);
	    QList<int> sizes;
	    sizes.push_back(200);
	    sizes.push_back(splitter->size().width() - 200);
	    splitter->setSizes(sizes);
	    splitter->setChildrenCollapsible(false);

	    /* --- Top panel --- */
	    QWidget *txdNameBackground = new QWidget();
	    txdNameBackground->setFixedHeight(60);
	    txdNameBackground->setObjectName("txdNameBackground");
	    QLabel *txdName = new QLabel("effectsPC.txd");
	    txdName->setObjectName("txdName");
	    txdName->setAlignment(Qt::AlignCenter);

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
	    QMenu *fileMenu = menu->addMenu(tr("File"));
	    QAction *actionOpen = new QAction("&Open", this);
	    fileMenu->addAction(actionOpen);

        this->connect( actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile );

	    QAction *actionSave = new QAction("&Save", this);
	    fileMenu->addAction(actionSave);
	    QAction *actionSaveAs = new QAction("&Save as...", this);
	    fileMenu->addAction(actionSaveAs);
	    QAction *closeCurrent = new QAction("&Close current", this);
	    fileMenu->addAction(closeCurrent);
	    fileMenu->addSeparator();

        this->connect( closeCurrent, &QAction::triggered, this, &MainWindow::onCloseCurrent );

	    QAction *actionQuit = new QAction("&Quit", this);
	    fileMenu->addAction(actionQuit);

	    QMenu *editMenu = menu->addMenu(tr("Edit"));
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

	    QMenu *exportMenu = menu->addMenu(tr("Export"));
	    QAction *actionExportPNG = new QAction("&PNG", this);
	    exportMenu->addAction(actionExportPNG);
	    QAction *actionExportDDS = new QAction("&DDS", this);
	    exportMenu->addAction(actionExportDDS);
	    QAction *actionExportBMP = new QAction("&BMP", this);
	    exportMenu->addAction(actionExportBMP);
	    QAction *actionExportTTXD = new QAction("&Text-based TXD", this);
	    exportMenu->addAction(actionExportTTXD);
	    exportMenu->addSeparator();
	    QAction *actionExportAll = new QAction("&Export all", this);
	    exportMenu->addAction(actionExportAll);

	    QMenu *viewMenu = menu->addMenu(tr("View"));
	    QAction *actionBackground = new QAction("&Background", this);
	    viewMenu->addAction(actionBackground);
	    QAction *action3dView = new QAction("&3D View", this);
	    viewMenu->addAction(action3dView);
	    QAction *actionShowMipLevels = new QAction("&Display mip-levels", this);
	    viewMenu->addAction(actionShowMipLevels);
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

    // Destroy the engine again.
    rw::DeleteEngine( this->rwEngine );

    this->rwEngine = NULL;
}

void MainWindow::setCurrentTXD( rw::TexDictionary *txdObj )
{
    if ( this->currentTXD != NULL )
    {
        // Make sure we have no more texture in our viewport.
        this->textureViewport->setTextureHandle( NULL );

        this->rwEngine->DeleteRwObject( this->currentTXD );

        this->currentTXD = NULL;

        // Clear anything in the GUI that represented the previous TXD.
        this->textureListWidget->clear();
    }

    if ( txdObj != NULL )
    {
        this->currentTXD = txdObj;

        QListWidget *listWidget = this->textureListWidget;

	    for ( rw::TexDictionary::texIter_t iter( txdObj->GetTextureIterator() ); iter.IsEnd() == false; iter.Increment() )
	    {
            rw::TextureBase *texItem = iter.Resolve();

	        QListWidgetItem *item = new QListWidgetItem;
	        listWidget->addItem(item);
	        listWidget->setItemWidget(item, new TexInfoWidget(texItem) );
		    item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), 54));
	    }
    }
}

void MainWindow::onOpenFile( bool checked )
{
    QString fileName = QFileDialog::getOpenFileName( this, tr( "Open TXD file..." ), QString(), tr( "RW Texture Archive (*.txd)" ) );

    if ( fileName.length() != 0 )
    {
        // We got a file name, try to load that TXD file into our editor.
        std::string ansiFileName = fileName.toStdString().c_str();

        rw::streamConstructionFileParam_t fileOpenParam( ansiFileName.c_str() );

        rw::Stream *txdFileStream = this->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE, rw::RWSTREAMMODE_READONLY, &fileOpenParam );

        // If the opening succeeded, process things.
        if ( txdFileStream )
        {
            // Parse the input file.
            rw::RwObject *parsedObject = NULL;

            try
            {
                parsedObject = this->rwEngine->Deserialize( txdFileStream );
            }
            catch( rw::RwException& except )
            {
                // TODO: display this error to the user.
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
                }
            }

            // Remember to close the stream again.
            this->rwEngine->DeleteStream( txdFileStream );
        }
    }
}

void MainWindow::onCloseCurrent( bool checked )
{
    // Make sure we got no TXD active.
    this->setCurrentTXD( NULL );
}

void MainWindow::onTextureItemSelected( QListWidgetItem *listItem )
{
    QListWidget *texListWidget = this->textureListWidget;

    QWidget *listItemWidget = texListWidget->itemWidget( listItem );

    TexInfoWidget *texItem = dynamic_cast <TexInfoWidget*> ( listItemWidget );

    if ( texItem != NULL )
    {
        // Get the actual texture we are associated with and present it on the output pane.
        rw::TextureBase *theTexture = texItem->GetTextureHandle();

        // Put it to render in our viewport.
        this->textureViewport->setTextureHandle( theTexture );
    }
}