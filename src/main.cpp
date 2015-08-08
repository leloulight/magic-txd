#include "mainwindow.h"
#include <QApplication>
#include "styles.h"

int main(int argc, char *argv[])
{
	QStringList paths = QCoreApplication::libraryPaths();
	paths.append(".");
	paths.append("imageformats");
	paths.append("platforms");
	QCoreApplication::setLibraryPaths(paths);

    QApplication a(argc, argv);
    a.setStyleSheet(styles::get("resources\\main.shell"));
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