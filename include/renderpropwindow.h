#pragma once

#include <list>

#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

struct RenderPropWindow : public QDialog
{
private:
    QComboBox* createFilterBox( void ) const;

public:
    RenderPropWindow( MainWindow *mainWnd, TexInfoWidget *texInfo );
    ~RenderPropWindow( void );

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