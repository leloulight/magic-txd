#include "mainwindow.h"
#include <QApplication>
#include "styles.h"

#include <QImageWriter>

#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)

struct ScopedSystemEventFilter
{
    inline ScopedSystemEventFilter( QObject *receiver, QEvent *evt )
    {
        _currentFilter = this;

        QWidget *theWidget = NULL;

        this->evt = NULL;
        this->handlerWidget = NULL;

        if ( receiver->isWidgetType() )
        {
            theWidget = (QWidget*)receiver;
        }

        if ( theWidget )
        {
            // We are not there yet.
            // Check whether this widget supports system event signalling.
            SystemEventHandlerWidget *hWidget = dynamic_cast <SystemEventHandlerWidget*> ( theWidget );

            if ( hWidget )
            {
                hWidget->beginSystemEvent( evt );

                this->evt = evt;
                this->handlerWidget = hWidget;
            }
        }
    }

    inline ~ScopedSystemEventFilter( void )
    {
        if ( handlerWidget )
        {
            handlerWidget->endSystemEvent( evt );
        }

        _currentFilter = NULL;
    }

    static ScopedSystemEventFilter *_currentFilter;

    QEvent *evt;
    SystemEventHandlerWidget *handlerWidget;
};

ScopedSystemEventFilter* ScopedSystemEventFilter::_currentFilter = NULL;

SystemEventHandlerWidget::~SystemEventHandlerWidget( void )
{
    ScopedSystemEventFilter *sysEvtFilter = ScopedSystemEventFilter::_currentFilter;

    if ( sysEvtFilter )
    {
        if ( sysEvtFilter->handlerWidget = this )
        {
            sysEvtFilter->handlerWidget = NULL;
            sysEvtFilter->evt = NULL;
        }
    }
}

struct MagicTXDApplication : public QApplication
{
    inline MagicTXDApplication( int argc, char *argv[] ) : QApplication( argc, argv )
    {
        return;
    }

    bool notify( QObject *receiver, QEvent *evt ) override
    {
        ScopedSystemEventFilter filter( receiver, evt );

        return QApplication::notify( receiver, evt );
    }
};

int main(int argc, char *argv[])
{
	QStringList paths = QCoreApplication::libraryPaths();
	paths.append(".");
	paths.append("imageformats");
	paths.append("platforms");
	QCoreApplication::setLibraryPaths(paths);

    MagicTXDApplication a(argc, argv);
    a.setStyleSheet(styles::get("resources\\dark.shell"));
    MainWindow w;
    w.show();

    return a.exec();
}

// Stubs.
namespace rw
{
    LibraryVersion app_version()
    {
        return rw::KnownVersions::getGameVersion( rw::KnownVersions::SA );
    }

    int32 rwmain( Interface *engineInterface )
    {
        return -1;
    }
};