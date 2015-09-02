#include "mainwindow.h"

#include "qtrwutils.hxx"

static const bool _lockdownPlatform = false;        // SET THIS TO TRUE FOR RELEASE.
static const size_t _recommendedPlatformMaxName = 32;
static const bool _enableMaskName = false;

inline QString calculateImageBaseName( QString fileName )
{
    // Determine the texture name.
    QFileInfo fileInfo( fileName );

    return fileInfo.baseName();
}

QString TexAddDialog::GetCurrentPlatform( void )
{
    QString currentPlatform;

    if ( QLineEdit *editBox = dynamic_cast <QLineEdit*> ( this->platformSelectWidget ) )
    {
        currentPlatform = editBox->text();
    }
    else if ( QComboBox *comboBox = dynamic_cast <QComboBox*> ( this->platformSelectWidget ) )
    {
        currentPlatform = comboBox->currentText();
    }

    return currentPlatform;
}

void TexAddDialog::releaseConvRaster( void )
{
    if ( rw::Raster *convRaster = this->convRaster )
    {
        rw::DeleteRaster( this->convRaster );

        this->convRaster = NULL;
    }
}

void TexAddDialog::loadPlatformOriginal( void )
{
    // If we have a converted raster, release it.
    this->releaseConvRaster();

    bool hasPreview = false;

    try
    {
        // Depends on what we have.
        if ( this->dialog_type == CREATE_IMGPATH )
        {
            // Set the platform of our raster.
            // If we have no platform, we have no preview.
            bool hasPlatform = false;

            QString currentPlatform = this->GetCurrentPlatform();

            if ( currentPlatform.isEmpty() == false )
            {
                std::string ansiNativeName = currentPlatform.toStdString();
            
                // Delete any preview native data.
                this->platformOrigRaster->clearNativeData();

                this->platformOrigRaster->newNativeData( ansiNativeName.c_str() );

                hasPlatform = true;
            }

            if ( hasPlatform )
            {
                // Open a stream to the image data.
                std::wstring unicodePathToImage = this->imgPath.toStdWString();

                rw::streamConstructionFileParamW_t wparam( unicodePathToImage.c_str() );

                rw::Stream *imgStream = mainWnd->rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &wparam );

                if ( imgStream )
                {
                    try
                    {
                        // Load it.
                        this->platformOrigRaster->readImage( imgStream );

                        // Success!
                        hasPreview = true;
                    }
                    catch( ... )
                    {
                        mainWnd->rwEngine->DeleteStream( imgStream );

                        throw;
                    }

                    mainWnd->rwEngine->DeleteStream( imgStream );
                }
            }
        }
        else if ( this->dialog_type == CREATE_RASTER )
        {
            // We always have a platform original.
            hasPreview = true;
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

    this->hasPlatformOriginal = hasPreview;

    // If we have a preview, update the preview widget with its content.
    if ( hasPreview )
    {
        // If we have a preview, we want to limit the properties box.
        this->propertiesWidget->setMaximumWidth( 300 );

        // Set the recommended sizes.
        rw::uint32 recWidth, recHeight;
        this->platformOrigRaster->getSize( recWidth, recHeight );

        this->previewWidget->recommendedWidth = (int)recWidth;
        this->previewWidget->recommendedHeight = (int)recHeight;

        this->updatePreviewWidget();
    }
    else
    {
        this->propertiesWidget->setMaximumWidth( 16777215 );
    }

    // Hide or show the changeable properties.
    this->platformPropsGroup->setVisible( hasPreview );
    this->propGenerateMipmaps->setVisible( hasPreview );

    // If we have no preview, then we also cannot push the data to the texture container.
    // This is why we should disable that possibility.
    this->applyButton->setDisabled( !hasPreview );

    // Hide or show the preview stuff.
    this->previewGroupWidget->setVisible( hasPreview );
}

void TexAddDialog::updatePreviewWidget( void )
{
    this->previewWidget->update();
}

void TexAddDialog::pixelPreviewWidget::paintEvent( QPaintEvent *evt )
{
    if ( this->wantsRasterUpdate )
    {
        rw::Raster *previewRaster = owner->GetDisplayRaster();

        if ( previewRaster )
        {
            try
            {
                // Put the contents of the platform original into the preview widget.
                // We want to transform the raster into a bitmap, basically.
                rw::Bitmap rasterBmp = previewRaster->getBitmap();

                // Do it.
                owner->pixelsToAdd = convertRWBitmapToQPixmap( rasterBmp );
            }
            catch( rw::RwException& except )
            {
                owner->mainWnd->txdLog->showError( QString( "failed to create preview: " ) + QString::fromStdString( except.message ) );

                // Continue normal execution.
            }
        }

        this->wantsRasterUpdate = false;
    }

    QPainter thePainter( this );

    thePainter.drawPixmap( 0, 0, this->width(), this->height(), owner->pixelsToAdd );

    QWidget::paintEvent( evt );
}

void TexAddDialog::createRasterForConfiguration( void )
{
    if ( this->hasPlatformOriginal == false )
        return;

    // This function prepares the raster that will be given to the texture dictionary.

    bool hasConfiguredRaster = false;

    try
    {
        rw::eCompressionType compressionType = rw::RWCOMPRESS_NONE;

        rw::eRasterFormat rasterFormat = rw::RASTER_DEFAULT;
        rw::ePaletteType paletteType = rw::PALETTE_NONE;

        // Now for the properties.
        if ( this->platformCompressionToggle->isChecked() )
        {
            // We are a compressed format, so determine what we actually are.
            QString selectedCompression = this->platformCompressionSelectProp->currentText();

            if ( selectedCompression == "DXT1" )
            {
                compressionType = rw::RWCOMPRESS_DXT1;
            }
            else if ( selectedCompression == "DXT2" )
            {
                compressionType = rw::RWCOMPRESS_DXT2;
            }
            else if ( selectedCompression == "DXT3" )
            {
                compressionType = rw::RWCOMPRESS_DXT3;
            }
            else if ( selectedCompression == "DXT4" )
            {
                compressionType = rw::RWCOMPRESS_DXT4;
            }
            else if ( selectedCompression == "DXT5" )
            {
                compressionType = rw::RWCOMPRESS_DXT5;
            }
            else
            {
                throw std::exception( "invalid compression type selected" );
            }

            rasterFormat = rw::RASTER_DEFAULT;
            paletteType = rw::PALETTE_NONE;
        }
        else
        {
            compressionType = rw::RWCOMPRESS_NONE;

            // Now we have a valid raster format selected in the pixel format combo box.
            // We kinda need one.
            if ( this->enablePixelFormatSelect )
            {
                QString formatName = this->platformPixelFormatSelectProp->currentText();

                std::string ansiFormatName = formatName.toStdString();

                rasterFormat = rw::FindRasterFormatByName( ansiFormatName.c_str() );

                if ( rasterFormat == rw::RASTER_DEFAULT )
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

                    paletteType = rw::PALETTE_4BIT;
                }
                else if ( paletteName == "PAL8" )
                {
                    paletteType = rw::PALETTE_8BIT;
                }
                else
                {
                    throw std::exception( "invalid palette type selected" );
                }
            }
            else
            {
                paletteType = rw::PALETTE_NONE;
            }
        }

        // Create the raster.
        try
        {
            // Clear previous image data.
            this->releaseConvRaster();

            rw::Raster *convRaster = rw::CloneRaster( this->platformOrigRaster );

            this->convRaster = convRaster;

            // If we have not created a direct platform raster through image loading, we
            // should put it into the currect platform over here.
            if ( this->dialog_type != CREATE_IMGPATH )
            {
                std::string currentPlatform = this->GetCurrentPlatform().toStdString();

                rw::ConvertRasterTo( convRaster, currentPlatform.c_str() );
            }

            // Format the raster appropriately.
            {
                if ( compressionType != rw::RWCOMPRESS_NONE )
                {
                    // Just compress it.
                    convRaster->compressCustom( compressionType );
                }
                else if ( rasterFormat != rw::RASTER_DEFAULT )
                {
                    // We want a specialized format.
                    // Go ahead.
                    if ( paletteType != rw::PALETTE_NONE )
                    {
                        // Palettize.
                        convRaster->convertToPalette( paletteType, rasterFormat );
                    }
                    else
                    {
                        // Let us convert to another format.
                        convRaster->convertToFormat( rasterFormat );
                    }
                }
            }

            // Success!
            hasConfiguredRaster = true;
        }
        catch( rw::RwException& except )
        {
            this->mainWnd->txdLog->showError( QString( "failed to create raster: " ) + QString::fromStdString( except.message ) );
        }
    }
    catch( std::exception& except )
    {
        // If we failed to push data to the output stage.
        this->mainWnd->txdLog->showError( QString( "failed to create raster: " ) + except.what() );
    }

    // If we do not need a configured raster anymore, release it.
    if ( !hasConfiguredRaster )
    {
        this->releaseConvRaster();
    }

    // Update the preview.
    this->updatePreviewWidget();
}

TexAddDialog::TexAddDialog( MainWindow *mainWnd, const dialogCreateParams& create_params, TexAddDialog::operationCallback_t cb ) : QDialog( mainWnd )
{
    this->mainWnd = mainWnd;

    this->dialog_type = create_params.type;

    this->cb = cb;

    this->setAttribute( Qt::WA_DeleteOnClose );
    this->setWindowModality( Qt::WindowModality::WindowModal );
    
    // Create a raster handle that will hold platform original data.
    this->platformOrigRaster = NULL;
    this->convRaster = NULL;

    if ( this->dialog_type == CREATE_IMGPATH )
    {
        this->platformOrigRaster = rw::CreateRaster( mainWnd->rwEngine );

        this->imgPath = create_params.img_path.imgPath;
    }
    else if ( this->dialog_type == CREATE_RASTER )
    {
        this->platformOrigRaster = rw::AcquireRaster( create_params.orig_raster.tex->GetRaster() );
    }

    this->enableRawRaster = true;
    this->enableCompressSelect = true;
    this->enablePaletteSelect = true;
    this->enablePixelFormatSelect = true;

    // Calculate an appropriate texture name.
    QString textureBaseName;
    QString textureMaskName;

    if ( this->dialog_type == CREATE_IMGPATH )
    {
        textureBaseName = calculateImageBaseName( this->imgPath );

        // screw mask name.
    }
    else if ( this->dialog_type == CREATE_RASTER )
    {
        textureBaseName = QString::fromStdString( create_params.orig_raster.tex->GetName() );
        textureMaskName = QString::fromStdString( create_params.orig_raster.tex->GetMaskName() );
    }

    if ( const QString *overwriteTexName = create_params.overwriteTexName )
    {
        textureBaseName = *overwriteTexName;
    }

    // TODO: verify that the texture name is full ANSI.

    this->setWindowTitle( create_params.actionName + " Texture..." );

    // Create our GUI interface.
    QHBoxLayout *rootHoriLayout = new QHBoxLayout();

    // Add a form to the left, a preview to the right.
    QTabWidget *formHolderWidget = new QTabWidget();

    this->propertiesWidget = formHolderWidget;

    // Add the contents of the form.
    QString curPlatformText;
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
            QLineEdit *texMaskNameEdit = new QLineEdit( textureMaskName );

            texMaskNameEdit->setMaxLength( _recommendedPlatformMaxName );

            formLayout->addRow( new QLabel( "Mask Name:" ), texMaskNameEdit );

            this->textureMaskNameEdit = texMaskNameEdit;
        }
        else
        {
            this->textureMaskNameEdit = NULL;
        }

        // If the current TXD already has a platform, we disable editing this platform and simply use it.
        const char *currentForcedPlatform = mainWnd->GetTXDPlatformString( mainWnd->currentTXD );

        QWidget *platformDisplayWidget;

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

            if ( currentForcedPlatform != NULL )
            {
                platformComboBox->setCurrentText( currentForcedPlatform );
            }

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

        this->platformSelectWidget = platformDisplayWidget;

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

            connect( compressionFormatSelect, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &TexAddDialog::OnTextureCompressionSeelct );

            groupContentFormLayout->addRow( compressionFormatToggle, compressionFormatSelect );

            this->platformCompressionSelectProp = compressionFormatSelect;

            QRadioButton *paletteFormatToggle = new QRadioButton( "palettized" );

            this->platformPaletteToggle = paletteFormatToggle;

            connect( paletteFormatToggle, &QRadioButton::toggled, this, &TexAddDialog::OnPlatformFormatTypeToggle );

            QComboBox *paletteFormatSelect = new QComboBox();

            paletteFormatSelect->addItem( "PAL4" );
            paletteFormatSelect->addItem( "PAL8" );

            connect( paletteFormatSelect, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &TexAddDialog::OnTexturePaletteTypeSelect );

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

            connect( pixelFormatSelect, (void (QComboBox::*)( const QString& ))&QComboBox::activated, this, &TexAddDialog::OnTexturePixelFormatSelect );

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

        this->cancelButton = cancelButton;

        connect( cancelButton, &QPushButton::clicked, this, &TexAddDialog::OnCloseRequest );

        bottomControlButtonsGroup->addWidget( cancelButton );

        QPushButton *addButton = new QPushButton( create_params.actionName );

        this->applyButton = addButton;

        connect( addButton, &QPushButton::clicked, this, &TexAddDialog::OnTextureAddRequest );

        bottomControlButtonsGroup->addWidget( addButton );

        generalTabRootLayout->addLayout( bottomControlButtonsGroup );

        generalTab->setLayout( generalTabRootLayout );

        formHolderWidget->addTab( generalTab, "Properties" );
    }

    rootHoriLayout->addWidget( formHolderWidget );

    this->setLayout( rootHoriLayout );

    // Cached the preview item.
    {
        QGroupBox *previewGroupBox = new QGroupBox();

        this->previewGroupWidget = previewGroupBox;

        pixelPreviewWidget *previewWidget = new pixelPreviewWidget( this );

        this->previewWidget = previewWidget;

        previewWidget->recommendedWidth = 0;
        previewWidget->recommendedHeight = 0;
        previewWidget->requiredHeight = this->sizeHint().height();

        QVBoxLayout *groupBoxLayout = new QVBoxLayout();

        groupBoxLayout->addWidget( previewWidget );

        previewGroupBox->setLayout( groupBoxLayout );

        rootHoriLayout->addWidget( previewGroupBox );

        previewGroupBox->setVisible( false );
    }

    // Do initial stuff.
    {
        if ( curPlatformText.isEmpty() == false )
        {
            this->OnPlatformSelect( curPlatformText );
        }
    }
}

TexAddDialog::~TexAddDialog( void )
{
    // Remove the raster that we created.
    // Remember that it is reference counted.
    rw::DeleteRaster( this->platformOrigRaster );

    this->releaseConvRaster();
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
    if ( checked != true )
        return;

    // Since we switched the platform format type, we have to adjust the accessability.
    // The accessability change must not swap items around on the GUI. Rather it should
    // disable items that make no sense.
    this->UpdateAccessability();

    // Since we switched the format type, the texture encoding has changed.
    // Update the preview.
    this->createRasterForConfiguration();
}

void TexAddDialog::OnTextureCompressionSeelct( const QString& newCompression )
{
    this->createRasterForConfiguration();
}

void TexAddDialog::OnTexturePaletteTypeSelect( const QString& newPaletteType )
{
    this->createRasterForConfiguration();
}

void TexAddDialog::OnTexturePixelFormatSelect( const QString& newPixelFormat )
{
    this->createRasterForConfiguration();
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

    this->enableRawRaster = enableRawRaster;
    this->enableCompressSelect = enableCompressSelect;
    this->enablePaletteSelect = enablePaletteSelect;
    this->enablePixelFormatSelect = enablePixelFormatSelect;

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

    // Reload the preview image with what the platform wants us to see.
    this->loadPlatformOriginal();

    // We want to create a raster special to the configuration.
    this->createRasterForConfiguration();
}

void TexAddDialog::OnTextureAddRequest( bool checked )
{
    // This is where we want to go.
    // Decide the format that the runtime has requested.

    rw::Raster *displayRaster = this->GetDisplayRaster();

    if ( displayRaster )
    {
        texAddOperation desc;
        desc.texName = this->textureNameEdit->text().toStdString();

        if ( this->textureMaskNameEdit )
        {
            desc.maskName = this->textureMaskNameEdit->text().toStdString();
        }

        // Maybe generate mipmaps.
        if ( this->propGenerateMipmaps->isChecked() )
        {
            displayRaster->generateMipmaps( INFINITE, rw::MIPMAPGEN_DEFAULT );
        }

        desc.raster = displayRaster;

        this->cb( desc );
    }

    // Close ourselves.
    this->close();
}

void TexAddDialog::OnCloseRequest( bool checked )
{
    // The user doesnt want to do it anymore.
    this->close();
}