#pragma once

#include <QDialog>
#include <QComboBox>

struct OptionsDialog : public QDialog
{
    OptionsDialog( MainWindow *mainWnd );
    ~OptionsDialog( void );

public slots:
    void OnRequestApply( bool checked );
    void OnRequestCancel( bool checked );

private:
    void serialize( void );

    MainWindow *mainWnd;

    QCheckBox *optionShowLogOnWarning;
};