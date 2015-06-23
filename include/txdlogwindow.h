#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QPainter>
#include <QRect>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <fstream>

#include <ctime>

enum eLogMessageType
{
    LOGMSG_INFO,
    LOGMSG_WARNING,
    LOGMSG_ERROR
};

class TxdLogWindow : public QDockWidget
{
    struct tableWidgetItemTypeColumn : public QTableWidgetItem
    {
        eLogMessageType msgType;
    };

    struct abstractItemDelegateIconColumn : public QAbstractItemDelegate
    {
        TxdLogWindow *owner;

        void paint( QPainter *painter, const QStyleOptionViewItem& viewSettings, const QModelIndex& itemIndex ) const override
        {
            QTableWidget *tableWidget = this->owner->txdLogWidget;

            // Attempt to get the message type.
            eLogMessageType msgType = LOGMSG_INFO;
            {
                tableWidgetItemTypeColumn *theItem = (tableWidgetItemTypeColumn*)tableWidget->item( itemIndex.row(), itemIndex.column() );

                if ( theItem != NULL )
                {
                    msgType = theItem->msgType;
                }
            }

            // First put a black background.
            QRect actualRect = viewSettings.rect;
    
            painter->fillRect( actualRect, QColor( 0, 0, 0, 255 ) );

            // Decide whether we need an icon and which kind.
            if ( msgType == LOGMSG_WARNING || msgType == LOGMSG_ERROR )
            {
                QPixmap *useIcon = NULL;

                if ( msgType == LOGMSG_WARNING )
                {
                    useIcon = &this->owner->cachedWarningIcon;
                }
                else if ( msgType == LOGMSG_ERROR )
                {
                    useIcon = &this->owner->cachedErrorIcon;
                }

                if ( useIcon != NULL )
                {
                    // Draw it.
                    int centerX = ( actualRect.width() - useIcon->width() ) / 2;
                    int centerY = ( actualRect.height() - useIcon->height() ) / 2;

                    painter->drawPixmap( actualRect.left() + centerX, actualRect.top() + centerY, *useIcon );
                }
            }
        }

        QSize sizeHint( const QStyleOptionViewItem& viewSettings, const QModelIndex& itemIndex ) const override
        {
            return QSize( 50, 20 );
        }
    };

    struct abstractItemDelegateMessageColumn : public QStyledItemDelegate
    {
        QWidget* createEditor( QWidget *parent, const QStyleOptionViewItem& styleSettings, const QModelIndex& modelIndex ) const override
        {
            QPlainTextEdit *editor = new QPlainTextEdit( parent );

            editor->setReadOnly( true );
            editor->setWordWrapMode( QTextOption::NoWrap );
            
            // Disable scroll bars.
            editor->setHorizontalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );
            editor->setVerticalScrollBarPolicy( Qt::ScrollBarPolicy::ScrollBarAlwaysOff );

            editor->horizontalScrollBar()->setDisabled( true );
            editor->verticalScrollBar()->setDisabled( true );

            editor->setObjectName( "logItemMessageEditor" );

            editor->setGeometry( styleSettings.rect );

            return editor;
        }

        void setEditorData( QWidget *editor, const QModelIndex& modelIndex ) const override
        {
            QPlainTextEdit *editorImpl = (QPlainTextEdit*)editor;

            editorImpl->setPlainText( modelIndex.data().value <QString> () );
        }

        void setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex& modelIndex ) const override
        {
            QPlainTextEdit *editorImpl = (QPlainTextEdit*)editor;

            model->setData( modelIndex, editorImpl->toPlainText() );
        }

        void updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem& styleSettings, const QModelIndex& modelIndex ) const override
        {
            QPlainTextEdit *editorImpl = (QPlainTextEdit*)editor;

            editorImpl->setGeometry( styleSettings.rect );
        }
    };

    abstractItemDelegateIconColumn iconColumnDelegate;
    abstractItemDelegateMessageColumn messageColumnDelegate;

public:
    inline TxdLogWindow( void ) :
        warningIcon( "resources/warning.png" ),
        errorIcon( "resources/error.png" )
    {
        this->cachedWarningIcon = this->warningIcon.pixmap( 20, 20 );
        this->cachedErrorIcon = this->errorIcon.pixmap( 20, 20 );

        QWidget *wrapper = new QWidget();

        QVBoxLayout *layout = new QVBoxLayout();

        layout->setContentsMargins( 0, 0, 0, 0 );

        // Setup a menubar for quick controls.
        QMenuBar *menuBar = new QMenuBar();

        menuBar->setObjectName( "logWindowMenu" );

        // The main menu allows for specific control over the menu contents.
        QMenu *mainMenu = menuBar->addMenu( "&Main" );

        QAction *actionClear = new QAction( "&Clear", this );
        mainMenu->addAction( actionClear );

        connect( actionClear, &QAction::triggered, this, &TxdLogWindow::onLogClearRequest );

        QAction *actionSave = new QAction( "&Save", this );
        mainMenu->addAction( actionSave );

        connect( actionSave, &QAction::triggered, this, &TxdLogWindow::onLogSaveRequest );

        mainMenu->addSeparator();

        QAction *actionHide = new QAction( "&Hide", this );
        mainMenu->addAction( actionHide );

        connect( actionHide, &QAction::triggered, this, &TxdLogWindow::onWindowHideRequest );

        layout->setMenuBar( menuBar );

        // Create the table that will hold log items.
        QTableWidget *txdLogWidget = new QTableWidget( 0, 2 );

        iconColumnDelegate.owner = this;

        QStringList columnNames;

        columnNames.append( "Type" );
        columnNames.append( "Message" );

        txdLogWidget->setHorizontalHeaderLabels( columnNames );

        txdLogWidget->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );

        txdLogWidget->setItemDelegateForColumn( 0, &iconColumnDelegate );
        txdLogWidget->setItemDelegateForColumn( 1, &messageColumnDelegate );

        txdLogWidget->horizontalHeader()->setStretchLastSection( true );
        txdLogWidget->verticalHeader()->setVisible( false );

        txdLogWidget->horizontalHeader()->setSectionsClickable( false );

        txdLogWidget->setFrameStyle( QFrame::NoFrame );

        txdLogWidget->setWordWrap( true );

        txdLogWidget->setObjectName( "txdLogWindowList" );

        this->txdLogWidget = txdLogWidget;

        layout->addWidget( txdLogWidget );

        wrapper->setLayout( layout );

        this->setWidget( wrapper );

        this->setObjectName( "txdLogWindow" );
        
        this->setWindowTitle( "Log Window" );
    }

    inline ~TxdLogWindow( void )
    {
        // TODO.
    }

    inline void addLogMessage( QString msg, eLogMessageType msgType = LOGMSG_INFO )
    {
        bool isAtScrollCheck = ( this->txdLogWidget->verticalScrollBar()->value() == this->txdLogWidget->verticalScrollBar()->maximum() );

        QTableWidget *txdLogWidget = this->txdLogWidget;

        int currentRow = txdLogWidget->rowCount();

        txdLogWidget->setRowCount( currentRow + 1 );

        tableWidgetItemTypeColumn *iconItem = new tableWidgetItemTypeColumn();

        iconItem->msgType = msgType;

        txdLogWidget->setItem( currentRow, 0, iconItem );

        QTableWidgetItem *cellItem = new QTableWidgetItem( msg );

        // Give the label an appropriate style depending on the message type.
        if ( msgType == LOGMSG_WARNING )
        {
            cellItem->setBackgroundColor( QColor( 20, 20, 0 ) );
        }
        else if ( msgType == LOGMSG_ERROR )
        {
            cellItem->setBackgroundColor( QColor( 20, 0, 0 ) );
        }
        else if ( msgType == LOGMSG_INFO )
        {
            cellItem->setBackgroundColor( QColor( 0, 0, 0 ) );
        }

        txdLogWidget->setItem( currentRow, 1, cellItem );

        QSize theSize = txdLogWidget->fontMetrics().size( 0, msg );

        txdLogWidget->setRowHeight( currentRow, theSize.height() + 10 );

        // Scroll to bottom, if we want.
        if ( isAtScrollCheck )
        {
            txdLogWidget->scrollToBottom();
        }
    }

    inline void clearLog( void )
    {
        this->txdLogWidget->setRowCount( 0 );
    }

    inline void writeLogToStream( std::ostream& outStream )
    {
        // Lets print some basic log header with information about the tool and stuff.
        time_t currentTime = time( NULL );

        char timeBuf[ 1024 ];

        std::strftime( timeBuf, sizeof( timeBuf ), "%A %c", localtime( &currentTime ) );

        outStream <<
            "Magic.TXD generated log file on " << timeBuf << std::endl <<
            "compiled on " __DATE__ " version: " MTXD_VERSION_STRING << std::endl <<
            "================================================" << std::endl << std::endl;

        // Go through all log rows and print them.
        QTableWidget *txdLogWidget = this->txdLogWidget;

        int numRows = txdLogWidget->rowCount();

        for ( int n = 0; n < numRows; n++ )
        {
            // Get message information.
            tableWidgetItemTypeColumn *msgTypeItem = (tableWidgetItemTypeColumn*)txdLogWidget->item( n, 0 );

            eLogMessageType msgType = LOGMSG_INFO;

            if ( msgTypeItem )
            {
                msgType = msgTypeItem->msgType;
            }

            const char *msgTypeString = "info";
            {
                if ( msgType == LOGMSG_INFO )
                {
                    msgTypeString = "info";
                }
                else if ( msgType == LOGMSG_WARNING )
                {
                    msgTypeString = "warning";
                }
                else if ( msgType == LOGMSG_ERROR )
                {
                    msgTypeString = "error";
                }
            }

            // Now the string itself.
            QTableWidgetItem *msgItem = txdLogWidget->item( n, 1 );

            if ( msgItem )
            {
                // If we have a string, it is worth printing it.
                const QString message = msgItem->text();

                const std::string ansiMessage = message.toStdString();
                
                // Do it.
                outStream << "*** [" << msgTypeString << "]: " << ansiMessage << std::endl;
            }
        }
    }

    inline void saveLog( QString fileName )
    {
        // Try to open a file handle.
        const std::wstring unicodeFileName = fileName.toStdWString();

        // NOTE: this is a Microsoft function.
        // there is no native cross-platform solution for unicode file paths.
        std::fstream fileHandle( unicodeFileName.c_str() );

        if ( fileHandle.good() )
        {
            // Write the stuff.
            writeLogToStream( fileHandle );

            // Close us again.
            fileHandle.close();
        }
        else
        {
            // TODO: display reason why failed.
        }
    }

public slots:
    void onLogClearRequest( bool checked )
    {
        this->clearLog();
    }

    void onLogSaveRequest( bool checked )
    {
        QString saveFileName = QFileDialog::getSaveFileName( this, tr( "Save Log as..." ), QString(), "Log File (*.txt)" );

        if ( saveFileName.length() != 0 )
        {
            this->saveLog( saveFileName );
        }
    }

    void onWindowHideRequest( bool checked )
    {
        this->hide();
    }

private:
    // Resources that we use.
    QIcon warningIcon;
    QIcon errorIcon;

    // Cached resources.
    QPixmap cachedWarningIcon;
    QPixmap cachedErrorIcon;

    // Widgets.
    QTableWidget *txdLogWidget;
};