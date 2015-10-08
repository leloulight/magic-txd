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
    GAME_MANHUNT
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
                else if ( rasterVersion.rwLibMinor <= 4 )
                {
                    knownGame = GAME_GTAVC;

                    hasKnownConfiguration = true;
                }
                else if ( rasterVersion.rwLibMinor <= 6 )
                {
                    knownGame = GAME_GTASA;

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
                else if ( rasterVersion.rwLibMajor <= 6 )
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
                else if ( rasterVersion.rwLibMajor >= 6 )
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
        QString iconPath = makeAppPath( "resources\\icons\\" );

        QString gameIconPath( iconPath );

        if ( knownGame == GAME_GTA3 )
        {
            gameIconPath += "G_III.png";
        }
        else if ( knownGame == GAME_GTAVC )
        {
            gameIconPath += "G_VC.png";
        }
        else if ( knownGame == GAME_GTASA )
        {
            gameIconPath += "G_SA.png";
        }
        else if ( knownGame == GAME_MANHUNT )
        {
            gameIconPath += "G_MH1.png";
        }
        else
        {
            gameIconPath += "NOGAME.png";
        }

        this->friendlyIconGame->setPixmap( QPixmap( gameIconPath ).scaled( 40, 40 ) );

        QString platIconPath( std::move( iconPath ) );

        if ( knownPlatform == PLATFORM_PC )
        {
            platIconPath += "PLAT_PC.png";
        }
        else if ( knownPlatform == PLATFORM_PS2 )
        {
            platIconPath += "PLAT_PS.png";
        }
        else if ( knownPlatform == PLATFORM_XBOX )
        {
            platIconPath += "PLAT_XBOX.png";
        }
        else if ( knownPlatform == PLATFORM_MOBILE )
        {
            platIconPath += "PLAT_MB.png";
        }
        else
        {
            platIconPath += "NOPLAT.png";
        }

        this->friendlyIconPlatform->setPixmap( QPixmap( platIconPath ).scaled( 40, 40 ) );
    }

    this->shouldShowFriendlyIcons = hasKnownConfiguration;

    this->updateFriendlyVisibilityState();
}