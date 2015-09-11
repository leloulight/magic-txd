#pragma once

#include <QDialog>

struct TexNameWindow : public QDialog
{
    inline TexNameWindow( MainWindow *mainWnd, TexInfoWidget *texInfo ) : QDialog( mainWnd )
    {
        this->mainWnd = mainWnd;
        this->texInfo = texInfo;
        
        this->setWindowModality( Qt::WindowModal );
        this->setAttribute( Qt::WA_DeleteOnClose );

        QString curTexName;
        {
            rw::TextureBase *texHandle = texInfo->GetTextureHandle();

            if ( texHandle )
            {
                curTexName = QString::fromStdString( texHandle->GetName() );
            }
        }

        // Fill things.
        QVBoxLayout *rootLayout = new QVBoxLayout( this );

        rootLayout->setContentsMargins( QMargins( 25, 15, 25, 10 ) );
        rootLayout->setSizeConstraint( QLayout::SetFixedSize );

        // We want a line with the texture name.
        QHBoxLayout *texNameLayout = new QHBoxLayout();

        texNameLayout->setContentsMargins( QMargins( 0, 0, 0, 10 ) );

        texNameLayout->addWidget( new QLabel( "Name:" ) );

        QLineEdit *texNameEdit = new QLineEdit( curTexName );
        texNameLayout->addWidget( texNameEdit );

        texNameEdit->setMaxLength( 32 );

        this->texNameEdit = texNameEdit;

        connect( texNameEdit, &QLineEdit::textChanged, this, &TexNameWindow::OnUpdateTexName );

        // Now we want a line with buttons.
        QHBoxLayout *buttonLayout = new QHBoxLayout();

        QPushButton *buttonSet = new QPushButton( "Set" );
        QPushButton *buttonCancel = new QPushButton( "Cancel" );

        this->buttonSet = buttonSet;

        connect( buttonSet, &QPushButton::clicked, this, &TexNameWindow::OnRequestSet );
        connect( buttonCancel, &QPushButton::clicked, this, &TexNameWindow::OnRequestCancel );

        buttonLayout->addWidget( buttonSet );
        buttonLayout->addWidget( buttonCancel );

        rootLayout->addLayout( texNameLayout );
        rootLayout->addLayout( buttonLayout );
        
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

        std::string ansiTexName = texName.toStdString();

        // Set it.
        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
            {
                texHandle->SetName( ansiTexName.c_str() );

                // Update the info item.
                texInfo->updateInfo();
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
                    std::string ansiCurTexName = curTexName.toStdString();

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