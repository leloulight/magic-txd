/*
    RenderWare bitmap loading pipeline for popular image formats.
*/

// The main API for pushing and pulling pixels.
bool DeserializeImage( Stream *inputStream, Bitmap& outputPixels );
bool SerializeImage( Stream *outputStream, const char *formatDescriptor, const Bitmap& inputPixels );