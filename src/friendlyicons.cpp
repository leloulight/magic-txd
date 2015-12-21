#include "mainwindow.h"

void MainWindow::showFriendlyIcons( void )
{
    // We show the friendly icons if the user has selected a texture in the dialog.

    this->areFriendlyIconsVisible = true;

    this->updateFriendlyIcons();

    this->updateFriendlyVisibilityState();
}

void MainWindow::hideFriendlyIcons( void )
{
    // We hide the friendly icons if the user has not selected a texture.

    this->areFriendlyIconsVisible = false;

    this->updateFriendlyVisibilityState();
}

void MainWindow::updateFriendlyVisibilityState( void )
{
    bool isVisible = ( this->areFriendlyIconsVisible && this->shouldShowFriendlyIcons );

    this->friendlyIconGame->setVisible( isVisible );
    this->friendlyIconSeparator->setVisible( isVisible );
    this->friendlyIconPlatform->setVisible( isVisible );
}

enum eKnownGameType
{
    GAME_GTA3,
    GAME_GTAVC,
    GAME_GTASA,
    GAME_MANHUNT,
    GAME_BULLY
};

enum eKnownPlatformType
{
    PLATFORM_PC,
    PLATFORM_XBOX,
    PLATFORM_PS2,
    PLATFORM_MOBILE
};

void MainWindow::updateFriendlyIcons( void )
{
    // Decide, based on the currently selected texture, what icons to show.

    TexInfoWidget *curSelTex = this->currentSelectedTexture;

    if ( !curSelTex )
        return; // should not happen.

    rw::TextureBase *texHandle = curSelTex->GetTextureHandle();

    if ( !texHandle )
        return;

    rw::Raster *texRaster = texHandle->GetRaster();

    if ( !texRaster )
        return;

    // The idea is that games were released with distinctive RenderWare versions and raster configurations.
    // We should decide very smartly what icons we want to show, out of a limited set.

    bool hasKnownConfiguration = false;
    eKnownGameType knownGame;
    eKnownPlatformType knownPlatform;

    try
    {
        rw::LibraryVersion rasterVersion = texRaster->GetEngineVersion();

        if ( rasterVersion.rwLibMajor == 3 )
        {
            if ( texRaster->hasNativeDataOfType( "PlayStation2" ) )
            {
                knownPlatform = PLATFORM_PS2;

                if ( rasterVersion.rwLibMinor <= 2 )
                {
                    knownGame = GAME_GTA3;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 3 )
                {
                    knownGame = GAME_GTAVC;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 4 )
                {
                    knownGame = GAME_MANHUNT;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 6 )
                {
                    knownGame = GAME_GTASA;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor == 7 )
                {
                    knownGame = GAME_BULLY;

                    hasKnownConfiguration = true;
                }
            }
            else if ( texRaster->hasNativeDataOfType( "Direct3D8" ) )
            {
                knownPlatform = PLATFORM_PC;

                if ( rasterVersion.rwLibMinor <= 3 )
                {
                    knownGame = GAME_GTA3;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 4 )
                {
                    knownGame = GAME_GTAVC;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 6 )
                {
                    knownGame = GAME_MANHUNT;

                    hasKnownConfiguration = true;
                }
            }
            else if ( texRaster->hasNativeDataOfType( "Direct3D9" ) )
            {
                if ( rasterVersion.rwLibMinor >= 5 )
                {
                    knownGame = GAME_GTASA;
                    knownPlatform = PLATFORM_PC;

                    hasKnownConfiguration = true;
                }
            }
            else if ( texRaster->hasNativeDataOfType( "XBOX" ) )
            {
                knownPlatform = PLATFORM_XBOX;

                if ( rasterVersion.rwLibMinor == 5 )
                {
                    knownGame = GAME_GTA3;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor >= 6 )
                {
                    knownGame = GAME_GTASA;

                    hasKnownConfiguration = true;
                }
            }
            else if ( texRaster->hasNativeDataOfType( "AMDCompress" ) ||
                      texRaster->hasNativeDataOfType( "PowerVR" ) ||
                      texRaster->hasNativeDataOfType( "uncompressed_mobile" ) ||
                      texRaster->hasNativeDataOfType( "s3tc_mobile" ) )
            {
                if ( rasterVersion.rwLibMinor < 5 )
                {
                    knownPlatform = PLATFORM_MOBILE;
                    knownGame = GAME_GTA3;

                    hasKnownConfiguration = true;
                }
            }
        }
    }
    catch( rw::RwException& )
    {
        // Ignore exceptions.
    }

    if ( hasKnownConfiguration )
    {
        if (this->showGameIcon) {
            QString iconPath = makeAppPath("resources\\icons\\");

            QString gameIconPath(iconPath);

            if (knownGame == GAME_GTA3)
            {
                gameIconPath += "gta3.png";
            }
            else if (knownGame == GAME_GTAVC)
            {
                gameIconPath += "vc.png";
            }
            else if (knownGame == GAME_GTASA)
            {
                gameIconPath += "sa.png";
            }
            else if (knownGame == GAME_MANHUNT)
            {
                gameIconPath += "mh.png";
            }
            else if (knownGame == GAME_BULLY)
            {
                gameIconPath += "bully.png";
            }
            else
            {
                gameIconPath += "nogame.png";
            }

            this->friendlyIconGame->setPixmap(QPixmap(gameIconPath));

            QString platIconPath(std::move(iconPath));

            if (knownPlatform == PLATFORM_PC)
            {
                platIconPath += "pc.png";
            }
            else if (knownPlatform == PLATFORM_PS2)
            {
                platIconPath += "ps2.png";
            }
            else if (knownPlatform == PLATFORM_XBOX)
            {
                platIconPath += "xbox.png";
            }
            else if (knownPlatform == PLATFORM_MOBILE)
            {
                platIconPath += "mobile.png";
            }
            else
            {
                platIconPath += "noplat.png";
            }

            this->friendlyIconPlatform->setPixmap(QPixmap(platIconPath));
        }
        else {
            QString gameName;

            if (knownGame == GAME_GTA3)
            {
                gameName = "GTA 3";
            }
            else if (knownGame == GAME_GTAVC)
            {
                gameName = "Vice City";
            }
            else if (knownGame == GAME_GTASA)
            {
                gameName = "San Andreas";
            }
            else if (knownGame == GAME_MANHUNT)
            {
                gameName = "Manhunt";
            }
            else if (knownGame == GAME_BULLY)
            {
                gameName = "Bully";
            }
            else
            {
                gameName = "";
            }

            this->friendlyIconGame->setText(gameName);

            QString platName;

            if (knownPlatform == PLATFORM_PC)
            {
                platName = "PC";
            }
            else if (knownPlatform == PLATFORM_PS2)
            {
                platName = "PS2";
            }
            else if (knownPlatform == PLATFORM_XBOX)
            {
                platName = "XBOX";
            }
            else if (knownPlatform == PLATFORM_MOBILE)
            {
                platName = "Mobile";
            }
            else
            {
                platName = "";
            }

            this->friendlyIconPlatform->setText(platName);
        }
    }

    this->shouldShowFriendlyIcons = hasKnownConfiguration;

    this->updateFriendlyVisibilityState();
}