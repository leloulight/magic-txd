#pragma once

#include <QDialog>

class MainWindow;

struct AboutDialog : public QDialog, public magicTextLocalizationItem
{
    AboutDialog( MainWindow *mainWnd );
    ~AboutDialog( void );

    void updateContent( MainWindow *mainWnd );

public slots:
    void OnRequestClose( bool checked );

private:
    MainWindow *mainWnd;
};