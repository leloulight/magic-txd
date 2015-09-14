/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.zip.internal.h
*  PURPOSE:     Master header of ZIP archive filesystem private modules
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_ZIP_PRIVATE_MODULES_
#define _FILESYSTEM_ZIP_PRIVATE_MODULES_

// ZIP extension struct.
struct zipExtension
{
    static fileSystemFactory_t::pluginOffset_t _zipPluginOffset;

    static zipExtension*        Get( CFileSystem *sys )
    {
        if ( _zipPluginOffset != fileSystemFactory_t::INVALID_PLUGIN_OFFSET )
        {
            return fileSystemFactory_t::RESOLVE_STRUCT <zipExtension> ( (CFileSystemNative*)sys, _zipPluginOffset );
        }
        return NULL;
    }

    void                        Initialize      ( CFileSystemNative *sys );
    void                        Shutdown        ( CFileSystemNative *sys );

    CArchiveTranslator*         NewArchive      ( CFile& writeStream );
    CArchiveTranslator*         OpenArchive     ( CFile& readWriteStream );

    // Private extension methods.
    CFileTranslator*            GetTempRoot     ( void );

    // Extension members.
    // ... for managing temporary files (OS dependent).
    // See zipExtension::Init().
    CRepository                 repo;
};

// Checksums.
#define ZIP_SIGNATURE               0x06054B50
#define ZIP_FILE_SIGNATURE          0x02014B50
#define ZIP_LOCAL_FILE_SIGNATURE    0x04034B50

#include <time.h>

#include "CFileSystem.vfs.h"

#pragma warning(push)
#pragma warning(disable:4250)

class CZIPArchiveTranslator : public CSystemPathTranslator, public CArchiveTranslator
{
    friend struct zipExtension;
    friend class CArchiveFile;
public:
                    CZIPArchiveTranslator( zipExtension& theExtension, CFile& file );
                    ~CZIPArchiveTranslator( void );

    bool            WriteData( const char *path, const char *buffer, size_t size );
    bool            CreateDir( const char *path );
    CFile*          Open( const char *path, const char *mode );
    bool            Exists( const char *path ) const;
    bool            Delete( const char *path );
    bool            Copy( const char *src, const char *dst );
    bool            Rename( const char *src, const char *dst );
    size_t          Size( const char *path ) const;
    bool            Stat( const char *path, struct stat *stats ) const;
    bool            ReadToBuffer( const char *path, std::vector <char>& output ) const;

    bool            ChangeDirectory( const char *path );

    void            ScanDirectory( const char *directory, const char *wildcard, bool recurse,
                        pathCallback_t dirCallback,
                        pathCallback_t fileCallback,
                        void *userdata ) const;

    void            GetDirectories( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const;
    void            GetFiles( const char *path, const char *wildcard, bool recurse, std::vector <filePath>& output ) const;

    void            Save();

    // Members.
    zipExtension&   m_zipExtension;
    CFile&          m_file;

#pragma pack(1)
    struct _localHeader
    {
        fsUInt_t        signature;
        fsUShort_t      version;
        fsUShort_t      flags;
        fsUShort_t      compression;
        fsUShort_t      modTime;
        fsUShort_t      modDate;
        fsUInt_t        crc32val;

        fsUInt_t        sizeCompressed;
        fsUInt_t        sizeReal;

        fsUShort_t      nameLen;
        fsUShort_t      commentLen;
    };

    struct _centralFile
    {
        fsUInt_t        signature;
        fsUShort_t      version;
        fsUShort_t      reqVersion;
        fsUShort_t      flags;
        fsUShort_t      compression;
        fsUShort_t      modTime;
        fsUShort_t      modDate;
        fsUInt_t        crc32val;

        fsUInt_t        sizeCompressed;
        fsUInt_t        sizeReal;

        fsUShort_t      nameLen;
        fsUShort_t      extraLen;
        fsUShort_t      commentLen;

        fsUShort_t      diskID;
        fsUShort_t      internalAttr;
        fsUInt_t        externalAttr;
        fsUInt_t        localHeaderOffset;
    };
#pragma pack()

private:
    void            ReadFiles( unsigned int count );    // throws an exception on failure.

public:
    struct fileMetaData;
    struct directoryMetaData;

public:
    struct genericMetaData abstract
    {
        VirtualFileSystem::fsObjectInterface *genericInterface;

        inline void SetInterface( VirtualFileSystem::fsObjectInterface *intf )
        {
            this->genericInterface = intf;
        }

        unsigned short  version;
        unsigned short  reqVersion;
        unsigned short  flags;

        unsigned short  diskID;

        unsigned short  modTime;
        unsigned short  modDate;

        size_t          localHeaderOffset;

        std::string     extra;
        std::string     comment;

        inline _localHeader ConstructLocalHeader( void ) const
        {
            _localHeader header;
            header.signature = ZIP_LOCAL_FILE_SIGNATURE;
            header.modTime = modTime;
            header.modDate = modDate;
            header.nameLen = (fsUShort_t)this->genericInterface->GetRelativePath().size();
            header.commentLen = (fsUShort_t)comment.size();
            return header;
        }

        inline _centralFile ConstructCentralHeader( void ) const
        {
            _centralFile header;
            header.signature = ZIP_FILE_SIGNATURE;
            header.version = version;
            header.reqVersion = reqVersion;
            header.flags = flags;
            //header.compression
            header.modTime = modTime;
            header.modDate = modDate;
            //header.crc32
            //header.sizeCompressed
            //header.sizeReal
            header.nameLen = (fsUShort_t)this->genericInterface->GetRelativePath().size();
            header.extraLen = (fsUShort_t)extra.size();
            header.commentLen = (fsUShort_t)comment.size();
            header.diskID = diskID;
            //header.internalAttr
            //header.externalAttr
            header.localHeaderOffset = (fsUInt_t)localHeaderOffset;
            return header;
        }

        inline void AllocateArchiveSpace( CZIPArchiveTranslator *archive, _localHeader& entryHeader, size_t& sizeCount )
        {
            // Update the offset.
            localHeaderOffset = archive->m_file.Tell() - archive->m_structOffset;

            sizeCount += sizeof( entryHeader ) + entryHeader.nameLen + entryHeader.commentLen;
        }

        inline void Reset( void )
        {
#ifdef _WIN32
            version = 10;
#else
            version = 0x03; // Unix
#endif //_WIN32
            reqVersion = 0x14;
            flags = 0;

            UpdateTime();

            diskID = 0;

            extra.clear();
            comment.clear();
        }

        inline void SetModTime( const tm& date )
        {
            unsigned short year = date.tm_year;

            if ( date.tm_year > 1980 )
                year -= 1980;
            else if ( date.tm_year > 80 )
                year -= 80;

            modDate = date.tm_mday | ((date.tm_mon + 1) << 5) | (year << 9);
            modTime = date.tm_sec >> 1 | (date.tm_min << 5) | (date.tm_hour << 11);
        }

        inline void GetModTime( tm& date ) const
        {
            date.tm_mday = modDate & 0x1F;
            date.tm_mon = ((modDate & 0x1E0) >> 5) - 1;
            date.tm_year = ((modDate & 0x0FE00) >> 9) + 1980;

            date.tm_hour = (modTime & 0xF800) >> 11;
            date.tm_min = (modTime & 0x7E0) >> 5;
            date.tm_sec = (modTime & 0x1F) << 1;

            date.tm_wday = 0;
            date.tm_yday = 0;
        }

        virtual void UpdateTime( void )
        {
            time_t curTime = time( NULL );
            SetModTime( *gmtime( &curTime ) );
        }
    };

    class stream;

    struct fileMetaData : public genericMetaData
    {
        unsigned short  compression;

        unsigned int    crc32val;
        size_t          sizeCompressed;
        size_t          sizeReal;

        unsigned short  internalAttr;
        unsigned int    externalAttr;

        bool            archived;
        bool            cached;
        bool            subParsed;

        typedef std::list <stream*> lockList_t;
        lockList_t locks;

        directoryMetaData*  dir;

        CZIPArchiveTranslator *translator;

        inline void Reset( void )
        {
            // Reset generic data.
            genericMetaData::Reset();

            // File-specific reset.
            compression = 8;

            crc32val = 0;
            sizeCompressed = 0;
            sizeReal = 0;
            internalAttr = 0;
            externalAttr = 0;

            archived = false;
            cached = false;
            subParsed = false;
        }

        inline bool IsLocked( void ) const
        {
            return ( locks.empty() == false );
        }

        inline void OnSetDirectory( directoryMetaData *data )
        {
            this->dir = data;
        }

        inline bool IsNative( void ) const
        {
#ifdef _WIN32
            return version == 10;
#else
            return version == 0x03; // Unix
#endif //_WIN32
        }

        inline void UpdateTime( void )
        {
            genericMetaData::UpdateTime();

            // Update the time of its parent directories.
            dir->UpdateTime();
        }

        // Virtual node methods.
        inline fsOffsetNumber_t GetSize( void ) const
        {
            fsOffsetNumber_t resourceSize = 0;

            if ( !this->cached )
            {
                resourceSize = this->sizeReal;
            }
            else
            {
                const filePath& ourPath = this->genericInterface->GetRelativePath();

                CFileTranslator *realtimeRoot = translator->GetRealtimeRoot();

                if ( realtimeRoot )
                {
                    resourceSize = realtimeRoot->Size( ourPath.c_str() );
                }
            }

            return resourceSize;
        }

        bool OnFileCopy( const dirTree& tree, const filePath& newName ) const;
        bool OnFileRename( const dirTree& tree, const filePath& newName );
        void OnFileDelete( void );

        inline void GetANSITimes( time_t& mtime, time_t& ctime, time_t& atime ) const
        {
            tm date;
            GetModTime( date );

            date.tm_year -= 1900;

            mtime = ctime = atime = mktime( &date );
        }

        inline void GetDeviceIdentifier( dev_t& deviceIdx ) const
        {
            deviceIdx = (dev_t)this->diskID;
        }

        inline void CopyAttributesTo( fileMetaData& dstEntry )
        {
            // Copy over general attributes
            dstEntry.flags          = flags;
            dstEntry.compression    = compression;
            dstEntry.modTime        = modTime;
            dstEntry.modDate        = modDate;
            dstEntry.diskID         = diskID;
            dstEntry.internalAttr   = internalAttr;
            dstEntry.externalAttr   = externalAttr;
            dstEntry.extra          = extra;
            dstEntry.comment        = comment;
            dstEntry.cached         = cached;
            dstEntry.subParsed      = subParsed;

            if ( !cached )
            {
                dstEntry.version            = version;
                dstEntry.reqVersion         = reqVersion;
                dstEntry.crc32val           = crc32val;
                dstEntry.sizeCompressed     = sizeCompressed;
                dstEntry.sizeReal           = sizeReal;
            }
        }
    };

private:
    inline void seekFile( const fileMetaData& info, _localHeader& header );

public:
    struct directoryMetaData : public genericMetaData
    {
        VirtualFileSystem::directoryInterface *dirInterface;

        inline void SetInterface( VirtualFileSystem::directoryInterface *intf )
        {
            dirInterface = intf;
        }
    
        inline bool     NeedsWriting( void ) const
        {
            return ( this->dirInterface->IsEmpty() || comment.size() != 0 || extra.size() != 0 );
        }

        inline void     UpdateTime( void )
        {
            genericMetaData::UpdateTime();

            // Update the time of its parent directories.
            if ( VirtualFileSystem::directoryInterface *parent = dirInterface->GetParent() )
            {
                ((directory*)parent)->metaData.UpdateTime();
            }
        }
    };

    // Tree of all filesystem objects.
    typedef CVirtualFileSystem <CZIPArchiveTranslator, directoryMetaData, fileMetaData> vfs_t;

    vfs_t m_virtualFS;

    typedef vfs_t::fsActiveEntry fsActiveEntry;
    typedef vfs_t::file file;
    typedef vfs_t::directory directory;
    typedef vfs_t::fileList fileList;

    // Initializators for meta-data.
    inline void InitializeFileMeta( fileMetaData& meta )
    {
        meta.translator = this;
    }

    inline void ShutdownFileMeta( fileMetaData& meta )
    {
        meta.translator = NULL;
    }

    inline void InitializeDirectoryMeta( directoryMetaData& meta )
    {

    }

    inline void ShutdownDirectoryMeta( directoryMetaData& meta )
    {

    }

    // Special functions for the VFS.
    CFile* OpenNativeFileStream( file *fsObject, unsigned int openMode, unsigned int access );

    // Implement the stream now.
    class stream abstract : public CFile
    {
        friend class CZIPArchiveTranslator;
    public:
        stream( CZIPArchiveTranslator& zip, file& info, CFile& sysFile ) : m_sysFile( sysFile ), m_archive( zip ), m_info( info )
        {
            info.metaData.locks.push_back( this );
        }

        ~stream( void )
        {
            m_info.metaData.locks.remove( this );

            delete &m_sysFile;
        }

        const filePath& GetPath( void ) const
        {
            return m_path;
        }

    private:
        CFile&                      m_sysFile;
        CZIPArchiveTranslator&      m_archive;
        file&                       m_info;
        filePath                    m_path;
    };

    // We need to cache data on the disk
    void            Extract( CFile& dstFile, vfs_t::file& info );

    // Stream that decompresses things using deflate.
    class fileDeflate : public stream
    {
        friend class CZIPArchiveTranslator;
    public:
                        fileDeflate( CZIPArchiveTranslator& zip, file& info, CFile& sysFile );
                        ~fileDeflate( void );

        size_t          Read( void *buffer, size_t sElement, size_t iNumElements ) override;
        size_t          Write( const void *buffer, size_t sElement, size_t iNumElements ) override;
        int             Seek( long iOffset, int iType ) override;
        long            Tell( void ) const override;
        bool            IsEOF( void ) const override;
        bool            Stat( struct stat *stats ) const override;
        void            PushStat( const struct stat *stats ) override;
        void            SetSeekEnd( void ) override;
        size_t          GetSize( void ) const override;
        void            Flush( void ) override;
        bool            IsReadable( void ) const override;
        bool            IsWriteable( void ) const override;

    private:
        inline void     Focus( void );

        bool            m_writeable;
        bool            m_readable;
    };

private:
    void            CacheDirectory( const directory& dir );
    void            SaveDirectory( directory& dir, size_t& size );
    unsigned int    BuildCentralFileHeaders( const directory& dir, size_t& size );

    struct extraData
    {
        fsUInt_t        signature;
        fsUInt_t        size;

        //data
    };

    std::string m_comment;

    CFileTranslator*    GetFileRoot         ( void );
    CFileTranslator*    GetUnpackRoot       ( void );
    CFileTranslator*    GetRealtimeRoot     ( void );

    // Runtime management file directories.
    // They are acquired once they are required.
    CFileTranslator*    m_fileRoot;
    CFileTranslator*    m_unpackRoot;
    CFileTranslator*    m_realtimeRoot;

    size_t      m_structOffset;
};

#pragma warning(pop)

#endif //_FILESYSTEM_ZIP_PRIVATE_MODULES_