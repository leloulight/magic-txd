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
    inline ScopedSystemEventFilter(QObject *receiver, QEvent *evt)
    {
        _currentFilter = this;

        QWidget *theWidget = NULL;

        this->evt = NULL;
        this->handlerWidget = NULL;

        if (receiver->isWidgetType())
        {
            theWidget = (QWidget*)receiver;
        }

        if (theWidget)
        {
            // We are not there yet.
            // Check whether this widget supports system event signalling.
            SystemEventHandlerWidget *hWidget = dynamic_cast <SystemEventHandlerWidget*> (theWidget);

            if (hWidget)
            {
                hWidget->beginSystemEvent(evt);

                this->evt = evt;
                this->handlerWidget = hWidget;
            }
        }
    }

    inline ~ScopedSystemEventFilter(void)
    {
        if (handlerWidget)
        {
            handlerWidget->endSystemEvent(evt);
        }

        _currentFilter = NULL;
    }

    static ScopedSystemEventFilter *_currentFilter;

    QEvent *evt;
    SystemEventHandlerWidget *handlerWidget;
};

ScopedSystemEventFilter* ScopedSystemEventFilter::_currentFilter = NULL;

SystemEventHandlerWidget::~SystemEventHandlerWidget(void)
{
    ScopedSystemEventFilter *sysEvtFilter = ScopedSystemEventFilter::_currentFilter;

    if (sysEvtFilter)
    {
        if (sysEvtFilter->handlerWidget = this)
        {
            sysEvtFilter->handlerWidget = NULL;
            sysEvtFilter->evt = NULL;
        }
    }
}

struct MagicTXDApplication : public QApplication
{
    inline MagicTXDApplication(int argc, char *argv[]) : QApplication(argc, argv)
    {
        return;
    }

    bool notify(QObject *receiver, QEvent *evt) override
    {
        ScopedSystemEventFilter filter(receiver, evt);

        return QApplication::notify(receiver, evt);
    }
};

// Main window factory.
mainWindowFactory_t mainWindowFactory;

struct mainWindowConstructor
{
    QString appPath;
    rw::Interface *rwEngine;
    CFileSystem *fsHandle;

    inline mainWindowConstructor(QString&& appPath, rw::Interface *rwEngine, CFileSystem *fsHandle)
        : appPath(appPath), rwEngine(rwEngine), fsHandle(fsHandle)
    {}

    inline MainWindow* Construct(void *mem) const
    {
        return new (mem) MainWindow(appPath, rwEngine, fsHandle);
    }
};

struct defaultMemAlloc
{
    static void* Allocate(size_t memSize)
    {
        return malloc(memSize);
    }

    static void Free(void *mem, size_t memSize)
    {
        free(mem);
    }
};

// Main window plugin entry points.
extern void InitializeRWFileSystemWrap(void);
extern void InitializeMassconvToolEnvironment(void);
extern void InitializeMassExportToolEnvironment( void );
extern void InitializeGUISerialization(void);

static defaultMemAlloc _factMemAlloc;

int main(int argc, char *argv[])
{
    // Initialize all main window plugins.
    InitializeRWFileSystemWrap();
    InitializeMassconvToolEnvironment();
    InitializeMassExportToolEnvironment();
    InitializeGUISerialization();

    int iRet = -1;

    // Initialize the RenderWare engine.
    rw::LibraryVersion engineVersion;

    // This engine version is the default version we create resources in.
    // Resources can change their version at any time, so we do not have to change this.
    engineVersion.rwLibMajor = 3;
    engineVersion.rwLibMinor = 6;
    engineVersion.rwRevMajor = 0;
    engineVersion.rwRevMinor = 3;

    rw::Interface *rwEngine = rw::CreateEngine( engineVersion );

    if ( rwEngine == NULL )
    {
        throw std::exception( "failed to initialize the RenderWare engine" );
    }

    try
    {
        // Set some typical engine properties.
        rwEngine->SetIgnoreSerializationBlockRegions( true );
        rwEngine->SetIgnoreSecureWarnings( false );

        rwEngine->SetWarningLevel( 3 );

        rwEngine->SetDXTRuntime( rw::DXTRUNTIME_SQUISH );
        rwEngine->SetPaletteRuntime( rw::PALRUNTIME_PNGQUANT );

        // Give RenderWare some info about us!
        rw::softwareMetaInfo metaInfo;
        metaInfo.applicationName = "Magic.TXD";
        metaInfo.applicationVersion = MTXD_VERSION_STRING;
        metaInfo.description = "by DK22Pac and The_GTA (https://github.com/quiret/magic-txd)";

        rwEngine->SetApplicationInfo( metaInfo );

        // Initialize the filesystem.
        fs_construction_params fsParams;
        fsParams.nativeExecMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( rwEngine );

        CFileSystem *fsHandle = CFileSystem::Create( fsParams );

        if ( !fsHandle )
        {
            throw std::exception( "failed to initialize the FileSystem module" );
        }

        try
        {
            QStringList paths = QCoreApplication::libraryPaths();
            paths.append(".");
            paths.append("imageformats");
            paths.append("platforms");
            QCoreApplication::setLibraryPaths(paths);

            MagicTXDApplication a(argc, argv);
            a.setStyleSheet(styles::get(a.applicationDirPath(), "resources\\dark.shell"));
            mainWindowConstructor wnd_constr(a.applicationDirPath(), rwEngine, fsHandle);

            MainWindow *w = mainWindowFactory.ConstructTemplate(_factMemAlloc, wnd_constr);

            try
            {
                w->setWindowIcon(QIcon(w->makeAppPath("resources\\icons\\stars.png")));
                w->show();
                QApplication::processEvents();

                //char text[256];
                //sprintf(text, "args: %d\n%s\n%s", argc, argv[0], argc > 1? argv[1] : "NO_ARG");
                //MessageBoxA(0, text, 0, 0);

                QStringList appargs = a.arguments();

                if (appargs.size() >= 2) {
                    QString txdFileToBeOpened = appargs.at(1);
                    if (!txdFileToBeOpened.isEmpty()) {
                        w->openTxdFile(txdFileToBeOpened);
                    }
                }

                iRet = a.exec();
            }
            catch( ... )
            {
                w->deleteChildWindows();

                mainWindowFactory.Destroy( _factMemAlloc, w );

                throw;
            }

            w->deleteChildWindows();

            mainWindowFactory.Destroy(_factMemAlloc, w);
        }
        catch( ... )
        {
            CFileSystem::Destroy( fsHandle );

            throw;
        }

        CFileSystem::Destroy( fsHandle );
    }
    catch( ... )
    {
        rw::DeleteEngine( rwEngine );

        throw;
    }

    rw::DeleteEngine( rwEngine );

    return iRet;
}

// Stubs.
namespace rw
{
    LibraryVersion app_version()
    {
        return rw::KnownVersions::getGameVersion(rw::KnownVersions::SA);
    }

    int32 rwmain(Interface *engineInterface)
    {
        return -1;
    }
};