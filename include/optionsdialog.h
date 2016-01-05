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
    void OnChangeSelectedLanguage(int newIndex);

private:
    void serialize( void );

    MainWindow *mainWnd;

    QCheckBox *optionShowLogOnWarning;
    QCheckBox *optionShowGameIcon;

    QComboBox *languageBox;
    QLabel *languageAuthorLabel;
};