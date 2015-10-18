#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QEvent>

#include <QCoreApplication>

struct TaskCompletionWindow : public QDialog
{
    friend struct taskCompletionWindowEnv;
    friend class MagicMassExportModule;
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
    TaskCompletionWindow( MainWindow *mainWnd, rw::thread_t taskHandle, QString title, QString statusMsg );
    ~TaskCompletionWindow( void );

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

    RwListEntry <TaskCompletionWindow> node;
};