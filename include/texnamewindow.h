#pragma once

#include <QDialog>

struct TexNameWindow : public QDialog
{
    inline TexNameWindow( MainWindow *mainWnd, TexInfoWidget *texInfo ) : QDialog( mainWnd )
    {
        this->setWindowTitle( "Texture Name" );
        this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

        this->mainWnd = mainWnd;
        this->texInfo = texInfo;
        
        this->setWindowModality( Qt::WindowModal );
        this->setAttribute( Qt::WA_DeleteOnClose );

        QString curTexName;
        {
            rw::TextureBase *texHandle = texInfo->GetTextureHandle();

            if ( texHandle )
            {
                curTexName = ansi_to_qt( texHandle->GetName() );
            }
        }

        // Fill things.
        QVBoxLayout *rootLayout = new QVBoxLayout( this );

        rootLayout->setContentsMargins( 0, 0, 0, 0 );
        rootLayout->setSpacing(0);
        rootLayout->setSizeConstraint( QLayout::SetFixedSize );

        // We want a line with the texture name.
        QHBoxLayout *texNameLayout = new QHBoxLayout();

        texNameLayout->setContentsMargins( QMargins( 12, 12, 12, 12 ) );
        texNameLayout->setSpacing(10);

        texNameLayout->addWidget( new QLabel( "Name:" ) );

        QLineEdit *texNameEdit = new QLineEdit( curTexName );
        texNameLayout->addWidget( texNameEdit );

        texNameEdit->setMaxLength( 32 );

        texNameEdit->setMinimumWidth(350);

        this->texNameEdit = texNameEdit;

        connect( texNameEdit, &QLineEdit::textChanged, this, &TexNameWindow::OnUpdateTexName );

        QWidget *line = new QWidget();
        line->setFixedHeight(1);
        line->setObjectName("hLineBackground");

        // Now we want a line with buttons.
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->setContentsMargins(QMargins(12, 12, 12, 12));
        buttonLayout->setAlignment(Qt::AlignBottom | Qt::AlignRight);
        buttonLayout->setSpacing(10);

        QPushButton *buttonSet = new QPushButton( "Set" );
        buttonSet->setMinimumWidth(90);

        QPushButton *buttonCancel = new QPushButton( "Cancel" );
        buttonCancel->setMinimumWidth(90);

        this->buttonSet = buttonSet;

        connect( buttonSet, &QPushButton::clicked, this, &TexNameWindow::OnRequestSet );
        connect( buttonCancel, &QPushButton::clicked, this, &TexNameWindow::OnRequestCancel );

        buttonLayout->addWidget( buttonSet );
        buttonLayout->addWidget( buttonCancel );

        QWidget *bottomWidget = new QWidget();
        bottomWidget->setObjectName("background_0");
        bottomWidget->setLayout(buttonLayout);

        rootLayout->addLayout( texNameLayout );
        rootLayout->addWidget( line );
        rootLayout->addWidget( bottomWidget );
        
        this->mainWnd->texNameDlg = this;

        // Initialize the window.
        this->UpdateAccessibility();
    }

    inline ~TexNameWindow( void )
    {
        // There can be only one texture name dialog.
        this->mainWnd->texNameDlg = NULL;
    }

public slots:
    void OnUpdateTexName( const QString& newText )
    {
        this->UpdateAccessibility();
    }

    void OnRequestSet( bool clicked )
    {
        // Attempt to change the name.
        QString texName = this->texNameEdit->text();

        if ( texName.isEmpty() )
            return;

        // TODO: verify if all ANSI.

        std::string ansiTexName = qt_to_ansi( texName );

        // Set it.
        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
            {
                texHandle->SetName( ansiTexName.c_str() );

                // Update the info item.
                texInfo->updateInfo();

                // Update texture list width
                texInfo->listItem->setSizeHint(QSize(mainWnd->textureListWidget->sizeHintForColumn(0), 54));
            }
        }

        this->close();
    }

    void OnRequestCancel( bool clicked )
    {
        this->close();
    }

private:
    inline void UpdateAccessibility( void )
    {
        // Only allow to push "Set" if we actually have a valid texture name.
        QString curTexName = texNameEdit->text();

        bool shouldAllowSet = ( curTexName.isEmpty() == false );

        if ( shouldAllowSet )
        {
            if ( TexInfoWidget *texInfo = this->texInfo )
            {
                if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
                {
                    // Setting an already set texture name makes no sense.
                    std::string ansiCurTexName = qt_to_ansi( curTexName );

                    if ( ansiCurTexName == texHandle->GetName() )
                    {
                        shouldAllowSet = false;
                    }
                }
            }
        }
        
        // TODO: validate the texture name aswell.

        this->buttonSet->setDisabled( !shouldAllowSet );
    }

    MainWindow *mainWnd;

    TexInfoWidget *texInfo;

    QLineEdit *texNameEdit;

    QPushButton *buttonSet;
};