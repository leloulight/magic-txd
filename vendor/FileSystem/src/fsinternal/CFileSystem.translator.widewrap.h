/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/fsinternal/CFileSystem.translator.widewrap.h
*  PURPOSE:     File Translator wrapper for ANSI-only filesystems.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_
#define _FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_

struct CFileTranslatorWideWrap : virtual public CFileTranslator
{
    using CFileTranslator::CreateDir;
    using CFileTranslator::Open;
    using CFileTranslator::Exists;
    using CFileTranslator::Delete;
    using CFileTranslator::Copy;
    using CFileTranslator::Rename;
    using CFileTranslator::Size;
    using CFileTranslator::Stat;

    using CFileTranslator::ScanDirectory;
    using CFileTranslator::GetDirectories;
    using CFileTranslator::GetFiles;

    // We implement all unicode methods here and redirect them to ANSI methods.
    bool            CreateDir( const wchar_t *path ) override;
    CFile*          Open( const wchar_t *path, const wchar_t *mode ) override;
    bool            Exists( const wchar_t *path ) const override;
    bool            Delete( const wchar_t *path ) override;
    bool            Copy( const wchar_t *src, const wchar_t *dst ) override;
    bool            Rename( const wchar_t *src, const wchar_t *dst ) override;
    size_t          Size( const wchar_t *path ) const override;
    bool            Stat( const wchar_t *path, struct stat *stats ) const override;
    
    void            ScanDirectory( const wchar_t *directory, const wchar_t *wildcard, bool recurse,
                                   pathCallback_t dirCallback,
                                   pathCallback_t fileCallback,
                                   void *userdata ) const override;
    void            GetDirectories( const wchar_t *directory, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const override;
    void            GetFiles( const wchar_t *directory, const wchar_t *wildcard, bool recurse, std::vector <filePath>& output ) const override;
};

#endif //_FILESYSTEM_TRANSLATOR_UNICODE_WRAPPER_