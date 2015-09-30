#include "mainwindow.h"
#include "optionsdialog.h"

OptionsDialog::OptionsDialog( MainWindow *mainWnd ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    setWindowTitle( "Magic.TXD Options" );
    setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

    setAttribute( Qt::WA_DeleteOnClose );

    // This will be a fairly complicated dialog.
    QVBoxLayout *mainVertical = new QVBoxLayout();

    // Pretty much options over options.
    QVBoxLayout *optionsLayout = new QVBoxLayout();

    optionsLayout->setAlignment( Qt::AlignTop );
    
    QCheckBox *optionShowLogOnWarning = new QCheckBox( "show TXD log on warning" );
    optionShowLogOnWarning->setChecked( mainWnd->showLogOnWarning );

    this->optionShowLogOnWarning = optionShowLogOnWarning;

    optionsLayout->addWidget( optionShowLogOnWarning );

    mainVertical->addLayout( optionsLayout );

    // After everything we want to add our button row.
    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setContentsMargins( 0, 20, 0, 0 );

    buttonRow->setAlignment( Qt::AlignBottom | Qt::AlignRight );

    QPushButton *buttonAccept = new QPushButton( "Accept" );
    buttonAccept->setMaximumWidth( 100 );

    connect( buttonAccept, &QPushButton::clicked, this, &OptionsDialog::OnRequestApply );

    buttonRow->addWidget( buttonAccept );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );
    buttonCancel->setMaximumWidth( 100 );

    connect( buttonCancel, &QPushButton::clicked, this, &OptionsDialog::OnRequestCancel );

    buttonRow->addWidget( buttonCancel );

    mainVertical->addLayout( buttonRow );

    this->setLayout( mainVertical );

    this->setMinimumWidth( 420 );   // blaze it.

    mainWnd->optionsDlg = this;
}

OptionsDialog::~OptionsDialog( void )
{
    mainWnd->optionsDlg = NULL;
}

void OptionsDialog::OnRequestApply( bool checked )
{
    this->serialize();

    this->close();
}

void OptionsDialog::OnRequestCancel( bool checked )
{
    this->close();
}

void OptionsDialog::serialize( void )
{
    // We want to store things into the main window.
    MainWindow *mainWnd = this->mainWnd;

    mainWnd->showLogOnWarning = this->optionShowLogOnWarning->isChecked();
}