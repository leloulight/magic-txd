#include "mainwindow.h"

#include "languages.h"

// Common language components that the whole Magic.TXD application has to know about.
// We basically use those components in the entire GUI interface.

struct SimpleLocalizedButton : public QPushButton, public simpleLocalizationItem
{
    inline SimpleLocalizedButton( MainWindow *mainWnd, QString systemToken ) : simpleLocalizationItem( mainWnd, std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedButton( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QPushButton* CreateButtonL( MainWindow *mainWnd, QString systemToken )
{
    SimpleLocalizedButton *newButton = new SimpleLocalizedButton( mainWnd, std::move( systemToken ) );
    newButton->setMinimumWidth( 90 );
    return newButton;
}

struct FixedWidthLocalizedButton : public QPushButton, public simpleLocalizationItem
{
    inline FixedWidthLocalizedButton( MainWindow *mainWnd, QString systemToken, int fontSize ) : fontSize( fontSize ), simpleLocalizationItem( mainWnd, systemToken )
    {
        Init();
    }

    inline ~FixedWidthLocalizedButton( void )
    {
        Shutdown();
    }

    void doText( QString theText )
    {
        this->setText( theText );
        this->setFixedWidth( GetTextWidthInPixels( theText, this->fontSize ) );
    }

    int fontSize;
};

QPushButton* CreateFixedWidthButtonL( MainWindow *mainWnd, QString systemToken, int fontSize )
{
    FixedWidthLocalizedButton *newButton = new FixedWidthLocalizedButton( mainWnd, systemToken, fontSize );

    return newButton;
}

struct SimpleLocalizedLabel : public QLabel, public simpleLocalizationItem
{
    inline SimpleLocalizedLabel( MainWindow *mainWnd, QString systemToken ) : simpleLocalizationItem( mainWnd, std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedLabel( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QLabel* CreateLabelL( MainWindow *mainWnd, QString systemToken )
{
    SimpleLocalizedLabel *newLabel = new SimpleLocalizedLabel( mainWnd, std::move( systemToken ) );

    return newLabel;
}

struct FixedWidthLocalizedLabel : public QLabel, public simpleLocalizationItem
{
    inline FixedWidthLocalizedLabel( MainWindow *mainWnd, QString systemToken, int fontSize ) : font_size( fontSize ), simpleLocalizationItem( mainWnd, std::move( systemToken ) )
    {
        Init();
    }

    inline ~FixedWidthLocalizedLabel( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
        this->setFixedWidth( GetTextWidthInPixels( text, this->font_size ) );
    }

    int font_size;
};

QLabel* CreateFixedWidthLabelL( MainWindow *mainWnd, QString systemToken, int fontSize )
{
    FixedWidthLocalizedLabel *newLabel = new FixedWidthLocalizedLabel( mainWnd, systemToken, fontSize );

    return newLabel;
}

struct SimpleLocalizedCheckBox : public QCheckBox, public simpleLocalizationItem
{
    inline SimpleLocalizedCheckBox( MainWindow *mainWnd, QString systemToken ) : simpleLocalizationItem( mainWnd, std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedCheckBox( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QCheckBox* CreateCheckBoxL( MainWindow *mainWnd, QString systemToken )
{
    SimpleLocalizedCheckBox *newCB = new SimpleLocalizedCheckBox( mainWnd, std::move( systemToken ) );

    return newCB;
}

struct SimpleLocalizedRadioButton : public QRadioButton, public simpleLocalizationItem
{
    inline SimpleLocalizedRadioButton( MainWindow *mainWnd, QString systemToken ) : simpleLocalizationItem( mainWnd, std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedRadioButton( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QRadioButton* CreateRadioButtonL( MainWindow *mainWnd, QString systemToken )
{
    SimpleLocalizedRadioButton *radioButton = new SimpleLocalizedRadioButton( mainWnd, systemToken );

    return radioButton;
}

struct MnemonicAction : public QAction, public magicTextLocalizationItem
{
    inline MnemonicAction( MainWindow *mainWnd, QString systemToken, QObject *parent ) : QAction( parent )
    {
        this->mainWnd = mainWnd;
        this->systemToken = std::move( systemToken );

        RegisterTextLocalizationItem( mainWnd, this );
    }

    inline ~MnemonicAction( void )
    {
        UnregisterTextLocalizationItem( mainWnd, this );
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        QString descText = getLanguageItemByKey( mainWnd, this->systemToken );

        this->setText( "&" + descText );
    }

private:
    MainWindow *mainWnd;
    QString systemToken;
};

QAction* CreateMnemonicActionL( MainWindow *mainWnd, QString systemToken, QObject *parent )
{
    MnemonicAction *newAction = new MnemonicAction( mainWnd, systemToken, parent );

    return newAction;
}