#pragma once

#include <list>

#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

struct RenderPropWindow : public QDialog, public magicTextLocalizationItem
{
private:
    QComboBox* createFilterBox( void ) const;

public:
    RenderPropWindow( MainWindow *mainWnd, TexInfoWidget *texInfo );
    ~RenderPropWindow( void );

    void updateContent( MainWindow *mainWnd ) override;

public slots:
    void OnRequestSet( bool checked );
    void OnRequestCancel( bool checked );

    void OnAnyPropertyChange( const QString& newValue )
    {
        this->UpdateAccessibility();
    }

private:
    void UpdateAccessibility( void );

    MainWindow *mainWnd;

    TexInfoWidget *texInfo;

    QPushButton *buttonSet;
    QComboBox *filterComboBox;
    QComboBox *uaddrComboBox;
    QComboBox *vaddrComboBox;
};