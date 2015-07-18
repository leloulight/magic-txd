// This file is the central configuration file of rwtools.
// Here you can turn on/off features at compiletime and tweak trivial things.
// Follow the instructions in the comments.

#ifndef _RENDERWARE_CONFIGURATION_
#define _RENDERWARE_CONFIGURATION_

// Define this macro if you have a "xdk/" folder in your vendors with all the
// XBOX Development Kit headers. This will use correct and optimized swizzling
// algorithms. Having this macro defined is recommended.
#define _USE_XBOX_SDK_

// Define this macro if you want to include imaging support in your rwlib compilation.
// This will allow you to store texel data of textures in popular picture formats, such as TGA.
#define RWLIB_INCLUDE_IMAGING

// Define those if you want to ship with specific imaging format support.
// If you undefine those, you can save quite some library space.
#define RWLIB_INCLUDE_TGA_IMAGING
#define RWLIB_INCLUDE_BMP_IMAGING
#define RWLIB_INCLUDE_PNG_IMAGING
#define RWLIB_INCLUDE_JPEG_IMAGING
#define RWLIB_INCLUDE_GIF_IMAGING

// Define this if you want to enable the "libimagequant" library for palletizing
// image data. This is highly recommended, as that library produces high
// quality color-mapped images.
#define RWLIB_INCLUDE_LIBIMAGEQUANT

#endif //_RENDERWARE_CONFIGURATION_