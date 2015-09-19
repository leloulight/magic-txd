#include "mainwindow.h"
#include <QApplication>
#include "styles.h"
#include <QTimer>
#include "resource.h"

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

// Main window factory.
mainWindowFactory_t mainWindowFactory;

struct mainWindowConstructor
{
    QString appPath;

    inline mainWindowConstructor( QString&& appPath ) : appPath( appPath )
    {}

    inline MainWindow* Construct( void *mem ) const
    {
        return new (mem) MainWindow( appPath );
    }
};

struct defaultMemAlloc
{
    static void* Allocate( size_t memSize )
    {
        return malloc( memSize );
    }

    static void Free( void *mem, size_t memSize )
    {
        free( mem );
    }
};

// Main window plugin entry points.
extern void InitializeGUISerialization( void );

static defaultMemAlloc _factMemAlloc;

int main(int argc, char *argv[])
{
    // Initialize all main window plugins.
    InitializeGUISerialization();

	QStringList paths = QCoreApplication::libraryPaths();
	paths.append(".");
	paths.append("imageformats");
	paths.append("platforms");
	QCoreApplication::setLibraryPaths(paths);
    MagicTXDApplication a(argc, argv);
    a.setStyleSheet(styles::get(a.applicationDirPath(), "resources\\dark.shell"));

    mainWindowConstructor wnd_constr( a.applicationDirPath() );

    MainWindow *w = mainWindowFactory.ConstructTemplate( _factMemAlloc, wnd_constr );
    w->setWindowIcon(QIcon(w->makeAppPath("resources\\icons\\stars.png")));
    w->show();
    QApplication::processEvents();

    //char text[256];
    //sprintf(text, "args: %d\n%s\n%s", argc, argv[0], argc > 1? argv[1] : "NO_ARG");
    //MessageBoxA(0, text, 0, 0);
    
    if (argc >= 2 && argv[1] && argv[1][0]) {
        QString txdFileToBeOpened = QString::fromLocal8Bit(argv[1]);
        if (!txdFileToBeOpened.isEmpty()) {
            w->openTxdFile(txdFileToBeOpened);
        }
    }

    int iRet = a.exec();

    mainWindowFactory.Destroy( _factMemAlloc, w );

    return iRet;
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