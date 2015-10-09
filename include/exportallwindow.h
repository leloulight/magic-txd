#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

// We want to have a simple window which can be used by the user to export all textures of a TXD.

struct ExportAllWindow : public QDialog
{
private:
    inline static std::vector <std::string> GetAllSupportedImageFormats( rw::TexDictionary *texDict )
    {
        std::vector <std::string> formatsOut;

        rw::Interface *engineInterface = texDict->GetEngine();

        // The only formats that we question support for are native formats.
        // Currently, each native texture can export one native format.
        // Hence the only supported formats can be formats that are supported from the first texture on.
        bool isFirstRaster = true;

        for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
        {
            rw::TextureBase *texture = iter.Resolve();

            rw::Raster *texRaster = texture->GetRaster();

            if ( texRaster )
            {
                // If we are the first raster we encounter, we add all of its formats to our list.
                // Otherwise we check for support on each raster until no more support is guarranteed.
                const char *nativeFormat =
                    rw::GetNativeTextureImageFormatExtension( engineInterface, texRaster->getNativeDataTypeName() );
                
                if ( isFirstRaster )
                {
                    if ( nativeFormat )
                    {
                        formatsOut.push_back( nativeFormat );
                    }

                    isFirstRaster = false;
                }
                else
                {
                    // If we have no more formats, we quit.
                    if ( formatsOut.empty() )
                    {
                        break;
                    }

                    // Remove anything thats not part of the supported.
                    std::vector <std::string> newList;

                    if ( nativeFormat )
                    {
                        for ( const std::string& formstr : formatsOut )
                        {
                            if ( formstr == nativeFormat )
                            {
                                newList.push_back( nativeFormat );
                                break;
                            }
                        }
                    }

                    formatsOut = std::move( newList );
                }
            }
        }

        // After that come the formats that every raster supports anyway.
        rw::registered_image_formats_t availImageFormats;

        rw::GetRegisteredImageFormats( engineInterface, availImageFormats );

        for ( const rw::registered_image_format& format : availImageFormats )
        {
            const char *defaultExt = NULL;

            bool gotDefaultExt = rw::GetDefaultImagingFormatExtension( format.num_ext, format.ext_array, defaultExt );

            if ( gotDefaultExt )
            {
                formatsOut.push_back( defaultExt );
            }
        }

        // And of course, the texture chunk, that is always supported.
        formatsOut.push_back( "RWTEX" );

        return formatsOut;
    }

public:
    inline ExportAllWindow( MainWindow *mainWnd, rw::TexDictionary *texDict ) : QDialog( mainWnd )
    {
        this->mainWnd = mainWnd;
        this->texDict = texDict;

        setWindowFlags( windowFlags() & ~Qt::WindowContextHelpButtonHint );

        setWindowTitle( "Select Format..." );
        setWindowModality( Qt::WindowModal );

        this->setAttribute( Qt::WA_DeleteOnClose );

        // Basic window with a combo box and a button to accept.
        QVBoxLayout *rootLayout = new QVBoxLayout();

        rootLayout->setSizeConstraint( QLayout::SetFixedSize );

        QHBoxLayout *formatSelRow = new QHBoxLayout();

        formatSelRow->addWidget( new QLabel( "Format:" ) );

        QComboBox *formatSelBox = new QComboBox();

        // List formats that are supported by every raster in a TXD.
        {
            std::vector <std::string> formatsSupported = GetAllSupportedImageFormats( texDict );

            for ( const std::string& format : formatsSupported )
            {
                formatSelBox->addItem( QString::fromStdString( format ) );
            }
        }

        // Select the last used format, if it exists.
        {
            int foundLastUsed = formatSelBox->findText( QString::fromStdString( mainWnd->lastUsedAllExportFormat ), Qt::MatchExactly );

            if ( foundLastUsed != -1 )
            {
                formatSelBox->setCurrentIndex( foundLastUsed );
            }
        }

        this->formatSelBox = formatSelBox;

        formatSelRow->addWidget( formatSelBox );

        rootLayout->addLayout( formatSelRow );

        // Add the button row last.
        QHBoxLayout *buttonRow = new QHBoxLayout();

        QPushButton *buttonExport = new QPushButton( "Export" );

        connect( buttonExport, &QPushButton::clicked, this, &ExportAllWindow::OnRequestExport );

        buttonRow->addWidget( buttonExport );

        QPushButton *buttonCancel = new QPushButton( "Cancel" );

        connect( buttonCancel, &QPushButton::clicked, this, &ExportAllWindow::OnRequestCancel );

        buttonRow->addWidget( buttonCancel );

        rootLayout->addLayout( buttonRow );

        this->setLayout( rootLayout );

        this->setMinimumWidth( 250 );
    }

public slots:
    void OnRequestExport( bool checked )
    {
        bool shouldClose = false;

        rw::TexDictionary *texDict = this->texDict;

        rw::Interface *engineInterface = texDict->GetEngine();

        // Get the format to export as.
        QString formatTarget = this->formatSelBox->currentText();

        if ( formatTarget.isEmpty() == false )
        {
            std::string ansiFormatTarget = formatTarget.toStdString();

            // We need a directory to export to, so ask the user.
            QString folderExportTarget = QFileDialog::getExistingDirectory(
                this, "Select Export Target...",
                QString::fromStdWString( this->mainWnd->lastAllExportTarget )
            );

            if ( folderExportTarget.isEmpty() == false )
            {
                std::wstring wFolderExportTarget = folderExportTarget.toStdWString();

                // Remember this path.
                this->mainWnd->lastAllExportTarget = wFolderExportTarget;

                wFolderExportTarget += L'/';

                // Attempt to get a translator handle into that directory.
                CFileTranslator *dstTranslator = mainWnd->fileSystem->CreateTranslator( wFolderExportTarget.c_str() );

                if ( dstTranslator )
                {
                    try
                    {
                        // If we successfully did any export, we can close.
                        bool hasExportedAnything = false;

                        for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
                        {
                            rw::TextureBase *texture = iter.Resolve();

                            // We can only serialize if we have a raster.
                            rw::Raster *texRaster = texture->GetRaster();

                            if ( texRaster )
                            {
                                // Create a path to put the image at.
                                std::string imgExportPath = texture->GetName() + '.' + formatTarget.toLower().toStdString();

                                CFile *targetStream = dstTranslator->Open( imgExportPath.c_str(), "wb" );

                                if ( targetStream )
                                {
                                    try
                                    {
                                        rw::Stream *rwStream = RwStreamCreateTranslated( engineInterface, targetStream );

                                        if ( rwStream )
                                        {
                                            try
                                            {
                                                // Now attempt the write.
                                                try
                                                {
                                                    if ( stricmp( ansiFormatTarget.c_str(), "RWTEX" ) == 0 )
                                                    {
                                                        engineInterface->Serialize( texture, rwStream );
                                                    }
                                                    else
                                                    {
                                                        texRaster->writeImage( rwStream, ansiFormatTarget.c_str() );
                                                    }

                                                    hasExportedAnything = true;
                                                }
                                                catch( rw::RwException& except )
                                                {
                                                    // Nope.
                                                    this->mainWnd->txdLog->addLogMessage(
                                                        QString::fromStdString(
                                                            std::string( "failed to export texture '" ) + texture->GetName() + "': " + except.message
                                                        ), LOGMSG_WARNING
                                                    );
                                                }
                                            }
                                            catch( ... )
                                            {
                                                engineInterface->DeleteStream( rwStream );

                                                throw;
                                            }

                                            engineInterface->DeleteStream( rwStream );
                                        }
                                        else
                                        {
                                            this->mainWnd->txdLog->addLogMessage(
                                                QString::fromStdString(
                                                    std::string( "failed to create RW translation stream for texture " ) + texture->GetName()
                                                ), LOGMSG_WARNING
                                            );
                                        }
                                    }
                                    catch( ... )
                                    {
                                        delete targetStream;

                                        throw;
                                    }

                                    delete targetStream;
                                }
                                else
                                {
                                    this->mainWnd->txdLog->addLogMessage(
                                        QString::fromStdString(
                                            std::string( "failed to create export stream for texture " ) + texture->GetName()
                                        ), LOGMSG_WARNING
                                    );
                                }
                            }
                        }

                        if ( hasExportedAnything )
                        {
                            // Remember the format that we used.
                            this->mainWnd->lastUsedAllExportFormat = ansiFormatTarget;

                            // We can close now.
                            shouldClose = true;
                        }
                    }
                    catch( ... )
                    {
                        delete dstTranslator;

                        throw;
                    }

                    // Close the directory handle again.
                    delete dstTranslator;
                }
                else
                {
                    this->mainWnd->txdLog->showError(
                        QString( "failed to get a handle to target directory ('" ) + folderExportTarget + QString( "')" )
                    );
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
    MainWindow *mainWnd;
    rw::TexDictionary *texDict;

    QComboBox *formatSelBox;
};