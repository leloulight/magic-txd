#include "mainwindow.h"

#include "qtrwutils.hxx"

static const bool _lockdownPlatform = true;
static const size_t _recommendedPlatformMaxName = 32;
static const bool _enableMaskName = false;

inline QString calculateImageBaseName( QString fileName )
{
    // Determine the texture name.
    QFileInfo fileInfo( fileName );

    return fileInfo.baseName();
}

TexAddDialog::TexAddDialog( MainWindow *mainWnd, QString pathToImage, TexAddDialog::operationCallback_t cb ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    this->cb = cb;
    this->imgPath = pathToImage;

    this->setAttribute( Qt::WA_DeleteOnClose );
    this->setWindowModality( Qt::WindowModality::WindowModal );
    
    // We may have a recommended set of properties that we want to start out with.
    rw::eRasterFormat recommendedRasterFormat = rw::RASTER_8888;
    rw::uint32 recommendedPreviewWidth = 0;
    rw::uint32 recommendedPreviewHeight = 0;
    bool hasRecommendedRasterFormat = false;

    // Load the pixmap that is associated with the image path.
    bool hasPreview = false;

    try
    {
        std::wstring unicodePathToImage = pathToImage.toStdWString();

        rw::streamConstructionFileParamW_t wparam( unicodePathToImage.c_str() );

        rw::Stream *imgStream = mainWnd->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &wparam );

        if ( imgStream )
        {
            try
            {
                // Load it.
                rw::Bitmap rwBmp;

                bool deserializeSuccess = rw::DeserializeImage( imgStream, rwBmp );

                if ( deserializeSuccess )
                {
                    this->pixelsToAdd = convertRWBitmapToQPixmap( rwBmp );

                    // We now have a recommended raster format.
                    recommendedRasterFormat = rwBmp.getFormat();
                    recommendedPreviewWidth = rwBmp.getWidth();
                    recommendedPreviewHeight = rwBmp.getHeight();

                    hasRecommendedRasterFormat = true;

                    hasPreview = true;
                }
                // If deserialization failed, it might still succeed as mipmap.
                // Very unlikely, but yes.
            }
            catch( ... )
            {
                mainWnd->rwEngine->DeleteStream( imgStream );

                throw;
            }

            mainWnd->rwEngine->DeleteStream( imgStream );
        }
    }
    catch( rw::RwException& err )
    {
        // We do not care.
        // We simply failed to get a preview.
        hasPreview = false;

        // Probably should tell the user about this error, so we can fix it.
        mainWnd->txdLog->showError( QString( "error while building preview: " ) + QString::fromStdString( err.message ) );
    }

    // Calculate an appropriate texture name.
    QString textureBaseName = calculateImageBaseName( pathToImage );

    // TODO: verify the texture name is full ANSI.

    this->setWindowTitle( "Add Texture..." );

    // Create our GUI interface.
    QHBoxLayout *rootHoriLayout = new QHBoxLayout();

    // Add a form to the left, a preview to the right.
    QTabWidget *formHolderWidget = new QTabWidget();

    int req_width = 220;
    int req_height = 320;

    if ( hasPreview )
    {
        // If we have a preview window, we should leave space for the preview pane.
        formHolderWidget->setMaximumWidth( 300 );

        req_width += 300;
    }

    // Add the contents of the form.
    {
        QWidget *generalTab = new QWidget();

        QVBoxLayout *generalTabRootLayout = new QVBoxLayout();

        generalTabRootLayout->setContentsMargins( 5, 5, 5, 5 );

        QFormLayout *formLayout = new QFormLayout();

        QLineEdit *texNameEdit = new QLineEdit( textureBaseName );

        texNameEdit->setMaxLength( _recommendedPlatformMaxName );

        this->textureNameEdit = texNameEdit;

        formLayout->addRow( new QLabel( "Texture Name:" ), texNameEdit );

        if ( _enableMaskName )
        {
            QLineEdit *texMaskNameEdit = new QLineEdit();

            texMaskNameEdit->setMaxLength( _recommendedPlatformMaxName );

            formLayout->addRow( new QLabel( "Mask Name:" ), texMaskNameEdit );

            this->textureMaskNameEdit = texMaskNameEdit;
        }
        else
        {
            this->textureMaskNameEdit = NULL;
        }

        // If the current TXD already has a platform, we disable editting this platform and simply use it.
        const char *currentForcedPlatform = mainWnd->GetTXDPlatformString( mainWnd->currentTXD );

        QWidget *platformDisplayWidget;

        QString curPlatformText;

        if ( _lockdownPlatform == false || currentForcedPlatform == NULL )
        {
            QComboBox *platformComboBox = new QComboBox();

            // Fill out the combo box with available platforms.
            {
                rw::platformTypeNameList_t platforms = rw::GetAvailableNativeTextureTypes( mainWnd->rwEngine );

                for ( rw::platformTypeNameList_t::const_reverse_iterator iter = platforms.crbegin(); iter != platforms.crend(); iter++ )
                {
                    const std::string& platName = *iter;

                    platformComboBox->addItem( platName.c_str() );
                }
            }

            connect( platformComboBox, (void (QComboBox::*)(const QString&))&QComboBox::activated, this, &TexAddDialog::OnPlatformSelect );

            platformDisplayWidget = platformComboBox;

            curPlatformText = platformComboBox->currentText();
        }
        else
        {
            // We do not want to allow editing.
            QLineEdit *platformDisplayEdit = new QLineEdit( currentForcedPlatform );

            platformDisplayEdit->setDisabled( true );

            platformDisplayWidget = platformDisplayEdit;

            curPlatformText = platformDisplayEdit->text();
        }

        formLayout->addRow( new QLabel( "Platform:" ), platformDisplayWidget );

        // We want to leave some space after the platform stuff.
        formLayout->setContentsMargins( 0, 0, 0, 25 );

        generalTabRootLayout->addLayout( formLayout );

        // Add a new section for platform properties.
        QGroupBox *platformPropertiesGroupBox = new QGroupBox();

        platformPropertiesGroupBox->setTitle( "Platform Properties" );

        platformPropertiesGroupBox->setContentsMargins( 0, 0, 0, 15 );

        // Set the contents of the platform properties group box.
        {
            QFormLayout *groupContentFormLayout = new QFormLayout();

            this->platformPropForm = groupContentFormLayout;

            QRadioButton *rawRasterToggle = new QRadioButton( "Raw Raster" );

            this->platformRawRasterToggle = rawRasterToggle;

            rawRasterToggle->setChecked( true );

            connect( rawRasterToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle );

            groupContentFormLayout->addRow( rawRasterToggle );

            this->platformRawRasterProp = rawRasterToggle;

            QRadioButton *compressionFormatToggle = new QRadioButton( "compressed" );

            this->platformCompressionToggle = compressionFormatToggle;

            connect( compressionFormatToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle );

            QComboBox *compressionFormatSelect = new QComboBox();

            groupContentFormLayout->addRow( compressionFormatToggle, compressionFormatSelect );

            this->platformCompressionSelectProp = compressionFormatSelect;

            QRadioButton *paletteFormatToggle = new QRadioButton( "palettized" );

            this->platformPaletteToggle = paletteFormatToggle;

            connect( paletteFormatToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle );

            QComboBox *paletteFormatSelect = new QComboBox();

            paletteFormatSelect->addItem( "PAL4" );
            paletteFormatSelect->addItem( "PAL8" );

            groupContentFormLayout->addRow( paletteFormatToggle, paletteFormatSelect );

            this->platformPaletteSelectProp = paletteFormatSelect;

            QComboBox *pixelFormatSelect = new QComboBox();

            // TODO: add API to fetch actually supported raster formats for a native texture.
            // even though RenderWare may have added a bunch of raster formats, the native textures
            // are completely liberal in inplementing any or not.

            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_1555 ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_565 ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_4444 ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_LUM ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_8888 ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_888 ) );
            pixelFormatSelect->addItem( rw::GetRasterFormatStandardName( rw::RASTER_555 ) );

            // Select the recommended raster format.
            if ( hasRecommendedRasterFormat )
            {
                const char *recommendedFormatName = rw::GetRasterFormatStandardName( recommendedRasterFormat );

                if ( recommendedFormatName )
                {
                    pixelFormatSelect->setCurrentText( recommendedFormatName );
                }
            }

            groupContentFormLayout->addRow( new QLabel( "Pixel Format:" ), pixelFormatSelect );

            this->platformPixelFormatSelectProp = pixelFormatSelect;

            platformPropertiesGroupBox->setLayout( groupContentFormLayout );
        }

        this->platformPropsGroup = platformPropertiesGroupBox;

        generalTabRootLayout->addWidget( platformPropertiesGroupBox );

        // Add some basic properties that exist no matter what platform.
        QCheckBox *generateMipmapsToggle = new QCheckBox( "Generate mipmaps" );

        this->propGenerateMipmaps = generateMipmapsToggle;

        generalTabRootLayout->addWidget( generateMipmapsToggle );

        generalTabRootLayout->addStretch();

        // Add control buttons at the bottom.
        QHBoxLayout *bottomControlButtonsGroup = new QHBoxLayout();

        bottomControlButtonsGroup->setContentsMargins( 40, 0, 10, 10 );

        QPushButton *cancelButton = new QPushButton( "Cancel" );

        connect( cancelButton, &QPushButton::clicked, this, &TexAddDialog::OnCloseRequest );

        bottomControlButtonsGroup->addWidget( cancelButton );

        QPushButton *addButton = new QPushButton( "Add" );

        connect( addButton, &QPushButton::clicked, this, &TexAddDialog::OnTextureAddRequest );

        bottomControlButtonsGroup->addWidget( addButton );

        generalTabRootLayout->addLayout( bottomControlButtonsGroup );

        generalTab->setLayout( generalTabRootLayout );

        formHolderWidget->addTab( generalTab, "Properties" );

        // Do initial stuff.
        {
            if ( curPlatformText.isEmpty() == false )
            {
                this->OnPlatformSelect( curPlatformText );
            }
        }
    }

    rootHoriLayout->addWidget( formHolderWidget );

    this->setLayout( rootHoriLayout );

    if ( hasPreview )
    {
        QGroupBox *previewGroupBox = new QGroupBox();

        pixelPreviewWidget *previewWidget = new pixelPreviewWidget( this );

        previewWidget->recommendedWidth = (int)recommendedPreviewWidth;
        previewWidget->recommendedHeight = (int)recommendedPreviewHeight;
        previewWidget->requiredHeight = this->sizeHint().height();

        QVBoxLayout *groupBoxLayout = new QVBoxLayout();

        groupBoxLayout->addWidget( previewWidget );

        previewGroupBox->setLayout( groupBoxLayout );

        rootHoriLayout->addWidget( previewGroupBox );
    }

    //this->resize( req_width, req_height );
}

void TexAddDialog::UpdateAccessability( void )
{
    // We have to disable or enable certain platform property selectable fields if you toggle a certain format type.
    // This is to guide the user into the right direction, that a native texture cannot have multiple format types at once.

    bool wantsPixelFormatAccess = false;
    bool wantsCompressionAccess = false;
    bool wantsPaletteAccess = false;

    if ( this->platformRawRasterToggle->isChecked() )
    {
        wantsPixelFormatAccess = true;
    }
    else if ( this->platformCompressionToggle->isChecked() )
    {
        wantsCompressionAccess = true;
    }
    else if ( this->platformPaletteToggle->isChecked() )
    {
        wantsPixelFormatAccess = true;
        wantsPaletteAccess = true;
    }

    // Now disable or enable stuff.
    this->platformPixelFormatSelectProp->setDisabled( !wantsPixelFormatAccess );
    this->platformCompressionSelectProp->setDisabled( !wantsCompressionAccess );
    this->platformPaletteSelectProp->setDisabled( !wantsPaletteAccess );

    // TODO: maybe clear combo boxes aswell?
}

void TexAddDialog::OnPlatformFormatTypeToggle( bool checked )
{
    // Since we switched the platform format type, we have to adjust the accessability.
    // The accessability change must not swap items around on the GUI. Rather it should
    // disable items that make no sense.
    this->UpdateAccessability();
}

void TexAddDialog::OnPlatformSelect( const QString& newText )
{
    // We want to show the user properties based on what this platform supports.
    // So we fill the fields.

    std::string ansiNativeName = newText.toStdString();

    rw::nativeRasterFormatInfo formatInfo;

    bool gotFormatInfo = rw::GetNativeTextureFormatInfo( this->mainWnd->rwEngine, ansiNativeName.c_str(), formatInfo );

    // Decide what to do.
    bool enableRawRaster = true;
    bool enableCompressSelect = false;
    bool enablePaletteSelect = false;
    bool enablePixelFormatSelect = true;

    bool isOnlyCompression = false;

    bool supportsDXT1 = true;
    bool supportsDXT2 = true;
    bool supportsDXT3 = true;
    bool supportsDXT4 = true;
    bool supportsDXT5 = true;

    if ( gotFormatInfo )
    {
        if ( formatInfo.isCompressedFormat )
        {
            // We are a fixed compressed format, so we will pass pixels with high quality to the pipeline.
            enableRawRaster = true;
            enableCompressSelect = true;    // decide later.
            enablePaletteSelect = false;
            enablePixelFormatSelect = false;

            isOnlyCompression = true;
        }
        else
        {
            // We are a dynamic raster, so whatever goes best.
            enableRawRaster = true;
            enableCompressSelect = true;    // we decide this later again.
            enablePaletteSelect = formatInfo.supportsPalette;
            enablePixelFormatSelect = true;
        }

        supportsDXT1 = formatInfo.supportsDXT1;
        supportsDXT2 = formatInfo.supportsDXT2;
        supportsDXT3 = formatInfo.supportsDXT3;
        supportsDXT4 = formatInfo.supportsDXT4;
        supportsDXT5 = formatInfo.supportsDXT5;
    }

    // Decide whether enabling the compression select even makes sense.
    // If we have no compression supported, then it makes no sense.
    if ( enableCompressSelect )
    {
        enableCompressSelect =
            ( supportsDXT1 || supportsDXT2 || supportsDXT3 || supportsDXT4 || supportsDXT5 );
    }

    if ( enableCompressSelect && isOnlyCompression )
    {
        enableRawRaster = false;
    }

    // Do stuff.
    this->platformRawRasterProp->setVisible( enableRawRaster );

    if ( QWidget *partnerWidget = this->platformPropForm->labelForField( this->platformRawRasterProp ) )
    {
        partnerWidget->setVisible( enableRawRaster );
    }

    this->platformCompressionSelectProp->setVisible( enableCompressSelect );

    if ( QWidget *partnerWidget = this->platformPropForm->labelForField( this->platformCompressionSelectProp ) )
    {
        partnerWidget->setVisible( enableCompressSelect );
    }

    this->platformPaletteSelectProp->setVisible( enablePaletteSelect );

    if ( QWidget *partnerWidget = this->platformPropForm->labelForField( this->platformPaletteSelectProp ) )
    {
        partnerWidget->setVisible( enablePaletteSelect );
    }

    this->platformPixelFormatSelectProp->setVisible( enablePixelFormatSelect );

    if ( QWidget *partnerWidget = this->platformPropForm->labelForField( this->platformPixelFormatSelectProp ) )
    {
        partnerWidget->setVisible( enablePixelFormatSelect );
    }

    // If we have nothing mandatory visible in the platform selection window, we can just hide it.
    bool showPlatformGroup = ( enableRawRaster || enableCompressSelect || enablePaletteSelect );

    this->platformPropsGroup->setVisible( showPlatformGroup );

    // Fill in fields depending on capabilities.
    if ( enableCompressSelect )
    {
        this->platformCompressionSelectProp->clear();

        if ( supportsDXT1 )
        {
            this->platformCompressionSelectProp->addItem( "DXT1" );
        }

        if ( supportsDXT2 )
        {
            this->platformCompressionSelectProp->addItem( "DXT2" );
        }

        if ( supportsDXT3 )
        {
            this->platformCompressionSelectProp->addItem( "DXT3" );
        }

        if ( supportsDXT4 )
        {
            this->platformCompressionSelectProp->addItem( "DXT4" );
        }

        if ( supportsDXT5 )
        {
            this->platformCompressionSelectProp->addItem( "DXT5" );
        }
    }

    // If none of the visible toggles are select, select the first visible toggle.
    bool anyToggle = false;

    if ( this->platformRawRasterToggle->isVisible() && this->platformRawRasterToggle->isChecked() )
    {
        anyToggle = true;
    }

    if ( !anyToggle )
    {
        if ( this->platformCompressionToggle->isVisible() && this->platformCompressionToggle->isChecked() )
        {
            anyToggle = true;
        }
    }

    if ( !anyToggle )
    {
        if ( this->platformPaletteToggle->isVisible() && this->platformPaletteToggle->isChecked() )
        {
            anyToggle = true;
        }
    }

    // Now select the first one if we still have no selected toggle.
    if ( !anyToggle )
    {
        bool selectedToggle = false;

        if ( this->platformRawRasterToggle->isVisible() )
        {
            this->platformRawRasterToggle->setChecked( true );

            selectedToggle = true;
        }
        
        if ( !selectedToggle )
        {
            if ( this->platformCompressionToggle->isVisible() )
            {
                this->platformCompressionToggle->setChecked( true );

                selectedToggle = true;
            }
        }

        if ( !selectedToggle )
        {
            if ( this->platformPaletteToggle->isVisible() )
            {
                this->platformPaletteToggle->setChecked( true );

                selectedToggle = true;
            }
        }

        // Well, we do not _have_ to select one.
    }

    // Update what options make sense to the user.
    this->UpdateAccessability();
}

void TexAddDialog::OnTextureAddRequest( bool checked )
{
    // This is where we want to go.
    // Decide the format that the runtime has requested.

    try
    {
        rw::eCompressionType compressionType = rw::RWCOMPRESS_NONE;

        texAddOperation desc;
        desc.texName = this->textureNameEdit->text().toStdString();
    
        if ( this->textureMaskNameEdit )
        {
            desc.maskName = this->textureMaskNameEdit->text().toStdString();
        }

        desc.imgPath = this->imgPath;
    
        // Now for the properties.
        if ( this->platformCompressionToggle->isChecked() )
        {
            // We are a compressed format, so determine what we actually are.
            QString selectedCompression = this->platformCompressionSelectProp->currentText();

            if ( selectedCompression == "DXT1" )
            {
                desc.compressionType = rw::RWCOMPRESS_DXT1;
            }
            else if ( selectedCompression == "DXT2" )
            {
                desc.compressionType = rw::RWCOMPRESS_DXT2;
            }
            else if ( selectedCompression == "DXT3" )
            {
                desc.compressionType = rw::RWCOMPRESS_DXT3;
            }
            else if ( selectedCompression == "DXT4" )
            {
                desc.compressionType = rw::RWCOMPRESS_DXT4;
            }
            else if ( selectedCompression == "DXT5" )
            {
                desc.compressionType = rw::RWCOMPRESS_DXT5;
            }
            else
            {
                throw std::exception( "invalid compression type selected" );
            }

            desc.rasterFormat = rw::RASTER_DEFAULT;
            desc.paletteType = rw::PALETTE_NONE;
        }
        else
        {
            desc.compressionType = rw::RWCOMPRESS_NONE;

            // Now we have a valid raster format selected in the pixel format combo box.
            // We kinda need one.
            {
                QString formatName = this->platformPixelFormatSelectProp->currentText();

                std::string ansiFormatName = formatName.toStdString();

                desc.rasterFormat = rw::FindRasterFormatByName( ansiFormatName.c_str() );

                if ( desc.rasterFormat == rw::RASTER_DEFAULT )
                {
                    throw std::exception( "invalid pixel format selected" );
                }
            }

            // And then we need to know whether it should be a palette or not.
            if ( this->platformPaletteToggle->isChecked() )
            {
                // Alright, then we have to fetch a valid palette type.
                QString paletteName = this->platformPaletteSelectProp->currentText();

                if ( paletteName == "PAL4" )
                {
                    // TODO: some archictures might prefer the MSB version.
                    // we should detect that automatically!

                    desc.paletteType = rw::PALETTE_4BIT;
                }
                else if ( paletteName == "PAL8" )
                {
                    desc.paletteType = rw::PALETTE_8BIT;
                }
                else
                {
                    throw std::exception( "invalid palette type selected" );
                }
            }
            else
            {
                desc.paletteType = rw::PALETTE_NONE;
            }
        }

        // Add some basic properties that count all the time.
        desc.generateMipmaps = this->propGenerateMipmaps->isChecked();

        // Now we add the texture.
        // Error handling should be dealt with by the routine we push to.
        this->cb( desc );
    }
    catch( std::exception& except )
    {
        // If we failed to push data to the output stage.
        this->mainWnd->txdLog->showError( QString( "failed to create texture: " ) + except.what() );
    }

    // Close ourselves.
    this->close();
}

void TexAddDialog::OnCloseRequest( bool checked )
{
    // The user doesnt want to do it anymore.
    this->close();
}