#pragma once

#include <QHash>
#include <QVector>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qlabel.h>

#include <QDirIterator>

#define DEFAULT_LANGUAGE "English"

#define CURRENT_MAGL_VERSION 1
#define MINIMUM_SUPPORTED_MAGL_VERSION 1

struct magicTextLocalizationItem abstract
{
    // Called by the language loader when a language has been loaded into the application.
    // This will update all text in Magic.TXD!
    virtual void updateContent( MainWindow *mainWnd ) = 0;
};

// API to register localization items.
bool RegisterTextLocalizationItem( MainWindow *mainWnd, magicTextLocalizationItem *provider );
bool UnregisterTextLocalizationItem( MainWindow *mainWnd, magicTextLocalizationItem *provider );

// Main Query function to request current localized text strings.
QString getLanguageItemByKey( MainWindow *mainWnd, QString token, bool *found = NULL );

// Helper struct.
struct simpleLocalizationItem : public magicTextLocalizationItem
{
    inline simpleLocalizationItem( MainWindow *mainWnd, QString systemToken )
    {
        this->mainWnd = mainWnd;
        this->systemToken = std::move( systemToken );
    }

    inline ~simpleLocalizationItem( void )
    {
        return;
    }

    void Init( void )
    {
        // Register ourselves.
        RegisterTextLocalizationItem( mainWnd, this );
    }

    void Shutdown( void )
    {
        // Unregister again.
        UnregisterTextLocalizationItem( this->mainWnd, this );
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        QString newText = getLanguageItemByKey( mainWnd, this->systemToken );

        this->doText( std::move( newText ) );
    }

    virtual void doText( QString text ) = 0;

    MainWindow *mainWnd;
    QString systemToken;
};

// Common GUI components that are linked to localized text.
QPushButton* CreateButtonL( MainWindow *mainWnd, QString systemToken );
QLabel* CreateLabelL( MainWindow *mainWnd, QString systemToken );
QLabel* CreateFixedWidthLabelL( MainWindow *mainWnd, QString systemToken, int fontSize );
QCheckBox* CreateCheckBoxL( MainWindow *mainWnd, QString systemToken );
QRadioButton* CreateRadioButtonL( MainWindow *mainWnd, QString systemToken );

QAction* CreateMnemonicActionL( MainWindow *mainWnd, QString systemToken, QObject *parent = NULL );

// TODO: Maybe move it somewhere
unsigned int GetTextWidthInPixels(QString &text, unsigned int fontSize);