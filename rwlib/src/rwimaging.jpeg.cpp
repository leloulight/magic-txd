#include "StdInc.h"

#include "rwimaging.hxx"

#include "pluginutil.hxx"

#include <openjpeg.h>

namespace rw
{

// JPEG-2000 compliant serialization library for RenderWare.
struct jpegImagingExtension : public imagingFormatExtension
{
    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        // TODO.
        return false;
    }

    void GetStorageCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const override
    {
        // TODO.
    }

    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const override
    {
        // TODO.
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterImagingFormat( engineInterface, "JPEG-2000", "JPEG", this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterImagingFormat( engineInterface, this );
    }
};

static PluginDependantStructRegister <jpegImagingExtension, RwInterfaceFactory_t> jpegExtensionStore;

void registerJPEGImagingExtension( void )
{
    jpegExtensionStore.RegisterPlugin( engineFactory );
}

};