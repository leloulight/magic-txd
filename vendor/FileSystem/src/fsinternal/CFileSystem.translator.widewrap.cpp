/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/fsinternal/CFileSystem.translator.widewrap.cpp
*  PURPOSE:     File Translator wrapper for ANSI-only filesystems.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"

// Include the internal things.
#include "CFileSystem.internal.h"

bool CFileTranslatorWideWrap::CreateDir( const wchar_t *path )
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    return CreateDir( ansiPath.c_str() );
}

CFile* CFileTranslatorWideWrap::Open( const wchar_t *path, const wchar_t *mode )
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    filePath wideMode( mode );
    std::string ansiMode = wideMode.convert_ansi();

    return Open( ansiPath.c_str(), ansiMode.c_str() );
}

bool CFileTranslatorWideWrap::Exists( const wchar_t *path ) const
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    return Exists( ansiPath.c_str() );
}

bool CFileTranslatorWideWrap::Delete( const wchar_t *path )
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    return Delete( ansiPath.c_str() );
}

bool CFileTranslatorWideWrap::Copy( const wchar_t *src, const wchar_t *dst )
{
    filePath wideSrc( src );
    std::string ansiSrc = wideSrc.convert_ansi();

    filePath wideDst( dst );
    std::string ansiDst = wideDst.convert_ansi();

    return Copy( ansiSrc.c_str(), ansiDst.c_str() );
}

bool CFileTranslatorWideWrap::Rename( const wchar_t *src, const wchar_t *dst )
{
    filePath wideSrc( src );
    std::string ansiSrc = wideSrc.convert_ansi();

    filePath wideDst( dst );
    std::string ansiDst = wideDst.convert_ansi();

    return Rename( ansiSrc.c_str(), ansiDst.c_str() );
}

size_t CFileTranslatorWideWrap::Size( const wchar_t *path ) const
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    return Size( path );
}

bool CFileTranslatorWideWrap::Stat( const wchar_t *path, struct stat *statOut ) const
{
    filePath widePath( path );
    std::string ansiPath = widePath.convert_ansi();

    return Stat( ansiPath.c_str(), statOut );
}

void CFileTranslatorWideWrap::ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse, pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
{
    filePath wideDir( directory );
    std::string ansiDir = wideDir.convert_ansi();

    filePath wideWildcard( wildcard );
    std::string ansiWildcard = wideWildcard.convert_ansi();

    return ScanDirectory( ansiDir.c_str(), ansiWildcard.c_str(), recurse, dirCallback, fileCallback, userdata );
}

void CFileTranslatorWideWrap::GetDirectories( const wchar_t *directory, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    filePath wideDir( directory );
    std::string ansiDir = wideDir.convert_ansi();

    filePath wideWildcard( wildcard );
    std::string ansiWildcard = wideWildcard.convert_ansi();

    return GetDirectories( ansiDir.c_str(), ansiWildcard.c_str(), recurse, output );
}

void CFileTranslatorWideWrap::GetFiles( const wchar_t *directory, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const
{
    filePath wideDir( directory );
    std::string ansiDir = wideDir.convert_ansi();

    filePath wideWildcard( wildcard );
    std::string ansiWildcard = wideWildcard.convert_ansi();

    return GetFiles( ansiDir.c_str(), ansiWildcard.c_str(), recurse, output );
}