#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QFileInfo>
#include <QLabel>
#include <QScrollArea>

#include <renderware.h>

#include "defs.h"

#include "texinfoitem.h"
#include "txdlog.h"
#include "txdadddialog.h"

#include "MagicExport.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class TexAddDialog;

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    void initializeNativeFormats( void );
    void shutdownNativeFormats( void );

public:
    void setCurrentTXD( rw::TexDictionary *txdObj );

    void updateWindowTitle( void );
    void updateTextureMetaInfo( void );

    void updateTextureView( void );

    void saveCurrentTXDAt( QString location );

	void clearViewImage( void );

public slots:
    void onOpenFile( bool checked );
    void onCloseCurrent( bool checked );

	void onTextureItemChanged(QListWidgetItem *texInfoItem, QListWidgetItem *prevTexInfoItem);

    void onToggleShowMipmapLayers( bool checked );
	void onToggleShowBackground(bool checked);
    void onToggleShowLog( bool checked );
    void onSetupMipmapLayers( bool checked );
    void onClearMipmapLayers( bool checked );

    void onRequestSaveTXD( bool checked );
    void onRequestSaveAsTXD( bool checked );

    void onExportTexture( bool checked );

protected:
    void addTextureFormatExportLinkToMenu( QMenu *theMenu, const char *defaultExt, const char *formatName );

private:
    class rwPublicWarningDispatcher : public rw::WarningManagerInterface
    {
    public:
        inline rwPublicWarningDispatcher( MainWindow *theWindow )
        {
            this->mainWnd = theWindow;
        }

        void OnWarning( const std::string& msg ) override
        {
			this->mainWnd->txdLog->addLogMessage(msg.c_str(), LOGMSG_WARNING);
        }

    private:
        MainWindow *mainWnd;
    };

    rwPublicWarningDispatcher rwWarnMan;

    rw::Interface *rwEngine;
    rw::TexDictionary *currentTXD;

    TexInfoWidget *currentSelectedTexture;

    QFileInfo openedTXDFileInfo;

    QListWidget *textureListWidget;

	QScrollArea *imageView; // we handle full 2d-viewport as a scroll-area
	QLabel *imageWidget;    // we use label to put image on it

    QLabel *txdNameLabel;

    bool drawMipmapLayers;
	bool showBackground;

	TxdLog *txdLog; // log management class
};

#endif // MAINWINDOW_H
