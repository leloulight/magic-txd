#pragma once

#include <QLabel>
#include <QVBoxLayout>

class TexInfoWidget : public QWidget
{
public:
	TexInfoWidget(QListWidgetItem *listItem, rw::TextureBase *texItem) : QWidget()
    {
        QLabel *texName = new QLabel(QString(), this);
        texName->setFixedHeight(23);
        texName->setObjectName("texName");
        QLabel *texInfo = new QLabel(QString(), this);
        texInfo->setObjectName("texInfo");
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(5, 4, 0, 5);
        layout->addWidget(texName);
        layout->addWidget(texInfo);

        this->texNameLabel = texName;
        this->texInfoLabel = texInfo;
        this->rwTextureHandle = texItem;

        this->listItem = listItem;

        this->updateInfo();
    }

    inline rw::TextureBase* GetTextureHandle( void )
    {
        return this->rwTextureHandle;
    }

    inline void updateInfo( void )
    {
        // Construct some information about our texture item.
        QString textureInfo;

        if ( rw::Raster *rasterInfo = this->rwTextureHandle->GetRaster() )
        {
            // First part is the texture size.
            rw::uint32 width, height;
            rasterInfo->getSize( width, height );

            textureInfo += QString::number( width ) + QString( "x" ) + QString::number( height );

            // Then is the platform format info.
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

                textureInfo += " " + QString( (const char*)platformTexInfoBuf );
            }

            // After that how many mipmap levels we have.
            rw::uint32 mipCount = rasterInfo->getMipmapCount();

            textureInfo += " " + QString::number( mipCount );

            if ( mipCount == 1 )
            {
                textureInfo += " level";
            }
            else
            {
                textureInfo += " levels";
            }
        }

        this->texNameLabel->setText( tr( this->rwTextureHandle->GetName().c_str() ) );
        this->texInfoLabel->setText( textureInfo );
    }

    inline void remove( void )
    {
        delete this->listItem;
    }

private:
    QLabel *texNameLabel;
    QLabel *texInfoLabel;

    QListWidgetItem *listItem;

    rw::TextureBase *rwTextureHandle;
};