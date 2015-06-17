#pragma once

#include <QLabel>
#include <QVBoxLayout>

class TexInfoWidget : public QWidget
{
public:
	TexInfoWidget(rw::TextureBase *texItem) : QWidget()
    {
        QLabel *texName = new QLabel(texItem->GetName().c_str(), this);
        texName->setFixedHeight(23);
        texName->setObjectName("texName");

        // Construct some information about our texture item.
        QString textureInfo;

        if ( rw::Raster *rasterInfo = texItem->GetRaster() )
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

        QLabel *texInfo = new QLabel(textureInfo, this);
        texInfo->setObjectName("texInfo");
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(5, 4, 0, 5);
        layout->addWidget(texName);
        layout->addWidget(texInfo);

        this->rwTextureHandle = texItem;
    }

    rw::TextureBase* GetTextureHandle( void )
    {
        return this->rwTextureHandle;
    }

private:
    rw::TextureBase *rwTextureHandle;
};