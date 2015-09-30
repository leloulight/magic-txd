#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QEvent>

#include <QCoreApplication>

struct TaskCompletionWindow : public QDialog
{
private:
    struct status_msg_update : public QEvent
    {
        inline status_msg_update( QString newMsg ) : QEvent( QEvent::User )
        {
            this->msg = newMsg;
        }

        QString msg;
    };

    struct task_completion_event : public QEvent
    {
        inline task_completion_event( void ) : QEvent( QEvent::User )
        {
            return;
        }
    };

    static void waiterThread_runtime( rw::thread_t handle, rw::Interface *engineInterface, void *ud )
    {
        TaskCompletionWindow *wnd = (TaskCompletionWindow*)ud;

        rw::thread_t taskThreadHandle = wnd->taskThreadHandle;

        // Simply wait for the other task to finish.
        rw::JoinThread( engineInterface, taskThreadHandle );

        // We are done. Notify the window.
        task_completion_event *completeEvt = new task_completion_event();

        QCoreApplication::postEvent( wnd, completeEvt );
    }

public:
    inline TaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, QString title, QString statusMsg ) : QDialog( mainWnd )
    {
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
    }

    inline ~TaskCompletionWindow( void )
    {
        rw::Interface *rwEngine = this->mainWnd->GetEngine();

        // Terminate the task.
        rw::TerminateThread( rwEngine, this->taskThreadHandle );

        // We cannot close the window until the task has finished.
        rw::JoinThread( rwEngine, this->waitThreadHandle );

        rw::CloseThread( rwEngine, this->waitThreadHandle );
        
        // We can safely get rid of the task thread handle.
        rw::CloseThread( rwEngine, this->taskThreadHandle );
    }

    inline void updateStatusMessage( QString newMessage )
    {
        status_msg_update *evt = new status_msg_update( std::move( newMessage ) );

        QCoreApplication::postEvent( this, evt );
    }

    void customEvent( QEvent *evt ) override
    {
        if ( task_completion_event *completeEvt = dynamic_cast <task_completion_event*> ( evt ) )
        {
            // We finished!
            // This means that we can close ourselves.
            this->close();

            return;
        }

        if ( status_msg_update *msgEvt = dynamic_cast <status_msg_update*> ( evt ) )
        {
            // Update our label.
            this->statusMessageLabel->setText( msgEvt->msg );

            return;
        }

        return;
    }

public slots:
    void OnRequestCancel( bool checked )
    {
        rw::Interface *rwEngine = this->mainWnd->GetEngine();

        // Attempt to accelerate the closing of the dialog by terminating the task thread.
        rw::TerminateThread( rwEngine, this->taskThreadHandle, false );
    }

private:
    MainWindow *mainWnd;

    rw::thread_t taskThreadHandle;
    rw::thread_t waitThreadHandle;

    QLabel *statusMessageLabel;
};