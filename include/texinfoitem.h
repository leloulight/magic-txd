#pragma once

#include <QLabel>
#include <QVBoxLayout>
#include "languages.h"

class TexInfoWidget : public QWidget, public magicTextLocalizationItem
{
public:
	TexInfoWidget(MainWindow *mainWnd, QListWidgetItem *listItem, rw::TextureBase *texItem) : QWidget()
    {
        this->mainWnd = mainWnd;

        QLabel *texName = new QLabel(QString());
        texName->setFixedHeight(23);
        texName->setObjectName("label19px");
        QLabel *texInfo = new QLabel(QString());
        texInfo->setObjectName("texInfo");
        QVBoxLayout *layout = new QVBoxLayout();
        layout->setContentsMargins(5, 4, 0, 5);
        layout->addWidget(texName);
        layout->addWidget(texInfo);

        this->texNameLabel = texName;
        this->texInfoLabel = texInfo;
        this->rwTextureHandle = texItem;
        this->listItem = listItem;

        this->updateInfo();

        this->setLayout(layout);

        RegisterTextLocalizationItem( mainWnd, this );
    }

    ~TexInfoWidget( void )
    {
        UnregisterTextLocalizationItem( this->mainWnd, this );
    }

    inline void SetTextureHandle( rw::TextureBase *texHandle )
    {
        this->rwTextureHandle = texHandle;

        this->updateInfo();
    }

    inline rw::TextureBase* GetTextureHandle( void )
    {
        return this->rwTextureHandle;
    }

    inline static QString getRasterFormatString( const rw::Raster *rasterInfo )
    {
        char platformTexInfoBuf[ 256 + 1 ];

        size_t localPlatformTexInfoLength = 0;
        {
            // Note that the format string is only put into our buffer as much space as we have in it.
            // If the format string is longer, then that does not count as an error.
            // lengthOut will always return the format string length of the real thing, so be careful here.
            const size_t availableStringCharSpace = ( sizeof( platformTexInfoBuf ) - 1 );

            size_t platformTexInfoLength = 0;

            rasterInfo->getFormatString( platformTexInfoBuf, availableStringCharSpace, platformTexInfoLength );

            // Get the trimmed string length.
            localPlatformTexInfoLength = std::min( platformTexInfoLength, availableStringCharSpace );
        }

        // Zero terminate it.
        platformTexInfoBuf[ localPlatformTexInfoLength ] = '\0';

        return QString( (const char*)platformTexInfoBuf );
    }

    inline static QString getDefaultRasterInfoString( MainWindow *mainWnd, const rw::Raster *rasterInfo )
    {
        QString textureInfo;

        // First part is the texture size.
        rw::uint32 width, height;
        rasterInfo->getSize( width, height );

        textureInfo += QString::number( width ) + QString( "x" ) + QString::number( height );

        // Then is the platform format info.
        textureInfo += " " + getRasterFormatString( rasterInfo );

        // After that how many mipmap levels we have.
        rw::uint32 mipCount = rasterInfo->getMipmapCount();

        textureInfo += " " + QString::number( mipCount ) + " ";

        if ( mipCount == 1 )
        {
            textureInfo += getLanguageItemByKey( mainWnd, "Main.TexInfo.Level" );
        }
        else
        {
            QString levelsKeyStr = "Main.TexInfo.Lvl" + QString::number(mipCount);
            bool found;
            QString levelsStr = getLanguageItemByKey( mainWnd, levelsKeyStr, &found );
            if(found)
                textureInfo += levelsStr;
            else
                textureInfo += getLanguageItemByKey( mainWnd, "Main.TexInfo.Levels" );
        }

        return textureInfo;
    }

    inline void updateInfo( void )
    {
        MainWindow *mainWnd = this->mainWnd;

        // Construct some information about our texture item.
        if ( rw::TextureBase *texHandle = this->rwTextureHandle )
        {
            QString textureInfo;

            if ( rw::Raster *rasterInfo = this->rwTextureHandle->GetRaster() )
            {
                textureInfo = getDefaultRasterInfoString( mainWnd, rasterInfo );
            }

            this->texNameLabel->setText( tr( this->rwTextureHandle->GetName().c_str() ) );
            this->texInfoLabel->setText( textureInfo );
        }
        else
        {
            this->texNameLabel->setText( getLanguageItemByKey( mainWnd, "Main.TexInfo.NoTex" ) );
            this->texInfoLabel->setText( getLanguageItemByKey( mainWnd, "Main.TexInfo.Invalid" ) );
        }
    }

    void updateContent( MainWindow *mainWnd )
    {
        this->updateInfo();
    }

    inline void remove( void )
    {
        delete this->listItem;
    }

private:
    MainWindow *mainWnd;

    QLabel *texNameLabel;
    QLabel *texInfoLabel;

    rw::TextureBase *rwTextureHandle;

public:
    QListWidgetItem *listItem;
};