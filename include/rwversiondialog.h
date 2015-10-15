#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

#include <regex>
#include <string>
#include <sstream>

struct RwVersionTemplate {
	std::string name;

};



class RwVersionDialog : public QDialog
{
	//Q_OBJECT why would we need this?

    MainWindow *mainWnd;

    QLineEdit *versionLineEdit;
    QLineEdit *buildLineEdit;

    QPushButton *applyButton;

    QComboBox *gameSelectBox;

    bool GetSelectedVersion( rw::LibraryVersion& verOut ) const
    {
        QString currentVersionString = this->versionLineEdit->text();

        std::string ansiCurrentVersionString = currentVersionString.toStdString();

        rw::LibraryVersion theVersion;

        bool hasValidVersion = false;

        // Verify whether our version is valid while creating our local version struct.
        unsigned int rwLibMajor, rwLibMinor, rwRevMajor, rwRevMinor;
        bool hasProperMatch = false;
        {
            std::regex ver_regex( "(\\d)\\.(\\d{1,2})\\.(\\d{1,2})\\.(\\d{1,2})" );

            std::smatch ver_match;

            std::regex_match( ansiCurrentVersionString, ver_match, ver_regex );

            if ( ver_match.size() == 5 )
            {
                rwLibMajor = std::stoul( ver_match[ 1 ] );
                rwLibMinor = std::stoul( ver_match[ 2 ] );
                rwRevMajor = std::stoul( ver_match[ 3 ] );
                rwRevMinor = std::stoul( ver_match[ 4 ] );

                hasProperMatch = true;
            }
        }

        if ( hasProperMatch )
        {
            if ( ( rwLibMajor >= 3 && rwLibMajor <= 6 ) &&
                 ( rwLibMinor <= 15 ) &&
                 ( rwRevMajor <= 15 ) && 
                 ( rwRevMinor <= 63 ) )
            {
                theVersion.rwLibMajor = rwLibMajor;
                theVersion.rwLibMinor = rwLibMinor;
                theVersion.rwRevMajor = rwRevMajor;
                theVersion.rwRevMinor = rwRevMinor;

                hasValidVersion = true;
            }
        }

        if ( hasValidVersion )
        {
            // Also set the build number, if valid.
            QString buildNumber = this->buildLineEdit->text();

            std::string ansiBuildNumber = buildNumber.toStdString();

            unsigned int buildNum;

            int matchCount = sscanf( ansiBuildNumber.c_str(), "%x", &buildNum );

            if ( matchCount == 1 )
            {
                if ( buildNum <= 65535 )
                {
                    theVersion.buildNumber = buildNum;
                }
            }

            // Having an invalid build number does not mean that our version is invalid.
            // The build number is just candy anyway.
        }

        if ( hasValidVersion )
        {
            verOut = theVersion;
        }

        return hasValidVersion;
    }

    void UpdateAccessibility( void )
    {
        rw::LibraryVersion libVer;

        // Check whether we should even enable input.
        // This is only if the user selected "Custom".
        bool shouldAllowInput = ( this->gameSelectBox->currentText() == "Custom" );

        this->versionLineEdit->setDisabled( !shouldAllowInput );
        this->buildLineEdit->setDisabled( !shouldAllowInput );

        bool hasValidVersion = this->GetSelectedVersion( libVer );

        // Alright, set enabled-ness based on valid version.
        this->applyButton->setDisabled( !hasValidVersion );
    }

public slots:
    void OnChangeVersion( const QString& newText )
    {
        // The version must be validated.
        this->UpdateAccessibility();
    }

    void OnChangeSelectedGame( const QString& newItem )
    {
        // If we selected a game that we know, then set the version into the edits.
        rw::LibraryVersion libVer;
        bool hasGameVer = true;
        
        const char *currentPlatform = NULL;

        if ( rw::TexDictionary *currentTXD = this->mainWnd->currentTXD )
        {
            currentPlatform = MainWindow::GetTXDPlatformString( currentTXD );
        }

        if ( newItem == "GTA San Andreas" )
        {
            libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::SA );
        }
        else if ( newItem == "GTA Vice City" )
        {
            if ( currentPlatform && stricmp( currentPlatform, "PlayStation2" ) == 0 )
            {
                libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::VC_PS2 );
            }
            else
            {
                libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::VC_PC );
            }
        }
        else if ( newItem == "GTA III" )
        {
            libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::GTA3 );
        }
        else if ( newItem == "Manhunt" )
        {
            if ( currentPlatform && stricmp( currentPlatform, "PlayStation2" ) == 0 )
            {
                libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::MANHUNT_PS2 );
            }
            else
            {
                libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::MANHUNT_PC );
            }
        }
        else if ( newItem == "Bully" )
        {
            libVer = rw::KnownVersions::getGameVersion( rw::KnownVersions::BULLY );
        }
        else
        {
            hasGameVer = false;
        }

        if ( hasGameVer )
        {
            std::string verString =
                std::to_string( libVer.rwLibMajor ) + "." +
                std::to_string( libVer.rwLibMinor ) + "." +
                std::to_string( libVer.rwRevMajor ) + "." +
                std::to_string( libVer.rwRevMinor );

            std::string buildString;

            if ( libVer.buildNumber != 0xFFFF )
            {
                std::stringstream hex_stream;

                hex_stream << std::hex << libVer.buildNumber;

                buildString = hex_stream.str();
            }

            this->versionLineEdit->setText( QString::fromStdString( verString ) );
            this->buildLineEdit->setText( QString::fromStdString( buildString ) );
        }
        
        // We want to update the accessibility.
        this->UpdateAccessibility();
    }

    void OnRequestAccept( bool clicked )
    {
        // Set the version and close.
        rw::LibraryVersion libVer;

        bool hasVersion = this->GetSelectedVersion( libVer );

        if ( !hasVersion )
            return;

        // Set the version of the entire TXD.
        // Also patch the platform if feasible.
        if ( rw::TexDictionary *currentTXD = this->mainWnd->currentTXD )
        {
            bool patchD3D8 = false;
            bool patchD3D9 = false;
            bool shouldPatch = true;

            // TODO: maybe make this "fix" optional.

            QString currentGame = this->gameSelectBox->currentText();

            if ( currentGame == "GTA San Andreas" )
            {
                patchD3D8 = true;
            }
            else if ( currentGame == "GTA Vice City" ||
                      currentGame == "GTA III" ||
                      currentGame == "Manhunt" )
            {
                patchD3D9 = true;
            }
            else
            {
                shouldPatch = false;
            }

            currentTXD->SetEngineVersion( libVer );

            bool didPatchPlatform = false;

            // Also have to set the version of each texture.
            for ( rw::TexDictionary::texIter_t iter( currentTXD->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
            {
                rw::TextureBase *texHandle = iter.Resolve();

                texHandle->SetEngineVersion( libVer );

                // Maybe change platform.
                if ( shouldPatch )
                {
                    rw::Raster *texRaster = texHandle->GetRaster();
                    
                    if ( texRaster )
                    {
                        const char *texPlatform = texRaster->getNativeDataTypeName();

                        const char *newTarget = NULL;

                        if ( patchD3D8 && strcmp( texPlatform, "Direct3D8" ) == 0 )
                        {
                            newTarget = "Direct3D9";
                        }
                        else if ( patchD3D9 && strcmp( texPlatform, "Direct3D9" ) == 0 )
                        {
                            newTarget = "Direct3D8";
                        }

                        if ( newTarget )
                        {
                            try
                            {
                                bool hasChanged = rw::ConvertRasterTo( texRaster, newTarget );

                                if ( hasChanged )
                                {
                                    didPatchPlatform = true;
                                }
                            }
                            catch( rw::RwException& except )
                            {
                                // We output a warning.
                                this->mainWnd->txdLog->addLogMessage(
                                    QString( "failed to adjust texture platform of " ) + QString::fromStdString( texHandle->GetName() ) + QString( ": " ) + QString::fromStdString( except.message ),
                                    LOGMSG_WARNING
                                );

                                // Continue anyway.
                            }
                        }
                    }
                }
            }

            // The user might want to be notified of the platform change.
            if ( didPatchPlatform )
            {
                this->mainWnd->txdLog->addLogMessage( "changed the TXD platform to match version", LOGMSG_INFO );

                // Also update texture item info, because it may have changed.
                this->mainWnd->updateAllTextureMetaInfo();

                // The visuals of the texture _may_ have changed.
                this->mainWnd->updateTextureView();
            }

            // Done. :)
        }

        // Update the MainWindow stuff.
        this->mainWnd->updateWindowTitle();

        // Since the version has changed, the friendly icons should have changed.
        this->mainWnd->updateFriendlyIcons();

        this->close();
    }

    void OnRequestCancel( bool clicked )
    {
        this->close();
    }

public:
	RwVersionDialog( MainWindow *mainWnd ) : QDialog( mainWnd ) {
		setObjectName("background_1");
		setWindowTitle(tr("TXD Version Setup"));
        setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );
        setAttribute( Qt::WA_DeleteOnClose );

        setWindowModality( Qt::WindowModal );

        this->mainWnd = mainWnd;

		QVBoxLayout *verticalLayout = new QVBoxLayout(this);

		QVBoxLayout *topLayout = new QVBoxLayout;
		topLayout->setSpacing(6);
		topLayout->setMargin(10);

		QHBoxLayout *selectGameLayout = new QHBoxLayout;
		QLabel *gameLabel = new QLabel(tr("Game"));
		gameLabel->setObjectName("label25px");
		gameLabel->setFixedWidth(70);
		QComboBox *gameComboBox = new QComboBox;
		gameComboBox->addItem("GTA San Andreas");
		gameComboBox->addItem("GTA Vice City");
		gameComboBox->addItem("GTA III");
		gameComboBox->addItem("Manhunt");
        gameComboBox->addItem("Bully");
		gameComboBox->addItem("Custom");

        this->gameSelectBox = gameComboBox;

        connect( gameComboBox, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &RwVersionDialog::OnChangeSelectedGame );

		selectGameLayout->addWidget(gameLabel);
		selectGameLayout->addWidget(gameComboBox);

		QHBoxLayout *versionLayout = new QHBoxLayout;
		QLabel *versionLabel = new QLabel(tr("Version"));
		versionLabel->setObjectName("label25px");

		QHBoxLayout *versionNumbersLayout = new QHBoxLayout;
		QLineEdit *versionLine1 = new QLineEdit;
		//versionLine1->setValidator(new QIntValidator(3, 6, this));
		versionLine1->setInputMask("0.00.00.00");
		//versionLine1->setFixedWidth(24);
		//versionLine1->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine2 = new QLineEdit;
		//versionLine2->setInputMask("00");
		//versionLine2->setValidator(new QIntValidator(0, 15, this));
		//versionLine2->setFixedWidth(24);
		//versionLine2->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine3 = new QLineEdit;
		//versionLine3->setInputMask("00");
		//versionLine3->setValidator(new QIntValidator(0, 15, this));
		//versionLine3->setFixedWidth(24);
		//versionLine3->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine4 = new QLineEdit;
		//versionLine4->setInputMask("00");
		//versionLine4->setValidator(new QIntValidator(0, 63, this));
		//versionLine4->setFixedWidth(24);
		//versionLine4->setAlignment(Qt::AlignCenter);

		versionNumbersLayout->addWidget(versionLine1);
		//versionNumbersLayout->addWidget(versionLine2);
		//versionNumbersLayout->addWidget(versionLine3);
		//versionNumbersLayout->addWidget(versionLine4);
		//versionNumbersLayout->setSpacing(2);
		versionNumbersLayout->setMargin(0);

        this->versionLineEdit = versionLine1;

        connect( versionLine1, &QLineEdit::textChanged, this, &RwVersionDialog::OnChangeVersion );

		QLabel *buildLabel = new QLabel(tr("Build"));
		buildLabel->setObjectName("label25px");
		QLineEdit *buildLine = new QLineEdit;
		buildLine->setInputMask("HHHH");
        buildLine->clear();
		buildLine->setFixedWidth(50);

        this->buildLineEdit = buildLine;

		versionLayout->addWidget(versionLabel);
		versionLayout->addLayout(versionNumbersLayout);
		versionLayout->addWidget(buildLabel);
		versionLayout->addWidget(buildLine);

		topLayout->addLayout(selectGameLayout);
		topLayout->addSpacing(8);
		topLayout->addLayout(versionLayout);

		QWidget *line = new QWidget();
		line->setFixedHeight(1);
		line->setObjectName("hLineBackground");

		QHBoxLayout *buttonsLayout = new QHBoxLayout;
		QPushButton *buttonAccept = new QPushButton(tr("Accept"));
		QPushButton *buttonCancel = new QPushButton(tr("Cancel"));

        this->applyButton = buttonAccept;

        connect( buttonAccept, &QPushButton::clicked, this, &RwVersionDialog::OnRequestAccept );
        connect( buttonCancel, &QPushButton::clicked, this, &RwVersionDialog::OnRequestCancel );

		buttonsLayout->addWidget(buttonAccept);
		buttonsLayout->addWidget(buttonCancel);
		buttonsLayout->setAlignment(Qt::AlignRight);
		buttonsLayout->setMargin(10);
		buttonsLayout->setSpacing(6);
		
		verticalLayout->addLayout(topLayout);
		verticalLayout->addSpacing(12);
		verticalLayout->addWidget(line);
		verticalLayout->addLayout(buttonsLayout);
		verticalLayout->setSpacing(0);
		verticalLayout->setMargin(0);

		verticalLayout->setSizeConstraint(QLayout::SetFixedSize);

        // Initiate the ready dialog.
        this->OnChangeSelectedGame( gameComboBox->currentText() );
	}

    ~RwVersionDialog( void )
    {
        // There can only be one version dialog.
        this->mainWnd->verDlg = NULL;
    }
};