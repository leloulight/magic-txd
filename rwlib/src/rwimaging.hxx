// Internal header for the imaging components and environment.
namespace rw
{

struct imagingLayerTraversal
{
    // We do want to support compression traversal with this struct.
    // That is why this looks like a mipmap layer, but not really.
    uint32 layerWidth, layerHeight;
    uint32 mipWidth, mipHeight;
    void *texelSource;  // always newly allocated.
    uint32 dataSize;

    eRasterFormat rasterFormat;
    uint32 depth;
    eColorOrdering colorOrder;
    ePaletteType paletteType;
    void *paletteData;
    uint32 paletteSize;
    eCompressionType compressionType;
};

// Interface for various image formats that this library should support.
struct imagingFormatExtension abstract
{
    // Method to verify whether a stream should be compatible with this format.
    // This method does not have to completely verify the integrity of the stream, so that deserialization may fail anyway.
    virtual bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const = 0;

    // Pull and fetch methods.
    virtual void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const = 0;
    virtual void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const = 0;
};

// Function to register new imaging formats.
bool RegisterImagingFormat( Interface *engineInterface, const char *formatName, const char *defaultExt, imagingFormatExtension *intf );
bool UnregisterImagingFormat( Interface *engineInterface, imagingFormatExtension *intf );

}