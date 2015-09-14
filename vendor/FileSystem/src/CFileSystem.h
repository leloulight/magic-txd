/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.h
*  PURPOSE:     File management
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _CFileSystem_
#define _CFileSystem_

#define FILE_FLAG_TEMPORARY     0x00000001
#define FILE_FLAG_UNBUFFERED    0x00000002
#define FILE_FLAG_GRIPLOCK      0x00000004
#define FILE_FLAG_WRITESHARE    0x00000008

// Include extensions (public headers only)
#include "CFileSystem.zip.h"
#include "CFileSystem.img.h"

class CFileSystem : public CFileSystemInterface
{
protected:
    // Creation is not allowed by the general runtime anymore.
                            CFileSystem             ( void );
                            ~CFileSystem            ( void );

public:
    // Global functions for initialization of the FileSystem library.
    static CFileSystem*     Create                  ( void );
    static void             Destroy                 ( CFileSystem *lib );

    void                    InitZIP                 ( void );
    void                    DestroyZIP              ( void );

    bool                    CanLockDirectories      ( void );

    CFileTranslator*        CreateTranslator        ( const char *path );
    CArchiveTranslator*     OpenArchive             ( CFile& file );

    CArchiveTranslator*     OpenZIPArchive          ( CFile& file );
    CArchiveTranslator*     CreateZIPArchive        ( CFile& file );

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const char *srcPath );
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version );

    // Special functions for IMG archives that should support compression.
    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath );
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version );

    // Function to cast a CFileTranslator into a CArchiveTranslator.
    // If not possible, it returns NULL.
    CArchiveTranslator*     GetArchiveTranslator    ( CFileTranslator *translator );

    // Temporary directory generation for temporary data storage.
    CFileTranslator*        GetSystemTempTranslator ( void )                { return sysTmp; }
    CFileTranslator*        GenerateTempRepository  ( void );

    // Insecure functions
    bool                    IsDirectory             ( const char *path );
#ifdef _WIN32
    bool                    WriteMiniDump           ( const char *path, _EXCEPTION_POINTERS *except );
#endif //_WIN32
    bool                    Exists                  ( const char *path );
    size_t                  Size                    ( const char *path );
    bool                    ReadToBuffer            ( const char *path, std::vector <char>& output );

    // Settings.
    void                    SetIncludeAllDirectoriesInScan  ( bool enable )             { m_includeAllDirsInScan = enable; }
    bool                    GetIncludeAllDirectoriesInScan  ( void ) const              { return m_includeAllDirsInScan; }

    // Members.
    bool                    m_includeAllDirsInScan;     // decides whether ScanDir implementations should apply patterns on directories
#ifdef _WIN32
    bool                    m_hasDirectoryAccessPriviledge; // decides whether directories can be locked by the application
#endif //_WIN32

    // Temporary directory generator members.
    CFileTranslator*        sysTmp;
};

// These variables are exported for easy usage by the application.
// It is common sense that an application has a local filesystem and resides on a space.
extern CFileSystem *fileSystem;     // the local filesystem
extern CFileTranslator *fileRoot;   // the space it resides on

#endif //_CFileSystem_
