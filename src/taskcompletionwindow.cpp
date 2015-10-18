#include "mainwindow.h"
#include "taskcompletionwindow.h"

#include <sdk/PluginHelpers.h>

struct taskCompletionWindowEnv
{
    inline void Initialize( MainWindow *mainWnd )
    {
        LIST_CLEAR( this->windows.root );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        while ( !LIST_EMPTY( this->windows.root ) )
        {
            TaskCompletionWindow *wnd = LIST_GETITEM( TaskCompletionWindow, this->windows.root.next, node );

            delete wnd;
        }
    }

    RwList <TaskCompletionWindow> windows;
};

static PluginDependantStructRegister <taskCompletionWindowEnv, mainWindowFactory_t> taskCompletionWindowEnvRegister;

TaskCompletionWindow::TaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, QString title, QString statusMsg ) : QDialog( mainWnd )
{
    taskCompletionWindowEnv *env = taskCompletionWindowEnvRegister.GetPluginStruct( mainWnd );

    rw::Interface *rwEngine = mainWnd->GetEngine();

    this->setWindowTitle( title );
    this->setWindowFlags( this->windowFlags() & ( ~Qt::WindowContextHelpButtonHint & ~Qt::WindowCloseButtonHint ) );

    this->setAttribute( Qt::WA_DeleteOnClose );

    this->mainWnd = mainWnd;

    this->taskThreadHandle = taskHandle;

    // We need a waiter thread that will notify us of the task completion.
    this->waitThreadHandle = rw::MakeThread( rwEngine, waiterThread_runtime, this );

    rw::ResumeThread( rwEngine, this->waitThreadHandle );

    // This dialog should consist of a status message and a cancel button.
    QVBoxLayout *rootLayout = new QVBoxLayout();

    QLabel *statusMessageLabel = new QLabel( statusMsg );

    statusMessageLabel->setAlignment( Qt::AlignCenter );

    this->statusMessageLabel = statusMessageLabel;

    rootLayout->addWidget( statusMessageLabel );

    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setAlignment( Qt::AlignCenter );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );

    buttonCancel->setMaximumWidth( 90 );

    connect( buttonCancel, &QPushButton::clicked, this, &TaskCompletionWindow::OnRequestCancel );

    buttonRow->addWidget( buttonCancel );

    rootLayout->addLayout( buttonRow );

    this->setLayout( rootLayout );

    this->setMinimumWidth( 350 );

    LIST_INSERT( env->windows.root, this->node );
}

TaskCompletionWindow::~TaskCompletionWindow( void )
{
    // Remove us from the registry.
    LIST_REMOVE( this->node );

    rw::Interface *rwEngine = this->mainWnd->GetEngine();

    // Terminate the task.
    rw::TerminateThread( rwEngine, this->taskThreadHandle );

    // We cannot close the window until the task has finished.
    rw::JoinThread( rwEngine, this->waitThreadHandle );

    rw::CloseThread( rwEngine, this->waitThreadHandle );
        
    // We can safely get rid of the task thread handle.
    rw::CloseThread( rwEngine, this->taskThreadHandle );
}

void InitializeTaskCompletionWindowEnv( void )
{
    taskCompletionWindowEnvRegister.RegisterPlugin( mainWindowFactory );
}