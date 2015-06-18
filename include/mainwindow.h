#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QFileInfo>
#include <QLabel>
#include <QScrollArea>

#include <renderware.h>

#include "texinfoitem.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setCurrentTXD( rw::TexDictionary *txdObj );

    void updateWindowTitle( void );
    void updateTextureMetaInfo( void );

    void updateTextureView( void );

    void saveCurrentTXDAt( QString location );

public slots:
    void onOpenFile( bool checked );
    void onCloseCurrent( bool checked );

    void onTextureItemSelected( QListWidgetItem *texInfoItem );

    void onToggleShowMipmapLayers( bool checked );
    void onSetupMipmapLayers( bool checked );
    void onClearMipmapLayers( bool checked );

    void onRequestSaveTXD( bool checked );
    void onRequestSaveAsTXD( bool checked );

private:
    rw::Interface *rwEngine;
    rw::TexDictionary *currentTXD;

    TexInfoWidget *currentSelectedTexture;

    QFileInfo openedTXDFileInfo;

    QListWidget *textureListWidget;

	QScrollArea *imageView; // we handle full 2d-viewport as a scroll-area
	QLabel *imageWidget;    // we use label to put image on it

    QLabel *txdNameLabel;

    bool drawMipmapLayers;
};

#endif // MAINWINDOW_H
