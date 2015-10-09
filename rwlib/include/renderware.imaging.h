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

// Struct to specify the imaging format extensions that the framework should use.
// This has to be stored as array in the imaging extension itself.
struct imaging_filename_ext
{
    const char *ext;
    bool isDefault;
};

// Utilities to deal with imaging_filename_ext.
bool IsImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char *query_ext );
bool GetDefaultImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array, const char*& out_ext );
const char* GetLongImagingFormatExtension( uint32 num_ext, const imaging_filename_ext *ext_array );

// Get information about all registered image formats.
struct registered_image_format
{
    const char *formatName;
    uint32 num_ext;
    const imaging_filename_ext *ext_array;
};

typedef std::vector <registered_image_format> registered_image_formats_t;

void GetRegisteredImageFormats( Interface *engineInterface, registered_image_formats_t& formatsOut );