#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QFileInfo>
#include <QLabel>

#include <renderware.h>

#include "textureViewport.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setCurrentTXD( rw::TexDictionary *txdObj );

    void updateWindowTitle( void );

public slots:
    void onOpenFile( bool checked );
    void onCloseCurrent( bool checked );

    void onTextureItemSelected( QListWidgetItem *texInfoItem );

private:
    rw::Interface *rwEngine;
    rw::TexDictionary *currentTXD;

    QFileInfo openedTXDFileInfo;

    QListWidget *textureListWidget;
    TexViewportWidget *textureViewport;

    QLabel *txdNameLabel;
};

#endif // MAINWINDOW_H
