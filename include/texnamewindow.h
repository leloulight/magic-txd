#pragma once

#include <QDialog>
#include "qtutils.h"
#include "languages.h"

struct TexNameWindow : public QDialog, public magicTextLocalizationItem
{
    inline TexNameWindow( MainWindow *mainWnd, TexInfoWidget *texInfo ) : QDialog( mainWnd )
    {
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
        MagicLayout<QHBoxLayout> layout(this);
        layout.top->addWidget(CreateLabelL( "Main.Rename.Name" ));
        this->texNameEdit = new QLineEdit(curTexName);
        layout.top->addWidget(this->texNameEdit);
        this->texNameEdit->setMaxLength(32);
        this->texNameEdit->setMinimumWidth(350);

        connect(this->texNameEdit, &QLineEdit::textChanged, this, &TexNameWindow::OnUpdateTexName);

        this->buttonSet = CreateButtonL("Main.Rename.Set");
        QPushButton *buttonCancel = CreateButtonL("Main.Rename.Cancel");

        connect(this->buttonSet, &QPushButton::clicked, this, &TexNameWindow::OnRequestSet);
        connect(buttonCancel, &QPushButton::clicked, this, &TexNameWindow::OnRequestCancel);

        layout.bottom->addWidget(this->buttonSet);
        layout.bottom->addWidget(buttonCancel);
        this->mainWnd->texNameDlg = this;
        // Initialize the window.
        this->UpdateAccessibility();

        RegisterTextLocalizationItem( this );
    }

    inline ~TexNameWindow( void )
    {
        UnregisterTextLocalizationItem( this );

        // There can be only one texture name dialog.
        this->mainWnd->texNameDlg = NULL;
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        this->setWindowTitle( getLanguageItemByKey( "Main.Rename.Desc" ) );
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