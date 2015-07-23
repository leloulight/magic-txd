#include "mainwindow.h"

TexAddDialog::TexAddDialog( MainWindow *mainWnd, QPixmap pixels )
{
    this->setWindowTitle( "Add Texture..." );

    this->pixelsToAdd = pixels;

    // Create our GUI interface.
    QHBoxLayout *rootHoriLayout = new QHBoxLayout();

    // Add a form to the left, a preview to the right.
    QTabWidget *formHolderWidget = new QTabWidget();

    formHolderWidget->setMaximumWidth( 300 );

    // Add the contents of the form.
    {
        QWidget *generalTab = new QWidget();

        QVBoxLayout *generalTabRootLayout = new QVBoxLayout();

        generalTabRootLayout->setContentsMargins( 5, 5, 5, 5 );

        QFormLayout *formLayout = new QFormLayout();

        QLineEdit *texNameEdit = new QLineEdit();

        formLayout->addRow( new QLabel( "Texture Name:" ), texNameEdit );

#if 0
        QLineEdit *texMaskNameEdit = new QLineEdit();

        formLayout->addRow( new QLabel( "Mask Name:" ), texMaskNameEdit );
#endif

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
            
        formLayout->addRow( new QLabel( "Platform:" ), platformComboBox );

        // We want to leave some space after the platform stuff.
        formLayout->setContentsMargins( 0, 0, 0, 25 );

        generalTabRootLayout->addLayout( formLayout );

        // Add a new section for platform properties.
        QGroupBox *platformPropertiesGroupBox = new QGroupBox();

        platformPropertiesGroupBox->setTitle( "Platform Properties" );

        platformPropertiesGroupBox->setContentsMargins( 0, 0, 0, 15 );

        // Set the contents of the group box.
        {
            QFormLayout *groupContentFormLayout = new QFormLayout();

            QRadioButton *rawRasterToggle = new QRadioButton( "Raw Raster" );

            rawRasterToggle->setChecked( true );

            groupContentFormLayout->addRow( rawRasterToggle );

            QRadioButton *compressionFormatToggle = new QRadioButton( "compressed" );

            QComboBox *compressionFormatSelect = new QComboBox();

            groupContentFormLayout->addRow( compressionFormatToggle, compressionFormatSelect );

            QRadioButton *paletteFormatToggle = new QRadioButton( "palettized" );

            QComboBox *paletteFormatSelect = new QComboBox();

            paletteFormatSelect->addItem( "PAL4" );
            paletteFormatSelect->addItem( "PAL8" );

            groupContentFormLayout->addRow( paletteFormatToggle, paletteFormatSelect );

            QComboBox *pixelFormatSelect = new QComboBox();

            pixelFormatSelect->addItem( "RASTER_1555" );
            pixelFormatSelect->addItem( "RASTER_565" );
            pixelFormatSelect->addItem( "RASTER_4444" );
            pixelFormatSelect->addItem( "RASTER_LUM8" );
            pixelFormatSelect->addItem( "RASTER_8888" );
            pixelFormatSelect->addItem( "RASTER_888" );
            pixelFormatSelect->addItem( "RASTER_555" );

            groupContentFormLayout->addRow( new QLabel( "Pixel Format:" ), pixelFormatSelect );

            platformPropertiesGroupBox->setLayout( groupContentFormLayout );
        }

        generalTabRootLayout->addWidget( platformPropertiesGroupBox );

        // Add some basic properties that exist no matter what platform.
        QCheckBox *generateMipmapsToggle = new QCheckBox( "Generate mipmaps" );

        generalTabRootLayout->addWidget( generateMipmapsToggle );

        generalTabRootLayout->addStretch();

        // Add control buttons at the bottom.
        QHBoxLayout *bottomControlButtonsGroup = new QHBoxLayout();

        bottomControlButtonsGroup->setContentsMargins( 40, 0, 10, 10 );

        QPushButton *cancelButton = new QPushButton( "Cancel" );

        bottomControlButtonsGroup->addWidget( cancelButton );

        QPushButton *addButton = new QPushButton( "Add" );

        bottomControlButtonsGroup->addWidget( addButton );

        generalTabRootLayout->addLayout( bottomControlButtonsGroup );

        generalTab->setLayout( generalTabRootLayout );

        formHolderWidget->addTab( generalTab, "Properties" );
    }

    rootHoriLayout->addWidget( formHolderWidget );

    QGroupBox *previewGroupBox = new QGroupBox();

    pixelPreviewWidget *previewWidget = new pixelPreviewWidget( this );

    QVBoxLayout *groupBoxLayout = new QVBoxLayout();

    groupBoxLayout->addWidget( previewWidget );

    previewGroupBox->setLayout( groupBoxLayout );

    rootHoriLayout->addWidget( previewGroupBox );

    this->setLayout( rootHoriLayout );

    this->resize( 520, 320 );
}