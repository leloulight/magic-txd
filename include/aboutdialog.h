#pragma once

#include <QDialog>

class MainWindow;

struct AboutDialog : public QDialog
{
    AboutDialog( MainWindow *mainWnd );
    ~AboutDialog( void );

public slots:
    void OnRequestClose( bool checked );

private:
    MainWindow *mainWnd;
};