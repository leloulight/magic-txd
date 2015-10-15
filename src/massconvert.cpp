#include "mainwindow.h"

#include "massconvert.h"

#include "qtinteroputils.hxx"

#include <QCoreApplication>

#include "qtsharedlogic.h"

massconvEnvRegister_t massconvEnvRegister;

void InitializeMassconvToolEnvironment( void )
{
    massconvEnvRegister.RegisterPlugin( mainWindowFactory );
}

void massconvEnv::Shutdown( MainWindow *mainWnd )
{
    // Make sure all open dialogs are closed.
    while ( !LIST_EMPTY( openDialogs.root ) )
    {
        MassConvertWindow *wnd = LIST_GETITEM( MassConvertWindow, openDialogs.root.next, node );

        delete wnd;
    }
}

struct platformToNatural
{
    TxdGenModule::eTargetPlatform mode;
    QString natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <platformToNatural> platformToNaturalList_t;

static platformToNaturalList_t platformToNaturalList =
{
    { TxdGenModule::PLATFORM_PC, "PC" },
    { TxdGenModule::PLATFORM_PS2, "PS2" },
    { TxdGenModule::PLATFORM_XBOX, "XBOX" },
    { TxdGenModule::PLATFORM_DXT_MOBILE, "S3TC mobile" },
    { TxdGenModule::PLATFORM_PVR, "PowerVR" },
    { TxdGenModule::PLATFORM_ATC, "AMD TC" },
    { TxdGenModule::PLATFORM_UNC_MOBILE, "uncomp. mobile" }
};

struct gameToNatural
{
    TxdGenModule::eTargetGame mode;
    QString natural;

    inline bool operator == ( const decltype( mode )& right ) const
    {
        return ( right == this->mode );
    }

    inline bool operator == ( const QString& right ) const
    {
        return ( right == this->natural );
    }
};

typedef naturalModeList <gameToNatural> gameToNaturalList_t;

static gameToNaturalList_t gameToNaturalList =
{
    { TxdGenModule::GAME_GTA3, "GTA III" },
    { TxdGenModule::GAME_GTAVC, "GTA VC" },
    { TxdGenModule::GAME_GTASA, "GTA SA" },
    { TxdGenModule::GAME_MANHUNT, "Manhunt" },
    { TxdGenModule::GAME_BULLY, "Bully" }
};

MassConvertWindow::MassConvertWindow( MainWindow *mainwnd ) : QDialog( mainwnd )
{
    massconvEnv *massconv = massconvEnvRegister.GetPluginStruct( mainwnd );

    this->mainwnd = mainwnd;

    this->setWindowTitle( "Mass Conversion" );
    this->setAttribute( Qt::WA_DeleteOnClose );

    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    // Display lots of stuff.
    QHBoxLayout *mainLayout = new QHBoxLayout( this );

    mainLayout->setSizeConstraint( QLayout::SetFixedSize );

    QVBoxLayout *rootLayout = new QVBoxLayout();

    mainLayout->addLayout( rootLayout );

    QFormLayout *basicPathForm = new QFormLayout();

    QLayout *gameRootLayout = qtshared::createPathSelectGroup( QString::fromStdWString( massconv->txdgenConfig.c_gameRoot ), this->editGameRoot );
    QLayout *outputRootLayout = qtshared::createPathSelectGroup( QString::fromStdWString( massconv->txdgenConfig.c_outputRoot ), this->editOutputRoot );

    basicPathForm->addRow( new QLabel( "Game root:" ), gameRootLayout );
    basicPathForm->addRow( new QLabel( "Output root: " ), outputRootLayout );

    rootLayout->addLayout( basicPathForm );

    // Now a target format selection group.
    QHBoxLayout *platformGroup = new QHBoxLayout();

    platformGroup->addWidget( new QLabel( "Platform:" ) );

    QComboBox *platformSelBox = new QComboBox();

    // We have a fixed list of platforms here.
    for ( const platformToNatural& nat : platformToNaturalList )
    {
        platformSelBox->addItem( nat.natural );
    }

    this->selPlatformBox = platformSelBox;

    // Select current.
    platformToNaturalList.selectCurrent( platformSelBox, massconv->txdgenConfig.c_targetPlatform );

    platformGroup->addWidget( platformSelBox );

    rootLayout->addLayout( platformGroup );

    QHBoxLayout *gameGroup = new QHBoxLayout();

    gameGroup->addWidget( new QLabel( "Game:" ) );

    QComboBox *gameSelBox = new QComboBox();

    // Add a fixed list of known games.
    for ( const gameToNatural& nat : gameToNaturalList )
    {
        gameSelBox->addItem( nat.natural );
    }

    this->selGameBox = gameSelBox;

    gameToNaturalList.selectCurrent( gameSelBox, massconv->txdgenConfig.c_gameType );

    gameGroup->addWidget( gameSelBox );

    rootLayout->addLayout( gameGroup );

    // INVASION OF CHECKBOXES.
    QCheckBox *propClearMipmaps = new QCheckBox( "Clear mipmaps" );

    propClearMipmaps->setChecked( massconv->txdgenConfig.c_clearMipmaps );

    this->propClearMipmaps = propClearMipmaps;

    rootLayout->addWidget( propClearMipmaps );

    QHBoxLayout *genMipGroup = new QHBoxLayout();

    QCheckBox *propGenMipmaps = new QCheckBox( "Generate mipmaps" );

    propGenMipmaps->setChecked( massconv->txdgenConfig.c_generateMipmaps );

    this->propGenMipmaps = propGenMipmaps;

    genMipGroup->addWidget( propGenMipmaps );

    QHBoxLayout *mipMaxLevelGroup = new QHBoxLayout();

    mipMaxLevelGroup->setAlignment( Qt::AlignRight );

    mipMaxLevelGroup->addWidget( new QLabel( "Max:" ), 0, Qt::AlignRight );

    QLineEdit *maxMipLevelEdit = new QLineEdit( QString( "%1" ).arg( massconv->txdgenConfig.c_mipGenMaxLevel ) );

    QIntValidator *maxMipLevelVal = new QIntValidator( 0, 32, this );

    maxMipLevelEdit->setValidator( maxMipLevelVal );

    this->propGenMipmapsMax = maxMipLevelEdit;

    maxMipLevelEdit->setMaximumWidth( 40 );

    mipMaxLevelGroup->addWidget( maxMipLevelEdit );

    genMipGroup->addLayout( mipMaxLevelGroup );

    rootLayout->addLayout( genMipGroup );

    QCheckBox *propImproveFiltering = new QCheckBox( "Improve filtering" );

    propImproveFiltering->setChecked( massconv->txdgenConfig.c_improveFiltering );

    this->propImproveFiltering = propImproveFiltering;

    rootLayout->addWidget( propImproveFiltering );

    QCheckBox *propCompress = new QCheckBox( "Compress textures" );

    propCompress->setChecked( massconv->txdgenConfig.compressTextures );

    this->propCompressTextures = propCompress;

    rootLayout->addWidget( propCompress );

    QCheckBox *propReconstructIMG = new QCheckBox( "Reconstruct IMG archives" );

    propReconstructIMG->setChecked( massconv->txdgenConfig.c_reconstructIMGArchives );

    this->propReconstructIMG = propReconstructIMG;

    rootLayout->addWidget( propReconstructIMG );

    QCheckBox *propCompressedIMG = new QCheckBox( "Compressed IMG" );

    propCompressedIMG->setChecked( massconv->txdgenConfig.c_imgArchivesCompressed );

    this->propCompressedIMG = propCompressedIMG;

    rootLayout->addWidget( propCompressedIMG );

    // On the right we want to give messages to the user and the actual buttons to start things.
    QVBoxLayout *logPaneGroup = new QVBoxLayout();

    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setAlignment( Qt::AlignCenter );

    QPushButton *buttonConvert = new QPushButton( "Convert" );

    this->buttonConvert = buttonConvert;

    connect( buttonConvert, &QPushButton::clicked, this, &MassConvertWindow::OnRequestConvert );

    buttonRow->addWidget( buttonConvert, 0, Qt::AlignCenter );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );

    connect( buttonCancel, &QPushButton::clicked, this, &MassConvertWindow::OnRequestCancel );

    buttonRow->addWidget( buttonCancel, 0, Qt::AlignCenter );

    logPaneGroup->addLayout( buttonRow );

    // Add a log.
    QPlainTextEdit *logEdit = new QPlainTextEdit();

    logEdit->setMinimumWidth( 450 );
    logEdit->setReadOnly( true );

    this->logEdit = logEdit;

    logPaneGroup->addWidget( logEdit );

    mainLayout->addLayout( logPaneGroup );

    rw::Interface *rwEngine = mainwnd->GetEngine();

    this->convConsistencyLock = rw::CreateReadWriteLock( rwEngine );

    this->conversionThread = NULL;

    LIST_INSERT( massconv->openDialogs.root, this->node );
}

MassConvertWindow::~MassConvertWindow( void )
{
    LIST_REMOVE( this->node );

    // Make sure we finished the thread.
    rw::Interface *rwEngine = this->mainwnd->GetEngine();

    bool hasLocked = true;

    this->convConsistencyLock->enter_read();

    if ( rw::thread_t convThread = this->conversionThread )
    {
        convThread = rw::AcquireThread( rwEngine, convThread );

        this->convConsistencyLock->leave_read();

        hasLocked = false;

        rw::TerminateThread( rwEngine, convThread );

        rw::CloseThread( rwEngine, convThread );
    }

    if ( hasLocked )
    {
        this->convConsistencyLock->leave_read();
    }

    // Kill the lock.
    rw::CloseReadWriteLock( rwEngine, this->convConsistencyLock );

    this->serialize();
}

void MassConvertWindow::serialize( void )
{
    // We should serialize ourselves.
    massconvEnv *massconv = massconvEnvRegister.GetPluginStruct( this->mainwnd );

    // game version.
    gameToNaturalList.getCurrent( this->selGameBox, massconv->txdgenConfig.c_gameType );

    massconv->txdgenConfig.c_outputRoot = this->editOutputRoot->text().toStdWString();
    massconv->txdgenConfig.c_gameRoot = this->editGameRoot->text().toStdWString();

    platformToNaturalList.getCurrent( this->selPlatformBox, massconv->txdgenConfig.c_targetPlatform );

    massconv->txdgenConfig.c_clearMipmaps = this->propClearMipmaps->isChecked();
    massconv->txdgenConfig.c_generateMipmaps = this->propGenMipmaps->isChecked();
    massconv->txdgenConfig.c_mipGenMaxLevel = this->propGenMipmapsMax->text().toInt();
    massconv->txdgenConfig.c_improveFiltering = this->propImproveFiltering->isChecked();
    massconv->txdgenConfig.compressTextures = this->propCompressTextures->isChecked();
    massconv->txdgenConfig.c_reconstructIMGArchives = this->propReconstructIMG->isChecked();
    massconv->txdgenConfig.c_imgArchivesCompressed = this->propCompressedIMG->isChecked();
}

struct AppendConsoleMessageEvent : public QEvent
{
    inline AppendConsoleMessageEvent( QString msg ) : QEvent( QEvent::User )
    {
        this->msg = msg;
    }

    QString msg;
};

struct ConversionFinishEvent : public QEvent
{
    inline ConversionFinishEvent( void ) : QEvent( QEvent::User )
    {}
};

void MassConvertWindow::postLogMessage( QString msg )
{
    AppendConsoleMessageEvent *evt = new AppendConsoleMessageEvent( msg );

    QCoreApplication::postEvent( this, evt );
}

struct MassConvertTxdGenModule : public TxdGenModule
{
    MassConvertWindow *massconvWnd;

    inline MassConvertTxdGenModule( MassConvertWindow *massconvWnd, rw::Interface *rwEngine ) : TxdGenModule( rwEngine )
    {
        this->massconvWnd = massconvWnd;
    }

    void OnMessage( const std::string& msg ) override
    {
        massconvWnd->postLogMessage( QString::fromStdString( msg ) );
    }

    void OnMessage( const std::wstring& msg ) override
    {
        massconvWnd->postLogMessage( QString::fromStdWString( msg ) );
    }

    CFile* WrapStreamCodec( CFile *compressed ) override
    {
        return CreateDecompressedStream( massconvWnd->mainwnd, compressed );
    }
};

static void convThreadEntryPoint( rw::thread_t threadHandle, rw::Interface *engineInterface, void *ud )
{
    MassConvertWindow *massconvWnd = (MassConvertWindow*)ud;

    // Get our configuration.
    TxdGenModule::run_config run_cfg;

    if ( massconvEnv *massconv = massconvEnvRegister.GetPluginStruct( massconvWnd->mainwnd ) )
    {
        run_cfg = massconv->txdgenConfig;
    }

    // Any RenderWare configuration that we set on this thread should count for this thread only.
    rw::AssignThreadedRuntimeConfig( engineInterface );

    try
    {
        massconvWnd->postLogMessage( "starting conversion...\n\n" );

        MassConvertTxdGenModule module( massconvWnd, engineInterface );

        module.ApplicationMain( run_cfg );

        // Notify the application that we finished.
        {
            ConversionFinishEvent *evt = new ConversionFinishEvent();

            QCoreApplication::postEvent( massconvWnd, evt );
        }

        massconvWnd->postLogMessage( "\nconversion finished!\n\n" );
    }
    catch( ... )
    {
        // We have to quit normally.
        massconvWnd->postLogMessage( "terminated thread.\n" );
    }

    massconvWnd->convConsistencyLock->enter_write();

    massconvWnd->conversionThread = NULL;

    // Close our handle.
    rw::CloseThread( engineInterface, threadHandle );

    massconvWnd->convConsistencyLock->leave_write();
}

void MassConvertWindow::OnRequestConvert( bool checked )
{
    if ( this->conversionThread )
        return;

    // Update configuration.
    this->serialize();

    // Disable the conversion button, since we cannot run two conversions at the same time in
    // the same window.
    this->buttonConvert->setDisabled( true );

    // Run some the conversion in a seperate thread.
    rw::Interface *rwEngine = this->mainwnd->GetEngine();

    rw::thread_t convThread = rw::MakeThread( rwEngine, convThreadEntryPoint, this );

    this->conversionThread = convThread;

    rw::ResumeThread( rwEngine, convThread );
}

void MassConvertWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void MassConvertWindow::customEvent( QEvent *evt )
{
    if ( AppendConsoleMessageEvent *appendMsgEvt = dynamic_cast <AppendConsoleMessageEvent*> ( evt ) )
    {
        this->logEdit->moveCursor( QTextCursor::End );
        this->logEdit->insertPlainText( appendMsgEvt->msg );
        this->logEdit->moveCursor( QTextCursor::End );

        return;
    }
    else if ( ConversionFinishEvent *convEndEvt = dynamic_cast <ConversionFinishEvent*> ( evt ) )
    {
        // We can enable the conversion button again.
        this->buttonConvert->setDisabled( false );

        return;
    }

    QDialog::customEvent( evt );
}