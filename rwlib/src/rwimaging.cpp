#include "StdInc.h"

#include "rwimaging.hxx"

#include <PluginHelpers.h>

#include <map>

// Define RWLIB_INCLUDE_IMAGING in rwconf.h to enable this component.

namespace rw
{

#ifdef RWLIB_INCLUDE_IMAGING
// This component is used to convert Streams into a Bitmap and the other way round.
// The user should be able to decide in rwconf.h what modules he wants to ship with, or to trim the imaging altogether.

struct rwImagingEnv
{
    inline void Initialize( Interface *engineInterface )
    {
        return;
    }

    inline void Shutdown( Interface *engineInterface )
    {
        return;
    }

    // List of all registered formats.
    struct registeredExtension
    {
        const char *formatName;
        const char *defaultExt;
        imagingFormatExtension *intf;
    };

    typedef std::map <std::string, registeredExtension> formatList_t;

    formatList_t registeredFormats;

    inline bool Deserialize( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& layerOut ) const
    {
        // Loop through all imaging extensions and check which one identifies with the given stream.
        // For the one that identifies with it, try to deserialize the picture data with it.
        const int64 rasterStreamPos = inputStream->tell();

        const imagingFormatExtension *supportedExt = NULL;

        {
            bool needsPositionReset = false;

            for ( rwImagingEnv::formatList_t::const_iterator iter = this->registeredFormats.cbegin(); iter != this->registeredFormats.cend(); iter++ )
            {
                if ( needsPositionReset )
                {
                    inputStream->seek( rasterStreamPos, eSeekMode::RWSEEK_BEG );
                
                    needsPositionReset = false;
                }

                // Ask the imaging extension for support.
                const imagingFormatExtension *imgExt = (*iter).second.intf;

                bool hasSupport = false;

                try
                {
                    hasSupport = imgExt->IsStreamCompatible( engineInterface, inputStream );
                }
                catch( RwException& )
                {
                    // We do not have support, I guess.
                    hasSupport = false;
                }

                if ( hasSupport )
                {
                    supportedExt = imgExt;
                    break;
                }

                // We continue searching.
                needsPositionReset = true;
            }
        }

        // If we have a valid supported extension, try to fetch it's pixel data and put it into a Bitmap.
        if ( supportedExt != NULL )
        {
            imagingLayerTraversal fetchedLayer;

            inputStream->seek( rasterStreamPos, eSeekMode::RWSEEK_BEG );

            // Fetch stuff.
            try
            {
                supportedExt->DeserializeImage( engineInterface, inputStream, fetchedLayer );
            }
            catch( RwException& )
            {
                // The operation has failed, so we just yield.
                return false;
            }

            // Give the layer to the runtime.
            layerOut = fetchedLayer;
            return true;
        }

        return false;
    }

    inline bool Serialize( Interface *engineInterface, Stream *outputStream, const char *formatDescriptor, const imagingLayerTraversal& theLayer ) const
    {
        // TODO: make sure the imaging layer data can be safely put into the image format.
        // the image format has to export capabilities, just like the native texture rasters.
        assert( theLayer.compressionType == RWCOMPRESS_NONE );

        // We get the image format that is properly described by formatDescriptor and serialize the Bitmap with it.
        imagingFormatExtension *fittingFormat = NULL;

        for ( rwImagingEnv::formatList_t::const_iterator iter = this->registeredFormats.cbegin(); iter != this->registeredFormats.cend(); iter++ )
        {
            // We check the extension and name for case-insensitive match.
            const rwImagingEnv::registeredExtension& regExt = (*iter).second;

            if ( stricmp( regExt.formatName, formatDescriptor ) == 0 ||
                 stricmp( regExt.defaultExt, formatDescriptor ) == 0 )
            {
                // We found our format!
                fittingFormat = regExt.intf;
                break;
            }
        }

        if ( fittingFormat != NULL )
        {
            // Do it.
            try
            {
                fittingFormat->SerializeImage( engineInterface, outputStream, theLayer );
            }
            catch( ... )
            {
                return false;
            }

            // We are successful.
            return true;
        }

        // Something must have failed.
        return false;
    }
};

static PluginDependantStructRegister <rwImagingEnv, RwInterfaceFactory_t> rwImagingPluginRegister;

inline rwImagingEnv* GetImagingEnvironment( Interface *engineInterface )
{
    return rwImagingPluginRegister.GetPluginStruct( (EngineInterface*)engineInterface );
}
#endif //RWLIB_INCLUDE_IMAGING

// We also have native raster serialization functions.
// These methods should be used if the target image should store optimized texture data.
//todo.

bool DeserializeImage( Stream *inputStream, Bitmap& outputPixels )
{
    // If successful, we overwrite outputPixels with the new image data.

    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    Interface *engineInterface = inputStream->engineInterface;

    if ( const rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        imagingLayerTraversal fetchedLayer;

        bool hasFetchedLayer = imgEnv->Deserialize( engineInterface, inputStream, fetchedLayer );

        if ( hasFetchedLayer )
        {
            try
            {
                // Now that we successfully fetched stuff, we have to make sure it is not compressed.
                // If it is compressed, transform into non compressed, pronto!
                eCompressionType srcCompressionType = fetchedLayer.compressionType;

                if ( srcCompressionType != RWCOMPRESS_NONE )
                {
                    // We do want to convert to BGRA 32bit.
                    eRasterFormat dstRasterFormat = RASTER_8888;
                    eColorOrdering dstColorOrder = COLOR_BGRA;
                    uint32 dstDepth = 32;

                    uint32 dstPlaneWidth, dstPlaneHeight;
                    void *dstTexels = NULL;
                    uint32 dstDataSize = 0;

                    bool hasConvertedToRaw =
                        ConvertMipmapLayerNative(
                            engineInterface,
                            fetchedLayer.mipWidth, fetchedLayer.mipHeight, fetchedLayer.layerWidth, fetchedLayer.layerHeight, fetchedLayer.texelSource, fetchedLayer.dataSize,
                            fetchedLayer.rasterFormat, fetchedLayer.depth, fetchedLayer.colorOrder, fetchedLayer.paletteType, fetchedLayer.paletteData, fetchedLayer.paletteSize, fetchedLayer.compressionType,
                            dstRasterFormat, dstDepth, dstColorOrder, PALETTE_NONE, NULL, 0, RWCOMPRESS_NONE,
                            false,
                            dstPlaneWidth, dstPlaneHeight,
                            dstTexels, dstDataSize
                        );

                    if ( hasConvertedToRaw == false )
                    {
                        throw RwException( "failed to convert to raw pixel format" );
                    }

                    // Update stuff.
                    fetchedLayer.rasterFormat = dstRasterFormat;
                    fetchedLayer.colorOrder = dstColorOrder;
                    fetchedLayer.depth = dstDepth;
                    fetchedLayer.paletteType = PALETTE_NONE;
                    fetchedLayer.paletteData = NULL;
                    fetchedLayer.paletteSize = 0;
                    fetchedLayer.compressionType = RWCOMPRESS_NONE;

                    // Delete old texels.
                    if ( fetchedLayer.texelSource != dstTexels )
                    {
                        engineInterface->PixelFree( fetchedLayer.texelSource );
                    }

                    fetchedLayer.mipWidth = dstPlaneWidth;
                    fetchedLayer.mipHeight = dstPlaneHeight;
                    fetchedLayer.texelSource = dstTexels;
                    fetchedLayer.dataSize = dstDataSize;
                }
            }
            catch( ... )
            {
                // We must clear pixel data.
                if ( void *srcTexels = fetchedLayer.texelSource )
                {
                    engineInterface->PixelFree( srcTexels );

                    fetchedLayer.texelSource = NULL;
                }

                throw;
            }

            // Now nothing can go wrong anymore. Put things into the bitmap.
            outputPixels.setImageData(
                fetchedLayer.texelSource,
                fetchedLayer.rasterFormat, fetchedLayer.colorOrder, fetchedLayer.depth,
                fetchedLayer.mipWidth, fetchedLayer.mipHeight, fetchedLayer.dataSize, true
            );

            // Success!
            success = true;
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

bool SerializeImage( Stream *outputStream, const char *formatDescriptor, const Bitmap& inputPixels )
{
    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    Interface *engineInterface = outputStream->engineInterface;

    if ( const rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // Lets do the serialization.
        // To do that we have to temporarily put the bitmap texel data into our traversal struct.
        imagingLayerTraversal texelData;
        inputPixels.getSize( texelData.mipWidth, texelData.mipHeight );

        texelData.layerWidth = texelData.mipWidth;
        texelData.layerHeight = texelData.mipHeight;

        texelData.texelSource = inputPixels.getTexelsData();
        texelData.dataSize = inputPixels.getDataSize();

        texelData.rasterFormat = inputPixels.getFormat();
        texelData.depth = inputPixels.getDepth();
        texelData.colorOrder = inputPixels.getColorOrder();

        texelData.paletteType = PALETTE_NONE;
        texelData.paletteData = NULL;
        texelData.paletteSize = 0;

        texelData.compressionType = RWCOMPRESS_NONE;

        // Now attempt stuff.
        bool hasSerialized = imgEnv->Serialize( engineInterface, outputStream, formatDescriptor, texelData );

        if ( hasSerialized )
        {
            // We are done!
            success = true;
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

bool RegisterImagingFormat( Interface *engineInterface, const char *formatName, const char *defaultExt, imagingFormatExtension *intf )
{
    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    if ( rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // If we do not have a plugin with that name already, we register it.
        rwImagingEnv::formatList_t::const_iterator iter = imgEnv->registeredFormats.find( formatName );

        if ( iter == imgEnv->registeredFormats.end() )
        {
            // Alright, lets do this.
            rwImagingEnv::registeredExtension newExt;
            newExt.formatName = formatName;
            newExt.defaultExt = defaultExt;
            newExt.intf = intf;

            imgEnv->registeredFormats[ formatName ] = newExt;

            success = true;
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

bool UnregisterImagingFormat( Interface *engineInterface, imagingFormatExtension *intf )
{
    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    if ( rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // Attempt to find the imaging format that is described by the given interface.
        // If found, then unregister it.
        for ( rwImagingEnv::formatList_t::const_iterator iter = imgEnv->registeredFormats.begin(); iter != imgEnv->registeredFormats.end(); iter++ )
        {
            const rwImagingEnv::registeredExtension& regExt = (*iter).second;

            if ( regExt.intf == intf )
            {
                // Remove us.
                imgEnv->registeredFormats.erase( iter );

                success = true;
                break;
            }
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

// Imaging extensions.
extern void registerTGAImagingExtension( void );

void registerImagingPlugin( void )
{
#ifdef RWLIB_INCLUDE_IMAGING
    rwImagingPluginRegister.RegisterPlugin( engineFactory );

    // TODO: register all extensions that use us.
    registerTGAImagingExtension();
#endif //RWLIB_INCLUDE_IMAGING
}

}