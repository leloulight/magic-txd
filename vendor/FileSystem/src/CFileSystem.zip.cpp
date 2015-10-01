/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.zip.cpp
*  PURPOSE:     ZIP archive filesystem
*
*  Docu: https://users.cs.jmu.edu/buchhofp/forensics/formats/pkzip.html
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include <StdInc.h>

// Include internal (private) definitions.
#include "fsinternal/CFileSystem.internal.h"
#include "fsinternal/CFileSystem.zip.internal.h"

extern CFileSystem *fileSystem;

#include "CFileSystem.Utils.hxx"
#include "fsinternal/CFileSystem.zip.utils.hxx"


CArchiveTranslator* zipExtension::NewArchive( CFile& writeStream )
{
    if ( !writeStream.IsWriteable() )
        return NULL;

    return new CZIPArchiveTranslator( *this, writeStream );
}

CArchiveTranslator* zipExtension::OpenArchive( CFile& readWriteStream )
{
    {
        zip_mapped_rdircheck checker;

        if ( !FileSystem::MappedReaderReverse <char [1024]>( readWriteStream, checker ) )
            return NULL;
    }

    _endDir dirEnd;

    if ( !readWriteStream.ReadStruct( dirEnd ) )
        return NULL;

    readWriteStream.Seek( -(long)dirEnd.centralDirectorySize - (long)sizeof( dirEnd ) - 4, SEEK_CUR );

    CZIPArchiveTranslator *zip = new CZIPArchiveTranslator( *this, readWriteStream );

    try
    {
        zip->ReadFiles( dirEnd.entries );
    }
    catch( ... )
    {
        delete zip;

        return NULL;
    }

    return zip;
}

CFileTranslator* zipExtension::GetTempRoot( void )
{
    return repo.GetTranslator();
}

void zipExtension::Initialize( CFileSystemNative *sys )
{
    return;
}

void zipExtension::Shutdown( CFileSystemNative *sys )
{
    return;
}

// Export methods into the CFileSystem class directly.
CArchiveTranslator* CFileSystem::CreateZIPArchive( CFile& file )
{
    zipExtension *zipExt = zipExtension::Get( this );

    if ( zipExt )
    {
        return zipExt->NewArchive( file );
    }
    return NULL;
}

CArchiveTranslator* CFileSystem::OpenArchive( CFile& file )
{
    zipExtension *zipExt = zipExtension::Get( this );

    if ( zipExt )
    {
        // TODO: code an automatic detection of the archive format.
        // Also put this function into the main file (CFileSystem.cpp).
        return zipExt->OpenArchive( file );
    }
    return NULL;
}

CArchiveTranslator* CFileSystem::OpenZIPArchive( CFile& file )
{
    zipExtension *zipExt = zipExtension::Get( this );
    
    if ( zipExt )
    {
        return zipExt->OpenArchive( file );
    }
    return NULL;
}

fileSystemFactory_t::pluginOffset_t zipExtension::_zipPluginOffset = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;

void CFileSystemNative::RegisterZIPDriver( const fs_construction_params& params )
{
    zipExtension::_zipPluginOffset =
        _fileSysFactory.RegisterDependantStructPlugin <zipExtension> ( fileSystemFactory_t::ANONYMOUS_PLUGIN_ID );
}

void CFileSystemNative::UnregisterZIPDriver( void )
{
    if ( zipExtension::_zipPluginOffset != fileSystemFactory_t::ANONYMOUS_PLUGIN_ID )
    {
        _fileSysFactory.UnregisterPlugin( zipExtension::_zipPluginOffset );
    }
}