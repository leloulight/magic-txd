#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#pragma warning(disable: 4996)
#endif //_WIN32

// This is used to force case-insensitivity in ScanDir implementations.
// Currently a compatibility setting.
#define FILESYSTEM_FORCE_CASE_INSENSITIVE_GLOB

#include <CFileSystemInterface.h>
#include <CFileSystem.h>

#ifdef __linux__
#define FILE_ACCESS_FLAG ( S_IRUSR | S_IWUSR )

#include <sys/errno.h>
#endif //__linux__

#define NUMELMS(x)      ( sizeof(x) / sizeof(*x) )