#ifndef _TXDGEN_MODULE_
#define _TXDGEN_MODULE_

#include "shared.h"

class TxdGenModule : public MessageReceiver
{
public:
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
    
    inline TxdGenModule( rw::Interface *rwEngine )
    {
        this->rwEngine = rwEngine;
        this->_warningMan.module = this;
    }

    struct run_config
    {
        // By default, we create San Andreas files.
        rw::KnownVersions::eGameVersion c_gameVersion = rw::KnownVersions::SA;

        std::wstring c_outputRoot = L"txdgen_out/";
        std::wstring c_gameRoot = L"txdgen/";

        eTargetPlatform c_targetPlatform = PLATFORM_PC;

        bool c_clearMipmaps = false;

        bool c_generateMipmaps = false;

        rw::eMipmapGenerationMode c_mipGenMode = rw::MIPMAPGEN_DEFAULT;

        rw::uint32 c_mipGenMaxLevel = 0;

        bool c_improveFiltering = true;

        bool compressTextures = false;

        rw::ePaletteRuntimeType c_palRuntimeType = rw::PALRUNTIME_PNGQUANT;

        rw::eDXTCompressionMethod c_dxtRuntimeType = rw::DXTRUNTIME_SQUISH;

        bool c_reconstructIMGArchives = true;

        bool c_fixIncompatibleRasters = true;
        bool c_dxtPackedDecompression = false;

        bool c_imgArchivesCompressed = false;

        bool c_ignoreSerializationRegions = true;

        float c_compressionQuality = 1.0f;

        bool c_outputDebug = false;

        int c_warningLevel = 3;

        bool c_ignoreSecureWarnings = false;
    };

    run_config ParseConfig( CFileTranslator *root, const filePath& cfgPath ) const;

    bool ApplicationMain( const run_config& cfg );

    bool ProcessTXDArchive(
        CFileTranslator *srcRoot, CFile *srcStream, CFile *targetStream, eTargetPlatform targetPlatform,
        bool clearMipmaps,
        bool generateMipmaps, rw::eMipmapGenerationMode mipGenMode, rw::uint32 mipGenMaxLevel,
        bool improveFiltering,
        bool doCompress, float compressionQuality,
        bool outputDebug, CFileTranslator *debugRoot,
        rw::KnownVersions::eGameVersion gameVersion,
        std::string& errMsg
    ) const;

    rw::Interface* GetEngine( void ) const
    {
        return this->rwEngine;
    }

    struct RwWarningBuffer : public rw::WarningManagerInterface
    {
        TxdGenModule *module;
        std::string buffer;

        void Purge( void )
        {
            // Output the content to the stream.
            if ( !buffer.empty() )
            {
                module->OnMessage( "- Warnings:\n" );

                buffer += "\n";

                module->OnMessage( buffer );

                buffer.clear();
            }
        }

        virtual void OnWarning( std::string&& message ) override
        {
            if ( !buffer.empty() )
            {
                buffer += '\n';
            }

            buffer += message;
        }
    };

    RwWarningBuffer _warningMan;

private:
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

    static inline void ConvertRasterToPlatform( rw::TextureBase *theTexture, rw::Raster *texRaster, eTargetPlatform targetPlatform, rw::KnownVersions::eGameVersion gameVersion )
    {
        bool hasConversionSucceeded = false;

        if ( targetPlatform == PLATFORM_PS2 )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "PlayStation2" );
        }
        else if ( targetPlatform == PLATFORM_XBOX )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "XBOX" );
        }
        else if ( targetPlatform == PLATFORM_PC )
        {
            // Depends on the game.
            if (gameVersion == rw::KnownVersions::SA)
            {
                hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "Direct3D9" );
            }
            else
            {
                hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "Direct3D8" );
            }
        }
        else if ( targetPlatform == PLATFORM_DXT_MOBILE )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "s3tc_mobile" );
        }
        else if ( targetPlatform == PLATFORM_PVR )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "PowerVR" );
        }
        else if ( targetPlatform == PLATFORM_ATC )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "AMDCompress" );
        }
        else if ( targetPlatform == PLATFORM_UNC_MOBILE )
        {
            hasConversionSucceeded = rw::ConvertRasterTo( texRaster, "uncompressed_mobile" );
        }
        else
        {
            assert( 0 );
        }

        if ( hasConversionSucceeded == false )
        {
            theTexture->GetEngine()->PushWarning( "TxdGen: failed to convert texture " + theTexture->GetName() );
        }
    }

    rw::Interface *rwEngine;
};

#endif //_TXDGEN_MODULE_