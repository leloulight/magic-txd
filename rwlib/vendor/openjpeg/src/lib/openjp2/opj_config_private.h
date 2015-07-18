/* create opj_config_private.h for CMake */
#define OPJ_HAVE_INTTYPES_H

#define OPJ_PACKAGE_VERSION "1.0.0"

#define _LARGEFILE_SOURCE
#define _LARGE_FILES
#define _FILE_OFFSET_BITS 64
//#define OPJ_HAVE_FSEEKO

/* Byte order.  */
/* All compilers that support Mac OS X define either __BIG_ENDIAN__ or
__LITTLE_ENDIAN__ to match the endianness of the architecture being
compiled for. This is not necessarily the same as the architecture of the
machine doing the building. In order to support Universal Binaries on
Mac OS X, we prefer those defines to decide the endianness.
On other platforms we use the result of the TRY_RUN. */
#if !defined(__APPLE__)
#define OPJ_BIG_ENDIAN
#elif defined(__BIG_ENDIAN__)
#define OPJ_BIG_ENDIAN
#endif