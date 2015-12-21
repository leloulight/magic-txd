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
    mainVertical->setContentsMargins(0, 0, 0, 0);
    mainVertical->setSpacing(0);

    // Pretty much options over options.
    QVBoxLayout *optionsLayout = new QVBoxLayout();
    optionsLayout->setContentsMargins(12, 12, 12, 12);
    optionsLayout->setSpacing(12);

    //optionsLayout->setAlignment( Qt::AlignTop );
    
    QCheckBox *optionShowLogOnWarning = new QCheckBox( "show TXD log on warning" );
    optionShowLogOnWarning->setChecked( mainWnd->showLogOnWarning );

    this->optionShowLogOnWarning = optionShowLogOnWarning;

    QCheckBox *optionShowGameIcon = new QCheckBox("show game icon");
    optionShowGameIcon->setChecked(mainWnd->showGameIcon);

    this->optionShowGameIcon = optionShowGameIcon;

    optionsLayout->addWidget( optionShowLogOnWarning );
    optionsLayout->addWidget( optionShowGameIcon );

    mainVertical->addLayout( optionsLayout );

    // After everything we want to add our button row.
    QHBoxLayout *buttonRow = new QHBoxLayout();

    buttonRow->setAlignment( Qt::AlignBottom | Qt::AlignRight );

    buttonRow->setContentsMargins(12, 12, 12, 12);
    buttonRow->setSpacing(12);

    QPushButton *buttonAccept = new QPushButton( "Accept" );
    buttonAccept->setMaximumWidth( 100 );

    connect( buttonAccept, &QPushButton::clicked, this, &OptionsDialog::OnRequestApply );

    buttonRow->addWidget( buttonAccept );

    QPushButton *buttonCancel = new QPushButton( "Cancel" );
    buttonCancel->setMaximumWidth( 100 );

    connect( buttonCancel, &QPushButton::clicked, this, &OptionsDialog::OnRequestCancel );

    buttonRow->addWidget( buttonCancel );

    QWidget *line = new QWidget();
    line->setFixedHeight(1);
    line->setObjectName("hLineBackground");

    mainVertical->addWidget(line);

    QWidget *bottomWidget = new QWidget();
    bottomWidget->setObjectName("background_0");
    bottomWidget->setLayout(buttonRow);

    mainVertical->addWidget( bottomWidget );

    mainVertical->setSizeConstraint(QLayout::SetFixedSize);

    this->setLayout( mainVertical );

    //this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    //this->setFixedSize( 420, 200 );   // blaze it.

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

    if (mainWnd->showGameIcon != this->optionShowGameIcon->isChecked()) {
        mainWnd->showGameIcon = this->optionShowGameIcon->isChecked();
        mainWnd->updateFriendlyIcons();
    }
}