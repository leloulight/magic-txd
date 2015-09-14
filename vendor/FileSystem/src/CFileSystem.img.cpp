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

CIMGArchiveTranslatorHandle* imgExtension::NewArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{
    // Create an archive depending on version.
    CIMGArchiveTranslator *resultArchive = NULL;
    {
        CFile *contentFile = NULL;
        CFile *registryFile = NULL;

        if ( version == IMG_VERSION_1 )
        {
            // Just open the content file.
            contentFile = srcRoot->Open( srcPath, "wb" );

            // We need to create a seperate registry file.
            std::string dirOfArchive;
            std::string extention;

            std::string nameItem = FileSystem::GetFileNameItem( srcPath, false, &dirOfArchive, &extention );

            if ( nameItem.length() != 0 )
            {
                std::string regFilePath = dirOfArchive + nameItem + ".DIR";

                // Open a seperate registry file.
                registryFile = srcRoot->Open( regFilePath.c_str(), "wb" );
            }
        }
        else if ( version == IMG_VERSION_2 )
        {
            // Just create a content file.
            contentFile = srcRoot->Open( srcPath, "wb" );

            registryFile = contentFile;
        }

        if ( contentFile && registryFile )
        {
            resultArchive = new CIMGArchiveTranslator( *this, contentFile, registryFile, version );
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

CIMGArchiveTranslatorHandle* imgExtension::OpenArchive( CFileTranslator *srcRoot, const char *srcPath )
{
    CIMGArchiveTranslatorHandle *transOut = NULL;
        
    bool hasValidArchive = false;
    eIMGArchiveVersion theVersion;

    CFile *contentFile = srcRoot->Open( srcPath, "rb+" );

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
            unsigned int checksum;
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
        std::string dirOfArchive;
        std::string extOrig;

        std::string nameItem = FileSystem::GetFileNameItem( srcPath, false, &dirOfArchive, &extOrig );

        if ( nameItem.size() != 0 )
        {
            hasUniqueRegistryFile = true;

            // Try to open the registry file.
            std::string regFilePath = dirOfArchive + nameItem + ".DIR";

            registryFile = srcRoot->Open( regFilePath.c_str(), "rb+" );

            if ( registryFile )
            {
                hasValidArchive = true;
                theVersion = IMG_VERSION_1;
            }
        }
    }

    if ( hasValidArchive )
    {
        CIMGArchiveTranslator *translator = new CIMGArchiveTranslator( *this, contentFile, registryFile, theVersion );

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

CFileTranslator* imgExtension::GetTempRoot( void )
{
    return repo.GetTranslator();
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenIMGArchive( CFileTranslator *srcRoot, const char *srcPath )
{
    imgExtension *imgExt = imgExtension::Get( this );

    if ( imgExt )
    {
        return imgExt->OpenArchive( srcRoot, srcPath );
    }
    return NULL;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{
    imgExtension *imgExt = imgExtension::Get( this );

    if ( imgExt )
    {
        return imgExt->NewArchive( srcRoot, srcPath, version );
    }
    return NULL;
}

CIMGArchiveTranslatorHandle* CFileSystem::OpenCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath )
{
    CIMGArchiveTranslatorHandle *archiveHandle = this->OpenIMGArchive( srcRoot, srcPath );

    if ( archiveHandle )
    {
        imgExtension *imgExt = imgExtension::Get( this );

        if ( imgExt )
        {
            // Set the xbox compression handler.
            archiveHandle->SetCompressionHandler( &imgExt->xboxCompressionHandler );
        }
    }

    return archiveHandle;
}

CIMGArchiveTranslatorHandle* CFileSystem::CreateCompressedIMGArchive( CFileTranslator *srcRoot, const char *srcPath, eIMGArchiveVersion version )
{
    CIMGArchiveTranslatorHandle *archiveHandle = this->CreateIMGArchive( srcRoot, srcPath, version );

    if ( archiveHandle )
    {
        imgExtension *imgExt = imgExtension::Get( this );

        if ( imgExt )
        {
            // Set the xbox compression handler.
            archiveHandle->SetCompressionHandler( &imgExt->xboxCompressionHandler );
        }
    }

    return archiveHandle;
}

fileSystemFactory_t::pluginOffset_t imgExtension::_imgPluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;

void CFileSystemNative::RegisterIMGDriver( void )
{
    imgExtension::_imgPluginOffset =
        _fileSysFactory.RegisterDependantStructPlugin <imgExtension> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID );
}

void CFileSystemNative::UnregisterIMGDriver( void )
{
    if ( imgExtension::_imgPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
    {
        _fileSysFactory.UnregisterPlugin( imgExtension::_imgPluginOffset );
    }
}