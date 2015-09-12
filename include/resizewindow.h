#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

struct TexResizeWindow : public QDialog
{
    inline TexResizeWindow( MainWindow *mainWnd, TexInfoWidget *texInfo ) : QDialog( mainWnd )
    {
        this->setWindowTitle( "Texture Resize" );
        this->setWindowFlags( this->windowFlags() & ~Qt::WindowContextHelpButtonHint );

        this->mainWnd = mainWnd;
        this->texInfo = texInfo;

        this->setWindowModality( Qt::WindowModal );
        this->setAttribute( Qt::WA_DeleteOnClose );

        // Get the current texture dimensions.
        rw::uint32 curWidth = 0;
        rw::uint32 curHeight = 0;

        rw::rasterSizeRules rasterRules;
        {
            if ( texInfo )
            {
                if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
                {
                    if ( rw::Raster *texRaster = texHandle->GetRaster() )
                    {
                        try
                        {
                            // Getting the size may fail if there is no native data.
                            texRaster->getSize( curWidth, curHeight );

                            // Also get the size rules.
                            texRaster->getSizeRules( rasterRules );
                        }
                        catch( rw::RwException& except )
                        {
                            // Got no dimms.
                            curWidth = 0;
                            curHeight = 0;
                        }
                    }
                }
            }
        }

        // We want a basic dialog with two dimension line edits and our typical two buttons.
        QVBoxLayout *rootLayout = new QVBoxLayout( this );

        rootLayout->setSizeConstraint( QLayout::SetFixedSize );

        // A form layout might do it.
        QFormLayout *dimmForm = new QFormLayout();

        // We only want to accept unsigned integers.
        QIntValidator *dimensionValidator = new QIntValidator( 1, ( rasterRules.maximum ? rasterRules.maxVal : 4096 ), this );

        QLineEdit *widthEdit = new QLineEdit( QString::fromStdString( std::to_string( curWidth ) ) );
        widthEdit->setValidator( dimensionValidator );

        this->widthEdit = widthEdit;

        connect( widthEdit, &QLineEdit::textChanged, this, &TexResizeWindow::OnChangeDimensionProperty );

        QLineEdit *heightEdit = new QLineEdit( QString::fromStdString( std::to_string( curHeight ) ) );
        heightEdit->setValidator( dimensionValidator );

        this->heightEdit = heightEdit;

        connect( heightEdit, &QLineEdit::textChanged, this, &TexResizeWindow::OnChangeDimensionProperty );

        dimmForm->addRow( new QLabel( "Width:" ), widthEdit );
        dimmForm->addRow( new QLabel( "Height:" ), heightEdit );

        rootLayout->addLayout( dimmForm );

        // Make some space.
        dimmForm->setContentsMargins( QMargins( 0, 0, 0, 10 ) );

        // Now the buttons, I guess.
        QHBoxLayout *buttonRow = new QHBoxLayout();

        QPushButton *buttonSet = new QPushButton( "Set" );
        buttonRow->addWidget( buttonSet );

        this->buttonSet = buttonSet;

        connect( buttonSet, &QPushButton::clicked, this, &TexResizeWindow::OnRequestSet );

        QPushButton *buttonCancel = new QPushButton( "Cancel" );
        buttonRow->addWidget( buttonCancel );

        connect( buttonCancel, &QPushButton::clicked, this, &TexResizeWindow::OnRequestCancel );

        rootLayout->addLayout( buttonRow );

        // Remember us as the only resize dialog.
        mainWnd->resizeDlg = this;

        // Initialize the dialog.
        this->UpdateAccessibility();
    }

    inline ~TexResizeWindow( void )
    {
        // There can be only one.
        this->mainWnd->resizeDlg = NULL;
    }

public slots:
    void OnChangeDimensionProperty( const QString& newText )
    {
        this->UpdateAccessibility();
    }

    void OnRequestSet( bool checked )
    {
        // Do the resize.
        bool shouldClose = true;

        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
            {
                if ( rw::Raster *texRaster = texHandle->GetRaster() )
                {
                    // Fetch the sizes (with minimal validation).
                    QString widthDimmString = this->widthEdit->text();
                    QString heightDimmString = this->heightEdit->text();

                    bool validWidth, validHeight;

                    int widthDimm = widthDimmString.toInt( &validWidth );
                    int heightDimm = heightDimmString.toInt( &validHeight );

                    if ( validWidth && validHeight )
                    {
                        // Resize!
                        rw::uint32 rwWidth = (rw::uint32)widthDimm;
                        rw::uint32 rwHeight = (rw::uint32)heightDimm;

                        bool success = false;

                        try
                        {
                            // Use default filters.
                            texRaster->resize( rwWidth, rwHeight );

                            success = true;
                        }
                        catch( rw::RwException& except )
                        {
                            this->mainWnd->txdLog->showError( QString( "failed to resize raster: " ) + QString::fromStdString( except.message ) );

                            // We should not close the dialog.
                            shouldClose = false;
                        }

                        if ( success )
                        {
                            // Since we succeeded, we should update the view and things.
                            this->mainWnd->updateTextureView();

                            texInfo->updateInfo();
                        }
                    }
                }
            }
        }

        if ( shouldClose )
        {
            this->close();
        }
    }

    void OnRequestCancel( bool checked )
    {
        this->close();
    }

private:
    void UpdateAccessibility( void )
    {
        // Only allow setting if we have a width and height, whose values are different from the original.
        bool allowSet = true;

        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            if ( rw::TextureBase *texHandle = texInfo->GetTextureHandle() )
            {
                if ( rw::Raster *texRaster = texHandle->GetRaster() )
                {
                    rw::uint32 curWidth, curHeight;

                    try
                    {
                        texRaster->getSize( curWidth, curHeight );
                    }
                    catch( rw::RwException& )
                    {
                        curWidth = 0;
                        curHeight = 0;
                    }

                    // Check we have dimensions currenctly.
                    bool gotValidDimms = false;

                    QString widthDimmString = this->widthEdit->text();
                    QString heightDimmString = this->heightEdit->text();

                    if ( widthDimmString.isEmpty() == false && heightDimmString.isEmpty() == false )
                    {
                        bool validWidth, validHeight;

                        int widthDimm = widthDimmString.toInt( &validWidth );
                        int heightDimm = heightDimmString.toInt( &validHeight );

                        if ( validWidth && validHeight )
                        {
                            if ( widthDimm > 0 && heightDimm > 0 )
                            {
                                // Also verify whether the native texture can even accept the dimensions.
                                // Otherwise we get a nice red present.
                                rw::rasterSizeRules rasterRules;

                                texRaster->getSizeRules( rasterRules );

                                if ( rasterRules.verifyDimensions( widthDimm, heightDimm ) )
                                {
                                    // Now lets verify that those are different.
                                    rw::uint32 selWidth = (rw::uint32)widthDimm;
                                    rw::uint32 selHeight = (rw::uint32)heightDimm;

                                    if ( selWidth != curWidth || selHeight != curHeight )
                                    {
                                        gotValidDimms = true;
                                    }
                                }
                            }
                        }
                    }
                    
                    if ( !gotValidDimms )
                    {
                        allowSet = false;
                    }
                }
            }
        }

        this->buttonSet->setDisabled( !allowSet );
    }

    MainWindow *mainWnd;

    TexInfoWidget *texInfo;

    QPushButton *buttonSet;
    QLineEdit *widthEdit;
    QLineEdit *heightEdit;
};