/*
    RenderWare bitmap loading pipeline for popular image formats.
*/

// Special optimized mipmap pushing algorithms.
bool DeserializeMipmapLayer( Stream *inputStream, rawMipmapLayer& rawLayer );
bool SerializeMipmapLayer( Stream *outputStream, const char *formatDescriptor, const rawMipmapLayer& rawLayer );

bool IsImagingFormatAvailable( Interface *engineInterface, const char *formatDescriptor );

// The main API for pushing and pulling pixels.
bool DeserializeImage( Stream *inputStream, Bitmap& outputPixels );
bool SerializeImage( Stream *outputStream, const char *formatDescriptor, const Bitmap& inputPixels );

// Get information about all registered image formats.
struct registered_image_format
{
    const char *defaultExt;
    const char *formatName;
};

typedef std::vector <registered_image_format> registered_image_formats_t;

void GetRegisteredImageFormats( Interface *engineInterface, registered_image_formats_t& formatsOut );