#include "StdInc.h"

#include "rwimaging.hxx"

#include "pixelformat.hxx"

#include "pixelutil.hxx"

#include "txdread.d3d.dxt.hxx"

#include <PluginHelpers.h>

#include <map>

// Define RWLIB_INCLUDE_IMAGING in rwconf.h to enable this component.

namespace rw
{

#ifdef RWLIB_INCLUDE_IMAGING
// This component is used to convert Streams into a Bitmap and the other way round.
// The user should be able to decide in rwconf.h what modules he wants to ship with, or to trim the imaging altogether.

inline void CompatibilityTransformImagingLayer( Interface *engineInterface, const imagingLayerTraversal& layer, imagingLayerTraversal& layerOut, const pixelCapabilities& pixelCaps )
{
    // TODO: this is a copy of the code from txdread but adjusted for a single mipmap.
    // I hope I can somehow combine both functions together... because this is complicated.

    // Make sure the imaging layer does not violate the capabilities struct.
    // This is done by "downcasting". It preserves maximum image quality, but increases memory requirements.

    // First get the original parameters onto stack.
    eRasterFormat srcRasterFormat = layer.rasterFormat;
    uint32 srcDepth = layer.depth;
    uint32 srcRowAlignment = layer.rowAlignment;
    eColorOrdering srcColorOrder = layer.colorOrder;
    ePaletteType srcPaletteType = layer.paletteType;
    void *srcPaletteData = layer.paletteData;
    uint32 srcPaletteSize = layer.paletteSize;
    eCompressionType srcCompressionType = layer.compressionType;

    // Now decide the target format depending on the capabilities.
    eRasterFormat dstRasterFormat;
    uint32 dstDepth;
    uint32 dstRowAlignment;
    eColorOrdering dstColorOrder;
    ePaletteType dstPaletteType;
    void *dstPaletteData;
    uint32 dstPaletteSize;
    eCompressionType dstCompressionType;

    bool wantsUpdate =
        TransformDestinationRasterFormat(
            engineInterface,
            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
            pixelCaps, layer.hasAlpha
         );

    uint32 srcMipWidth = layer.mipWidth;
    uint32 srcMipHeight = layer.mipHeight;
    uint32 srcLayerWidth = layer.layerWidth;
    uint32 srcLayerHeight = layer.layerHeight;
    void *srcTexels = layer.texelSource;
    uint32 srcDataSize = layer.dataSize;

    uint32 dstMipWidth = srcMipWidth;
    uint32 dstMipHeight = srcMipHeight;
    uint32 dstLayerWidth = srcLayerWidth;
    uint32 dstLayerHeight = srcLayerHeight;
    void *dstTexels = srcTexels;
    uint32 dstDataSize = srcDataSize;

    if ( wantsUpdate )
    {
        // Convert the pixels now.
        bool hasUpdated;
        {
            // Just convert.
            uint32 dstPlaneWidth, dstPlaneHeight;
            void *newtexels;
            uint32 newdatasize;

            bool couldConvert = ConvertMipmapLayerNative(
                engineInterface, layer.mipWidth, layer.mipHeight, layer.layerWidth, layer.layerHeight, layer.texelSource, layer.dataSize,
                srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, srcCompressionType,
                dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, dstPaletteType, dstPaletteData, dstPaletteSize, dstCompressionType,
                false,
                dstPlaneWidth, dstPlaneHeight,
                newtexels, newdatasize
            );

            if ( couldConvert )
            {
                dstMipWidth = dstPlaneWidth;
                dstMipHeight = dstPlaneHeight;
                
                // We assume the input layer is _never_ newly allocated.
                // Now it is, tho. MAKE SURE you handle this right!
                dstTexels = newtexels;
                dstDataSize = newdatasize;

                hasUpdated = true;
            }
        }

        // If we want an update, we should get an update.
        // Otherwise, ConvertPixelData is to blame.
        assert( hasUpdated == true );
    }

    // Put data into the new struct.
    layerOut.mipWidth = dstMipWidth;
    layerOut.mipHeight = dstMipHeight;
    layerOut.layerWidth = dstLayerWidth;
    layerOut.layerHeight = dstLayerHeight;
    layerOut.texelSource = dstTexels;
    layerOut.dataSize = dstDataSize;

    layerOut.rasterFormat = dstRasterFormat;
    layerOut.depth = dstDepth;
    layerOut.rowAlignment = dstRowAlignment;
    layerOut.colorOrder = dstColorOrder;
    layerOut.paletteType = dstPaletteType;
    layerOut.paletteData = dstPaletteData;
    layerOut.paletteSize = dstPaletteSize;
    layerOut.compressionType = dstCompressionType;
}

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
            // Get the capabilities of the imaging layer and decide whether it needs a transform.
            pixelCapabilities imageCaps;

            fittingFormat->GetStorageCapabilities( imageCaps );

            // Create a copy, because the input is const.
            imagingLayerTraversal layerPush;

            CompatibilityTransformImagingLayer( engineInterface, theLayer, layerPush, imageCaps );

            bool success = true;

            try
            {
                // Serialize the legit pixels.
                fittingFormat->SerializeImage( engineInterface, outputStream, layerPush );
            }
            catch( ... )
            {
                success = false;
            }

            // If we allocated new pixels, free the new ones.
            if ( theLayer.texelSource != layerPush.texelSource )
            {
                engineInterface->PixelFree( layerPush.texelSource );
            }

            // We could be successful, hopefully.
            return success;
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
bool DeserializeMipmapLayer( Stream *inputStream, rawMipmapLayer& rawLayer )
{
    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    Interface *engineInterface = inputStream->engineInterface;

    if ( const rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // Fetch pixel data. All formats can be displayed in rawMipmapLayer.
        imagingLayerTraversal travData;

        bool hasDeserialized = imgEnv->Deserialize( engineInterface, inputStream, travData );

        if ( hasDeserialized )
        {
            // Just give the data to the runtime.
            rawLayer.mipData.width = travData.mipWidth;
            rawLayer.mipData.height = travData.mipHeight;
            rawLayer.mipData.mipWidth = travData.layerWidth;
            rawLayer.mipData.mipHeight = travData.layerHeight;
            rawLayer.mipData.texels = travData.texelSource;
            rawLayer.mipData.dataSize = travData.dataSize;

            rawLayer.rasterFormat = travData.rasterFormat;
            rawLayer.depth = travData.depth;
            rawLayer.rowAlignment = travData.rowAlignment;
            rawLayer.colorOrder = travData.colorOrder;
            rawLayer.paletteType = travData.paletteType;
            rawLayer.paletteData = travData.paletteData;
            rawLayer.paletteSize = travData.paletteSize;
            rawLayer.compressionType = travData.compressionType;

            rawLayer.hasAlpha = false;  // TODO.

            // Done!
            success = true;
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

bool SerializeMipmapLayer( Stream *outputStream, const char *formatDescriptor, const rawMipmapLayer& rawLayer )
{
    bool success = false;

#ifdef RWLIB_INCLUDE_IMAGING
    Interface *engineInterface = outputStream->engineInterface;

    if ( const rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // We put data into the traversal struct and push it to the imaging format manager.
        imagingLayerTraversal travData;
        travData.layerWidth = rawLayer.mipData.mipWidth;
        travData.layerHeight = rawLayer.mipData.mipHeight;
        travData.mipWidth = rawLayer.mipData.width;
        travData.mipHeight = rawLayer.mipData.height;
        travData.texelSource = rawLayer.mipData.texels;
        travData.dataSize = rawLayer.mipData.dataSize;

        travData.rasterFormat = rawLayer.rasterFormat;
        travData.depth = rawLayer.depth;
        travData.rowAlignment = rawLayer.rowAlignment;
        travData.colorOrder = rawLayer.colorOrder;
        travData.paletteType = rawLayer.paletteType;
        travData.paletteData = rawLayer.paletteData;
        travData.paletteSize = rawLayer.paletteSize;
        travData.compressionType = rawLayer.compressionType;

        travData.hasAlpha = rawLayer.hasAlpha;

        // Now send it to our serializer.
        bool hasSerialized = imgEnv->Serialize( engineInterface, outputStream, formatDescriptor, travData );

        if ( hasSerialized )
        {
            // OK!
            success = true;
        }
    }
#endif //RWLIB_INCLUDE_IMAGING

    return success;
}

// Those are generic routines for pushing RGBA data to image formats.
// Most likely those image formats will be supported the most.
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

                    uint32 dstRowAlignment = 4; // good measure.

                    uint32 dstPlaneWidth, dstPlaneHeight;
                    void *dstTexels = NULL;
                    uint32 dstDataSize = 0;

                    bool hasConvertedToRaw =
                        ConvertMipmapLayerNative(
                            engineInterface,
                            fetchedLayer.mipWidth, fetchedLayer.mipHeight, fetchedLayer.layerWidth, fetchedLayer.layerHeight, fetchedLayer.texelSource, fetchedLayer.dataSize,
                            fetchedLayer.rasterFormat, fetchedLayer.depth, fetchedLayer.rowAlignment, fetchedLayer.colorOrder, fetchedLayer.paletteType, fetchedLayer.paletteData, fetchedLayer.paletteSize, fetchedLayer.compressionType,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, PALETTE_NONE, NULL, 0, RWCOMPRESS_NONE,
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
                    fetchedLayer.rowAlignment = dstRowAlignment;
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

                // If we are a palette, we must unfold to non-palette.
                // This is done by converting the image data to default raster representation.
                if ( fetchedLayer.paletteType != PALETTE_NONE )
                {
                    eRasterFormat rasterFormat = fetchedLayer.rasterFormat;
                    uint32 depth = fetchedLayer.depth;
                    uint32 rowAlignment = fetchedLayer.rowAlignment;
                    eColorOrdering colorOrder = fetchedLayer.colorOrder;

                    ePaletteType paletteType = fetchedLayer.paletteType;
                    void *paletteData = fetchedLayer.paletteData;
                    uint32 paletteSize = fetchedLayer.paletteSize;

                    void *srcTexels = fetchedLayer.texelSource;
                    uint32 srcDataSize = fetchedLayer.dataSize;

                    // Determine the default raster representation.
                    uint32 dstItemDepth = Bitmap::getRasterFormatDepth( rasterFormat );

                    uint32 dstPlaneWidth, dstPlaneHeight;
                    void *newtexels = NULL;
                    uint32 dstDataSize = 0;

                    bool couldConvert =
                        ConvertMipmapLayerNative(
                            engineInterface, fetchedLayer.mipWidth, fetchedLayer.mipHeight, fetchedLayer.layerWidth, fetchedLayer.layerHeight,
                            srcTexels, srcDataSize,
                            rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                            rasterFormat, dstItemDepth, rowAlignment, colorOrder, PALETTE_NONE, NULL, 0, RWCOMPRESS_NONE,
                            false,
                            dstPlaneWidth, dstPlaneHeight,
                            newtexels, dstDataSize
                        );

                    if ( couldConvert == false )
                    {
                        throw RwException( "could not expand palette mipmap for Bitmap deserialization" );
                    }

                    // Free the old texels.
                    engineInterface->PixelFree( srcTexels );
                    engineInterface->PixelFree( paletteData );

                    // The raster format has sligthly changed.
                    fetchedLayer.depth = dstItemDepth;
                    fetchedLayer.paletteType = PALETTE_NONE;
                    fetchedLayer.paletteSize = 0;
                    fetchedLayer.paletteData = NULL;

                    // Store things into the mipmap layer.
                    fetchedLayer.mipWidth = dstPlaneWidth;
                    fetchedLayer.mipHeight = dstPlaneHeight;
                    fetchedLayer.texelSource = newtexels;
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

            assert( fetchedLayer.paletteType == PALETTE_NONE );

            // Now nothing can go wrong anymore. Put things into the bitmap.
            outputPixels.setImageData(
                fetchedLayer.texelSource,
                fetchedLayer.rasterFormat, fetchedLayer.colorOrder, fetchedLayer.depth, fetchedLayer.rowAlignment,
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
        texelData.rowAlignment = inputPixels.getRowAlignment();
        texelData.colorOrder = inputPixels.getColorOrder();

        texelData.paletteType = PALETTE_NONE;
        texelData.paletteData = NULL;
        texelData.paletteSize = 0;

        texelData.compressionType = RWCOMPRESS_NONE;

        texelData.hasAlpha = false; // TODO.

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

// Public function to get all registered imaging formats.
void GetRegisteredImageFormats( Interface *engineInterface, registered_image_formats_t& formatsOut )
{
#ifdef RWLIB_INCLUDE_IMAGING
    if ( const rwImagingEnv *imgEnv = GetImagingEnvironment( engineInterface ) )
    {
        // Loop through all formats and add their information.
        for ( rwImagingEnv::formatList_t::const_iterator iter = imgEnv->registeredFormats.begin(); iter != imgEnv->registeredFormats.end(); iter++ )
        {
            const rwImagingEnv::registeredExtension& ext = (*iter).second;

            registered_image_format formatInfo;
            formatInfo.defaultExt = ext.defaultExt;
            formatInfo.formatName = ext.formatName;

            formatsOut.push_back( formatInfo );
        }
    }
#endif //RWLIB_INCLUDE_IMAGING
}

// Imaging extensions.
extern void registerTGAImagingExtension( void );
extern void registerBMPImagingExtension( void );
extern void registerPNGImagingExtension( void );
extern void registerJPEGImagingExtension( void );

void registerImagingPlugin( void )
{
#ifdef RWLIB_INCLUDE_IMAGING
    rwImagingPluginRegister.RegisterPlugin( engineFactory );

    // TODO: register all extensions that use us.
    registerTGAImagingExtension();
    registerBMPImagingExtension();
    registerPNGImagingExtension();
    registerJPEGImagingExtension();
#endif //RWLIB_INCLUDE_IMAGING
}

}