/*
    RenderWare bitmap loading pipeline for popular image formats.
*/

// Special optimized mipmap pushing algorithms.
bool DeserializeMipmapLayer( Stream *inputStream, rawMipmapLayer& rawLayer );
bool SerializeMipmapLayer( Stream *outputStream, const char *formatDescriptor, const rawMipmapLayer& rawLayer );

// The main API for pushing and pulling pixels.
bool DeserializeImage( Stream *inputStream, Bitmap& outputPixels );
bool SerializeImage( Stream *outputStream, const char *formatDescriptor, const Bitmap& inputPixels );