#pragma once

#include <QLabel>
#include <QVBoxLayout>

class TexInfoWidget : public QWidget
{
public:
	TexInfoWidget(rw::TextureBase *texItem) : QWidget()
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

            // Then is the platform info.
            const char *typeName = rasterInfo->getNativeDataTypeName();

            textureInfo += " " + QString( typeName );

            // After that how many mipmap levels we have.
            textureInfo += " " + QString::number( rasterInfo->getMipmapCount() ) + " levels";
        }

        this->texNameLabel->setText( tr( this->rwTextureHandle->GetName().c_str() ) );
        this->texInfoLabel->setText( textureInfo );
    }

private:
    QLabel *texNameLabel;
    QLabel *texInfoLabel;

    rw::TextureBase *rwTextureHandle;
};