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

#include <CExecutiveManager.h>

// Include extensions (public headers only)
#include "CFileSystem.zip.h"
#include "CFileSystem.img.h"

struct fs_construction_params
{
    inline fs_construction_params( void )
    {
        this->nativeExecMan = NULL;
    }

    NativeExecutive::CExecutiveManager *nativeExecMan;      // set this field if you want MT support in FileSystem.
};

class CFileSystem : public CFileSystemInterface
{
protected:
    // Creation is not allowed by the general runtime anymore.
                            CFileSystem             ( const fs_construction_params& params );
                            ~CFileSystem            ( void );

public:
    // Global functions for initialization of the FileSystem library.
    // There can be only one CFileSystem object alive at a time.
    static CFileSystem*     Create                  ( const fs_construction_params& params );
    static void             Destroy                 ( CFileSystem *lib );

    bool                    CanLockDirectories      ( void );

    using CFileSystemInterface::CreateTranslator;

    CFileTranslator*        CreateTranslator        ( const char *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;
    CFileTranslator*        CreateTranslator        ( const wchar_t *path, eDirOpenFlags flags = DIR_FLAG_NONE ) override final;

    CArchiveTranslator*     OpenArchive             ( CFile& file ) override final;

    CArchiveTranslator*     OpenZIPArchive          ( CFile& file ) override final;
    CArchiveTranslator*     CreateZIPArchive        ( CFile& file ) override final;

    using CFileSystemInterface::OpenIMGArchive;
    using CFileSystemInterface::CreateIMGArchive;

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const char *srcPath ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version ) override final;

    CIMGArchiveTranslatorHandle*    OpenIMGArchive      ( CFileTranslator *srcRoot, const wchar_t *srcPath ) override final;
    CIMGArchiveTranslatorHandle*    CreateIMGArchive    ( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version ) override final;
    
    using CFileSystemInterface::OpenCompressedIMGArchive;
    using CFileSystemInterface::CreateCompressedIMGArchive;

    // Special functions for IMG archives that should support compression.
    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const char *srcPath ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version ) override final;

    CIMGArchiveTranslatorHandle*    OpenCompressedIMGArchive    ( CFileTranslator *srcRoot, const wchar_t *srcPath ) override final;
    CIMGArchiveTranslatorHandle*    CreateCompressedIMGArchive  ( CFileTranslator *srcRoot, const wchar_t *arcPath, eIMGArchiveVersion version ) override final;

    // Function to cast a CFileTranslator into a CArchiveTranslator.
    // If not possible, it returns NULL.
    CArchiveTranslator*     GetArchiveTranslator    ( CFileTranslator *translator );

    // Temporary directory generation for temporary data storage.
    CFileTranslator*        GetSystemTempTranslator ( void )                { return sysTmp; }
    CFileTranslator*        GenerateTempRepository  ( void );

    // Insecure functions
    bool                    IsDirectory             ( const char *path ) override final;
#ifdef _WIN32
    bool                    WriteMiniDump           ( const char *path, _EXCEPTION_POINTERS *except );
#endif //_WIN32
    bool                    Exists                  ( const char *path ) override final;
    size_t                  Size                    ( const char *path ) override final;
    bool                    ReadToBuffer            ( const char *path, std::vector <char>& output ) override final;

    // Settings.
    void                    SetIncludeAllDirectoriesInScan  ( bool enable ) final       { m_includeAllDirsInScan = enable; }
    bool                    GetIncludeAllDirectoriesInScan  ( void ) const final        { return m_includeAllDirsInScan; }

    // Members.
    bool                    m_includeAllDirsInScan;     // decides whether ScanDir implementations should apply patterns on directories
#ifdef _WIN32
    bool                    m_hasDirectoryAccessPriviledge; // decides whether directories can be locked by the application
#endif //_WIN32

    // Temporary directory generator members.
    CFileTranslator*        sysTmp;

    NativeExecutive::CExecutiveManager *nativeMan;
};

// These variables are exported for easy usage by the application.
// It is common sense that an application has a local filesystem and resides on a space.
extern CFileSystem *fileSystem;     // the local filesystem
extern CFileTranslator *fileRoot;   // the space it resides on

#endif //_CFileSystem_
