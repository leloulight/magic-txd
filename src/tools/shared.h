#pragma once

struct MessageReceiver abstract
{
    virtual void OnMessage( const std::string& msg ) = 0;
    virtual void OnMessage( const std::wstring& msg ) = 0;

    virtual CFile* WrapStreamCodec( CFile *compressed ) = 0;
};

// Shared utilities for human-friendly RenderWare operations.
namespace rwkind
{
    enum eTargetPlatform
    {
        PLATFORM_UNKNOWN,
        PLATFORM_PC,
        PLATFORM_PS2,
        PLATFORM_XBOX,
        PLATFORM_DXT_MOBILE,
        PLATFORM_PVR,
        PLATFORM_ATC,
        PLATFORM_UNC_MOBILE
    };

    enum eTargetGame
    {
        GAME_GTA3,
        GAME_GTAVC,
        GAME_GTASA,
        GAME_MANHUNT,
        GAME_BULLY
    };

    static inline eTargetPlatform GetRasterPlatform( rw::Raster *texRaster )
    {
        eTargetPlatform thePlatform = PLATFORM_UNKNOWN;

        if ( texRaster->hasNativeDataOfType( "Direct3D8" ) || texRaster->hasNativeDataOfType( "Direct3D9" ) )
        {
            thePlatform = PLATFORM_PC;
        }
        else if ( texRaster->hasNativeDataOfType( "XBOX" ) )
        {
            thePlatform = PLATFORM_XBOX;
        }
        else if ( texRaster->hasNativeDataOfType( "PlayStation2" ) )
        {
            thePlatform = PLATFORM_PS2;
        }
        else if ( texRaster->hasNativeDataOfType( "s3tc_mobile" ) )
        {
            thePlatform = PLATFORM_DXT_MOBILE;
        }
        else if ( texRaster->hasNativeDataOfType( "PowerVR" ) )
        {
            thePlatform = PLATFORM_PVR;
        }
        else if ( texRaster->hasNativeDataOfType( "AMDCompress" ) )
        {
            thePlatform = PLATFORM_ATC;
        }
        else if ( texRaster->hasNativeDataOfType( "uncompressed_mobile" ) )
        {
            thePlatform = PLATFORM_UNC_MOBILE;
        }

        return thePlatform;
    }

    static inline double GetPlatformQualityGrade( eTargetPlatform platform )
    {
        double quality = 0.0;

        if ( platform == PLATFORM_PC )
        {
            quality = 1.0;
        }
        else if ( platform == PLATFORM_XBOX )
        {
            quality = 1.0;
        }
        else if ( platform == PLATFORM_PS2 )
        {
            quality = 1.0;
        }
        else if ( platform == PLATFORM_DXT_MOBILE )
        {
            quality = 0.7;
        }
        else if ( platform == PLATFORM_PVR )
        {
            quality = 0.4;
        }
        else if ( platform == PLATFORM_ATC )
        {
            quality = 0.8;
        }
        else if ( platform == PLATFORM_UNC_MOBILE )
        {
            quality = 0.9;
        }

        return quality;
    }

    static inline bool ShouldRasterConvertBeforehand( rw::Raster *texRaster, eTargetPlatform targetPlatform )
    {
        bool shouldBeforehand = false;

        if ( targetPlatform != PLATFORM_UNKNOWN )
        {
            eTargetPlatform rasterPlatform = GetRasterPlatform( texRaster );

            if ( rasterPlatform != PLATFORM_UNKNOWN )
            {
                if ( targetPlatform != rasterPlatform )
                {
                    // Decide based on the raster and target platform.
                    // Basically, we want to improve the quality and the conversion speed.
                    // We want to convert beforehand if we convert from a lower quality texture to a higher quality.
                    double sourceQuality = GetPlatformQualityGrade( rasterPlatform );
                    double targetQuality = GetPlatformQualityGrade( targetPlatform );

                    if ( sourceQuality == targetQuality )
                    {
                        // If the quality of the platforms does not change, we do not care.
                        shouldBeforehand = false;
                    }
                    else if ( sourceQuality < targetQuality )
                    {
                        // If the quality of the current raster is worse than the target, we should.
                        shouldBeforehand = true;
                    }
                    else if ( sourceQuality > targetQuality )
                    {
                        // If the quality of the current raster is better than the target, we should not.
                        shouldBeforehand = false;
                    }
                }
            }
        }

        return shouldBeforehand;
    }

    static inline const char* GetTargetNativeFormatName( eTargetPlatform targetPlatform, eTargetGame targetGame )
    {
        if ( targetPlatform == PLATFORM_PS2 )
        {
            return "PlayStation2";
        }
        else if ( targetPlatform == PLATFORM_XBOX )
        {
            return "XBOX";
        }
        else if ( targetPlatform == PLATFORM_PC )
        {
            // Depends on the game.
            if (targetGame == GAME_GTASA)
            {
                return "Direct3D9";
            }
            
            return "Direct3D8";
        }
        else if ( targetPlatform == PLATFORM_DXT_MOBILE )
        {
            return "s3tc_mobile";
        }
        else if ( targetPlatform == PLATFORM_PVR )
        {
            return "PowerVR";
        }
        else if ( targetPlatform == PLATFORM_ATC )
        {
            return "AMDCompress";
        }
        else if ( targetPlatform == PLATFORM_UNC_MOBILE )
        {
            return "uncompressed_mobile";
        }
        
        return NULL;
    }

    static inline bool ConvertRasterToPlatform( rw::Raster *texRaster, eTargetPlatform targetPlatform, eTargetGame targetGame )
    {
        bool hasConversionSucceeded = false;

        const char *nativeName = GetTargetNativeFormatName( targetPlatform, targetGame );

        if ( nativeName )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, nativeName );
        }
        else
        {
            assert( 0 );
        }

        return hasConversionSucceeded;
    }
};