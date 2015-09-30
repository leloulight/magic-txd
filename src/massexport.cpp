#include "mainwindow.h"
#include "massexport.h"

#include "qtsharedlogic.h"

#include "taskcompletionwindow.h"

massexportEnvRegister_t massexportEnvRegister;

void massexportEnv::Shutdown( MainWindow *mainWnd )
{
    // Make sure all open dialogs are closed.
    while ( !LIST_EMPTY( openDialogs.root ) )
    {
        MassExportWindow *wnd = LIST_GETITEM( MassExportWindow, openDialogs.root.next, node );

        delete wnd;
    }
}

MassExportWindow::MassExportWindow( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    // We want a dialog similar to the Mass Export one but it should be without a log.
    // Instead, we will launch a task completion window that will handle our task.

    this->mainWnd = mainWnd;

    massexportEnv *env = massexportEnvRegister.GetPluginStruct( mainWnd );

    rw::Interface *rwEngine = mainWnd->GetEngine();

    this->setWindowTitle( "Mass Exporting" );
    this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    this->setAttribute( Qt::WA_DeleteOnClose );

    // Just one pane with things to edit.
    // First we should put the paths to perform the extraction.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    rootLayout->setSizeConstraint( QLayout::SetFixedSize );

    rootLayout->setSizeConstraint( QLayout::SetFixedSize );

    QFormLayout *pathRootForm = new QFormLayout();

    pathRootForm->addRow(
        new QLabel( "Game root:" ),
        qtshared::createPathSelectGroup( QString::fromStdWString( env->config.gameRoot ), this->editGameRoot )
    );

    QHBoxLayout *outputRootGroup = new QHBoxLayout();

    pathRootForm->addRow(
        new QLabel( "Output root:" ),
        qtshared::createPathSelectGroup( QString::fromStdWString( env->config.outputRoot ), this->editOutputRoot )
   );

    rootLayout->addLayout( pathRootForm );

    // The other most important thing is to select a target image format.
    // This is pretty easy.
    QHBoxLayout *imgFormatGroup = new QHBoxLayout();

    imgFormatGroup->setContentsMargins( 0, 0, 0, 10 );

    imgFormatGroup->addWidget( new QLabel( "Image format:" ) );

    QComboBox *boxRecomImageFormat = new QComboBox();
    {
        // We fill it with base formats.
        // Those formats are available to all native texture types.
        rw::registered_image_formats_t formats;
        rw::GetRegisteredImageFormats( rwEngine, formats );

        for ( const rw::registered_image_format& format : formats )
        {
            boxRecomImageFormat->addItem( QString( format.defaultExt ).toUpper() );
        }
    }

    // If the remembered format exists in our combo box, select it.
    {
        int existFormatIndex = boxRecomImageFormat->findText( QString::fromStdString( env->config.recImgFormat ), Qt::MatchExactly );

        if ( existFormatIndex != -1 )
        {
            boxRecomImageFormat->setCurrentIndex( existFormatIndex );
        }
    }
    
    this->boxRecomImageFormat = boxRecomImageFormat;

    imgFormatGroup->addWidget( boxRecomImageFormat );

    rootLayout->addLayout( imgFormatGroup );

    // Textures can be extracted in multiple modes, depending on how the user likes it best.
    // The plain mode extracts textures simply by their texture name into the location of the TXD.
    // The TXD name mode works differently: TXD name + _ + texture name; into the location of the TXD.
    // The folders mode extracts textures into folders that are called after the TXD, at the location of the TXD.

    MassExportModule::eOutputType outputType = env->config.outputType;

    QRadioButton *optionExportPlain = new QRadioButton( "with texture name only" );

    this->optionExportPlain = optionExportPlain;

    if ( outputType == MassExportModule::OUTPUT_PLAIN )
    {
        optionExportPlain->setChecked( true );
    }

    rootLayout->addWidget( optionExportPlain );

    QRadioButton *optionExportTXDName = new QRadioButton( "pre-prended with TXD name" );

    this->optionExportTXDName = optionExportTXDName;

    if ( outputType == MassExportModule::OUTPUT_TXDNAME )
    {
        optionExportTXDName->setChecked( true );
    }

    rootLayout->addWidget( optionExportTXDName );

    QRadioButton *optionExportFolders = new QRadioButton( "in seperate folders" );

    this->optionExportFolders = optionExportFolders;

    if ( outputType == MassExportModule::OUTPUT_FOLDERS )
    {
        optionExportFolders->setChecked( true );
    }

    rootLayout->addWidget( optionExportFolders );

    // The dialog is done, we finish off with the typical buttons.
    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setContentsMargins( 0, 15, 0, 0 );

    QPushButton *buttonExport = new QPushButton( "Export" );

    connect( buttonExport, &QPushButton::clicked, this, &MassExportWindow::OnRequestExport );

    buttonRow->addWidget( buttonExport );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );

    connect( buttonCancel, &QPushButton::clicked, this, &MassExportWindow::OnRequestCancel );

    buttonRow->addWidget( buttonCancel );

    rootLayout->addLayout( buttonRow );

    this->setLayout( rootLayout );

    LIST_INSERT( env->openDialogs.root, this->node );

    // Finito.
}

MassExportWindow::~MassExportWindow( void )
{
    LIST_REMOVE( this->node );
}

struct MagicMassExportModule : public MassExportModule
{
    inline MagicMassExportModule( rw::Interface *rwEngine, TaskCompletionWindow *wnd ) : MassExportModule( rwEngine )
    {
        this->wnd = wnd;
    }

    void OnProcessingFile( const std::wstring& fileName ) override
    {
        wnd->updateStatusMessage( QString( "processing: " ) + QString::fromStdWString( fileName ) + QString( " ..." ) );
    }

private:
    TaskCompletionWindow *wnd;
};

struct exporttask_params
{
    TaskCompletionWindow *taskWnd;
    MassExportModule::run_config config;
};

static void exporttask_runtime( rw::thread_t handle, rw::Interface *engineInterface, void *ud )
{
    exporttask_params *params = (exporttask_params*)ud;

    try
    {
        // We want our own configuration.
        rw::AssignThreadedRuntimeConfig( engineInterface );

        // Warnings should be ignored.
        engineInterface->SetWarningLevel( 0 );
        engineInterface->SetWarningManager( NULL );

        // Run the application.
        MagicMassExportModule module( engineInterface, params->taskWnd );

        module.ApplicationMain( params->config );
    }
    catch( ... )
    {
        delete params;

        throw;
    }

    // Release the configuration that has been given to us.
    delete params;
}

void MassExportWindow::OnRequestExport( bool checked )
{
    // Store our configuration.
    this->serialize();

    // Launch the task.
    {
        rw::Interface *engineInterface = this->mainWnd->GetEngine();

        // Allocate a private configuration for the task.
        const massexportEnv *env = massexportEnvRegister.GetConstPluginStruct( mainWnd );
        
        exporttask_params *params = new exporttask_params();
        params->config = env->config;
        params->taskWnd = NULL;

        rw::thread_t taskHandle = rw::MakeThread( engineInterface, exporttask_runtime, params );

        // Create a window that is responsible of it.
        TaskCompletionWindow *taskWnd = new TaskCompletionWindow( this->mainWnd, taskHandle, "Exporting...", "calculating prime numbers." );

        params->taskWnd = taskWnd;

        rw::ResumeThread( engineInterface, taskHandle );

        taskWnd->setVisible( true );
    }

    this->close();
}

void MassExportWindow::OnRequestCancel( bool checked )
{
    this->close();
}

void MassExportWindow::serialize( void )
{
    massexportEnv *env = massexportEnvRegister.GetPluginStruct( this->mainWnd );

    env->config.gameRoot = this->editGameRoot->text().toStdWString();
    env->config.outputRoot = this->editOutputRoot->text().toStdWString();
    env->config.recImgFormat = this->boxRecomImageFormat->currentText().toStdString();
    
    MassExportModule::eOutputType outputType = MassExportModule::OUTPUT_TXDNAME;

    if ( this->optionExportPlain->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_PLAIN;
    }
    else if ( this->optionExportTXDName->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_TXDNAME;
    }
    else if ( this->optionExportFolders->isChecked() )
    {
        outputType = MassExportModule::OUTPUT_FOLDERS;
    }

    env->config.outputType = outputType;
}

void InitializeMassExportToolEnvironment( void )
{
    massexportEnvRegister.RegisterPlugin( mainWindowFactory );
}