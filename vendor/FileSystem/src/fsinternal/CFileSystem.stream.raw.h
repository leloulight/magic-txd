/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.stream.raw.h
*  PURPOSE:     Raw OS filesystem file link
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_RAW_OS_LINK_
#define _FILESYSTEM_RAW_OS_LINK_

class CRawFile : public CFile
{
public:
                        CRawFile        ( const filePath& absFilePath );
                        ~CRawFile       ( void );

    size_t              Read            ( void *buffer, size_t sElement, size_t iNumElements ) override;
    size_t              Write           ( const void *buffer, size_t sElement, size_t iNumElements ) override;
    int                 Seek            ( long iOffset, int iType ) override;
    int                 SeekNative      ( fsOffsetNumber_t iOffset, int iType ) override;
    long                Tell            ( void ) const override;
    fsOffsetNumber_t    TellNative      ( void ) const override;
    bool                IsEOF           ( void ) const override;
    bool                Stat            ( struct stat *stats ) const override;
    void                PushStat        ( const struct stat *stats ) override;
    void                SetSeekEnd      ( void ) override;
    size_t              GetSize         ( void ) const override;
    fsOffsetNumber_t    GetSizeNative   ( void ) const override;
    void                Flush           ( void ) override;
    const filePath&     GetPath         ( void ) const override;
    bool                IsReadable      ( void ) const override;
    bool                IsWriteable     ( void ) const override;

private:
    friend class CSystemFileTranslator;
    friend class CFileSystem;

#ifdef _WIN32
    HANDLE          m_file;
#elif defined(__linux__)
    FILE*           m_file;
#endif //OS DEPENDANT CODE
    DWORD           m_access;
    filePath        m_path;
};

#endif //_FILESYSTEM_RAW_OS_LINK_