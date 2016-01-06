#pragma once

#include <QDialog>
#include <QComboBox>

struct OptionsDialog : public QDialog, public magicTextLocalizationItem
{
    OptionsDialog( MainWindow *mainWnd );
    ~OptionsDialog( void );

    void updateContent( MainWindow *mainWnd ) override;

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