/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.img.cpp
*  PURPOSE:     IMG R* Games archive management
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.img.internal.h"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"


void imgExtension::Initialize( CFileSystemNative *sys )
{
    return;
}

void imgExtension::Shutdown( CFileSystemNative *sys )
{
    return;
}

template <typename charType>
inline const charType* GetReadWriteMode( bool isNew )
{
    static_assert( false, "invalid character type" );
}

template <>
inline const char* GetReadWriteMode <char> ( bool isNew )
{
    return ( isNew ? "wb" : "rb+" );
}

template <>
inline const wchar_t* GetReadWriteMode <wchar_t> ( bool isNew )
{
    return ( isNew ? L"wb" : L"rb+" );
}

template <typename charType>
inline CFile* OpenSeperateIMGRegistryFile( CFileTranslator *srcRoot, const charType *imgFilePath, bool isNew )
{
    CFile *registryFile = NULL;

    filePath dirOfArchive;
    filePath extention;

    filePath nameItem = FileSystem::GetFileNameItem( imgFilePath, false, &dirOfArchive, &extention );

    if ( nameItem.size() != 0 )
    {
        filePath regFilePath = dirOfArchive + nameItem + ".DIR";

        // Open a seperate registry file.
        registryFile = srcRoot->Open( regFilePath, GetReadWriteMode <wchar_t> ( isNew ), FILE_FLAG_WRITESHARE );
    }

    return registryFile;
}

template <typename charType, typename handlerType>
static AINLINE CIMGArchiveTranslatorHandle* GenNewArchiveTemplate( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version, handlerType handler )
{
    // Create an archive depending on version.
    CIMGArchiveTranslatorHandle *resultArchive = NULL;
    {
        CFile *contentFile = NULL;
        CFile *registryFile = NULL;

        if ( version == IMG_VERSION_1 )
        {
            // Just open the content file.
            contentFile = srcRoot->Open( srcPath, GetReadWriteMode <charType> ( true ), FILE_FLAG_WRITESHARE );

            // We need to create a seperate registry file.
            registryFile = OpenSeperateIMGRegistryFile( srcRoot, srcPath, true );
        }
        else if ( version == IMG_VERSION_2 )
        {
            // Just create a content file.
            contentFile = srcRoot->Open( srcPath, GetReadWriteMode <charType> ( true ), FILE_FLAG_WRITESHARE );

            registryFile = contentFile;
        }

        if ( contentFile && registryFile )
        {
            resultArchive = handler( env, registryFile, contentFile, version );
        }

        if ( !resultArchive )
        {
            if ( contentFile )
            {
                delete contentFile;
            }

            if ( registryFile && registryFile != contentFile )
            {
                delete registryFile;
            }
        }
    }
    return resultArchive;
}

static AINLINE CIMGArchiveTranslator* _regularIMGConstructor( imgExtension *env, CFile *registryFile, CFile *contentFile, eIMGArchiveVersion version )
{
    return new CIMGArchiveTranslator( *env, contentFile, registryFile, version );
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenNewArchive( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version )
{
    return GenNewArchiveTemplate( env, srcRoot, srcPath, version, _regularIMGConstructor );
}

CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{ return GenNewArchive( this, srcRoot, srcPath, version ); }
CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version )
{ return GenNewArchive( this, srcRoot, srcPath, version ); }

template <typename charType, typename constructionHandler>
static inline CIMGArchiveTranslatorHandle* GenOpenArchiveTemplate( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath, constructionHandler constr )
{
    CIMGArchiveTranslatorHandle *transOut = NULL;
        
    bool hasValidArchive = false;
    eIMGArchiveVersion theVersion;

    CFile *contentFile = srcRoot->Open( srcPath, GetReadWriteMode <charType> ( false ), FILE_FLAG_WRITESHARE );

    if ( !contentFile )
    {
        return NULL;
    }

    bool hasUniqueRegistryFile = false;
    CFile *registryFile = NULL;

    // Check for version 2.
    struct mainHeader
    {
        union
        {
            unsigned char version[4];
            fsUInt_t checksum;
        };
    };

    mainHeader imgHeader;

    bool hasReadMainHeader = contentFile->ReadStruct( imgHeader );

    if ( hasReadMainHeader && imgHeader.checksum == '2REV' )
    {
        hasValidArchive = true;
        theVersion = IMG_VERSION_2;

        registryFile = contentFile;
    }

    if ( !hasValidArchive )
    {
        // Check for version 1.
        hasUniqueRegistryFile = true;

        registryFile = OpenSeperateIMGRegistryFile( srcRoot, srcPath, false );
        
        if ( registryFile )
        {
            hasValidArchive = true;
            theVersion = IMG_VERSION_1;
        }
    }

    if ( hasValidArchive )
    {
        CIMGArchiveTranslator *translator = constr( env, registryFile, contentFile, theVersion );

        if ( translator )
        {
            bool loadingSuccess = translator->ReadArchive();

            if ( loadingSuccess )
            {
                transOut = translator;
            }
            else
            {
                delete translator;

                contentFile = NULL;
                registryFile = NULL;
            }
        }
    }

    if ( !transOut )
    {
        if ( contentFile )
        {
            delete contentFile;

            contentFile = NULL;
        }

        if ( hasUniqueRegistryFile && registryFile )
        {
            delete registryFile;

            registryFile = NULL;
        }
    }

    return transOut;
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenArchive( imgExtension *env, CFileTranslator *srcRoot, const charType *srcPath )
{
    return GenOpenArchiveTemplate( env, srcRoot, srcPath, _regularIMGConstructor );
}

CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const char *srcPath )
{ return GenOpenArchive( this, srcRoot, srcPath ); }
CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const wchar_t *srcPath )
{ return GenOpenArchive( this, srcRoot, srcPath ); }

CFileTranslator* imgExtension::GetTempRoot( void )
{
    return repo.GetTranslator();
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath )
{
    imgExtension *imgExt = imgExtension::Get( sys );

    if ( imgExt )
    {
        return imgExt->OpenArchive( srcRoot, srcPath );
    }
    return NULL;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const char *srcPath )
{ return GenOpenIMGArchive( this, srcRoot, srcPath ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath )
{ return GenOpenIMGArchive( this, srcRoot, srcPath ); }

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenCreateIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version )
{
    imgExtension *imgExt = imgExtension::Get( sys );

    if ( imgExt )
    {
        return imgExt->NewArchive( srcRoot, srcPath, version );
    }
    return NULL;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{ return GenCreateIMGArchive( this, srcRoot, srcPath, version ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version )
{ return GenCreateIMGArchive( this, srcRoot, srcPath, version ); }

#pragma warning(push)
#pragma warning(disable:4250)

struct CIMGArchiveTranslator_lzo : public CIMGArchiveTranslator
{
    inline CIMGArchiveTranslator_lzo( imgExtension& imgExt, CFile *contentFile, CFile *registryFile, eIMGArchiveVersion theVersion )
        : CIMGArchiveTranslator( imgExt, contentFile, registryFile, theVersion )
    {
        // Set the compression provider.
        this->SetCompressionHandler( &compression );
    }

    inline ~CIMGArchiveTranslator_lzo( void )
    {
        // We must unset the compression handler.
        this->SetCompressionHandler( NULL );
    }

    // We need a compressor per translator, so we can compress simultaneously on multiple threads.
    xboxIMGCompression compression;
};

#pragma warning(pop)

static AINLINE CIMGArchiveTranslator* _lzoCompressedIMGConstructor( imgExtension *env, CFile *registryFile, CFile *contentFile, eIMGArchiveVersion version )
{
    return new CIMGArchiveTranslator_lzo( *env, contentFile, registryFile, version );
}

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenOpenCompressedIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath )
{
    CIMGArchiveTranslatorHandle *archiveHandle = NULL;
    {
        imgExtension *imgExt = imgExtension::Get( sys );

        if ( imgExt )
        {
            // Create a translator specifically with the LZO compression algorithm.
            archiveHandle = GenOpenArchiveTemplate( imgExt, srcRoot, srcPath, _lzoCompressedIMGConstructor );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath )
{ return GenOpenCompressedIMGArchive( this, srcRoot, srcPath ); }
CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath )
{ return GenOpenCompressedIMGArchive( this, srcRoot, srcPath ); }

template <typename charType>
static inline CIMGArchiveTranslatorHandle* GenCreateCompressedIMGArchive( CFileSystem *sys, CFileTranslator *srcRoot, const charType *srcPath, eIMGArchiveVersion version )
{
    CIMGArchiveTranslatorHandle *archiveHandle = NULL;
    {
        imgExtension *imgExt = imgExtension::Get( sys );

        if ( imgExt )
        {
            // Create a translator specifically with the LZO compression algorithm.
            archiveHandle = GenNewArchiveTemplate( imgExt, srcRoot, srcPath, version, _lzoCompressedIMGConstructor );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{ return GenCreateCompressedIMGArchive( this, srcRoot, srcPath, version ); }
CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const wchar_t *srcPath, eIMGArchiveVersion version )
{ return GenCreateCompressedIMGArchive( this, srcRoot, srcPath, version ); }

// Sub modules.
extern void InitializeXBOXIMGCompressionEnvironment( const fs_construction_params& params );

extern void ShutdownXBOXIMGCompressionEnvironment( void );

fileSystemFactory_t::pluginOffset_t imgExtension::_imgPluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;

void CFileSystemNative::RegisterIMGDriver( const fs_construction_params& params )
{
    imgExtension::_imgPluginOffset =
        _fileSysFactory.RegisterDependantStructPlugin <imgExtension> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID );

    // Register sub modules.
    InitializeXBOXIMGCompressionEnvironment( params );
}

void CFileSystemNative::UnregisterIMGDriver( void )
{
    // Unregister sub modules.
    ShutdownXBOXIMGCompressionEnvironment();

    if ( imgExtension::_imgPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
    {
        _fileSysFactory.UnregisterPlugin( imgExtension::_imgPluginOffset );
    }
}