/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.vfs.h
*  PURPOSE:     Virtual filesystem translator for filesystem emulation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_VFS_IMPLEMENTATION_
#define _FILESYSTEM_VFS_IMPLEMENTATION_

#include "CFileSystem.utils.hxx"

#include <map>

// This class implements a virtual file system.
// It consists of nodes which are either directories or files.
// Each, either directory or file, has its own set of meta data.

#pragma warning(push)
#pragma warning(disable: 4250)

namespace VirtualFileSystem
{
    struct fsObjectInterface abstract
    {
        virtual const filePath& GetRelativePath( void ) const = 0;

        virtual fsOffsetNumber_t GetDataSize( void ) const = 0;
    };

    struct fileInterface abstract : public virtual fsObjectInterface
    {
        //todo.
    };

    struct directoryInterface abstract : public virtual fsObjectInterface
    {
        virtual bool IsEmpty( void ) const = 0;

        virtual directoryInterface* GetParent( void ) = 0;
    };
};

template <typename translatorType, typename directoryMetaData, typename fileMetaData>
struct CVirtualFileSystem
{
    CVirtualFileSystem( void ) : m_rootDir( filePath(), filePath() )
    {
        // We zero it our for debugging.
        this->hostTranslator = NULL;

        // Current directory starts at root
        m_curDirEntry = &m_rootDir;
        m_rootDir.parent = NULL;
        
        m_rootDir.hostVFS = this;
    }

    ~CVirtualFileSystem( void )
    {
        return;
    }

    // Host translator of this VFS.
    // Needs to be set by the translator that uses this VFS.
    translatorType *hostTranslator;

    // Forward declarations.
    struct file;
    struct directory;

    struct fsActiveEntry : virtual public VirtualFileSystem::fsObjectInterface
    {
        fsActiveEntry( const filePath& name, const filePath& relPath ) : name( name ), relPath( relPath )
        {
            this->isFile = false;
            this->isDirectory = false;
        }

        const filePath& GetRelativePath( void ) const
        {
            return relPath;
        }

        fsOffsetNumber_t GetDataSize( void ) const
        {
            return 0;
        }

        filePath        name;
        filePath        relPath;

        bool isDirectory, isFile;
    };

    struct file : public fsActiveEntry, public VirtualFileSystem::fileInterface
    {
        file( const filePath& name ) : fsActiveEntry( name, filePath() )
        {
            metaData.SetInterface( this );

            this->isFile = true;
        }

        inline void OnRecreation( void )
        {
            this->metaData.Reset();
        }

        fsOffsetNumber_t GetDataSize( void ) const
        {
            return this->metaData.GetSize();
        }

        directory*      dir;

        fileMetaData    metaData;
    };

    typedef std::list <file*> fileList;

    struct directory : public fsActiveEntry, public VirtualFileSystem::directoryInterface
    {
        inline directory( filePath fileName, filePath path ) : fsActiveEntry( fileName, path )
        {
            metaData.SetInterface( this );

            this->isDirectory = true;

            this->hostVFS = NULL;
        }

        inline void deleteDirectory( directory *theDir )
        {
            this->hostVFS->hostTranslator->ShutdownDirectoryMeta( theDir->metaData );

            delete theDir;
        }

        inline void deleteFile( file *theFile )
        {
            this->hostVFS->hostTranslator->ShutdownFileMeta( theFile->metaData );

            delete theFile;
        }

        inline ~directory( void )
        {
            subDirs::iterator iter = children.begin();

            for ( ; iter != children.end(); ++iter )
            {
                deleteDirectory( *iter );
            }

            fileList::iterator fileIter = files.begin();

            for ( ; fileIter != files.end(); ++fileIter )
            {
                deleteFile( *fileIter );
            }
        }

        fsOffsetNumber_t GetDataSize( void ) const
        {
            fsOffsetNumber_t theDataSize = 0;

            for ( fileList::const_iterator iter = files.begin(); iter != files.end(); iter++ )
            {
                file *theFile = *iter;

                theDataSize += theFile->GetDataSize();
            }

            for ( subDirs::const_iterator iter = children.begin(); iter != children.end(); iter++ )
            {
                directory *theDir = *iter;

                theDataSize += theDir->GetDataSize();
            }

            return theDataSize;
        }

        typedef std::map <std::string, file*> fileNameMap_t;
        typedef std::map <std::string, directory*> childrenMap_t;

        fileNameMap_t fileNameMap;
        childrenMap_t childrenMap;

        typedef std::list <directory*> subDirs;

        fileList files;
        subDirs children;

        directory* parent;

        directoryMetaData metaData;

        CVirtualFileSystem *hostVFS;

        inline directory*  FindDirectory( const filePath& dirName ) const
        {
            childrenMap_t::const_iterator iter = childrenMap.find( dirName );

            if ( iter == childrenMap.end() )
                return NULL;

            return iter->second;
        }

        inline directory&  GetDirectory( const filePath& dirName )
        {
            directory *dir = FindDirectory( dirName );

            if ( dir )
                return *dir;

            dir = new directory( dirName, relPath + dirName + filePath( "/" ) );
            dir->name = dirName;
            dir->parent = this;
            dir->metaData.Reset();

            dir->hostVFS = this->hostVFS;

            this->hostVFS->hostTranslator->InitializeDirectoryMeta( dir->metaData );

            children.push_back( dir );

            childrenMap.insert( std::pair <std::string, directory*> ( dirName.convert_ansi(), dir ) );

            return *dir;
        }

        inline bool     RemoveDirectory( directory *dirObject )
        {
            if ( dirObject->IsLocked() )
                return false;

            deleteDirectory( dirObject );

            UnlinkDirectory( *dirObject );

            return true;
        }

        inline void     PositionObject( fsActiveEntry& entry )
        {
            entry.relPath = relPath;
            entry.relPath += entry.name;

            if ( entry.isDirectory )
            {
                entry.relPath += "/";
            }
        }

        inline file&    AddFile( const filePath& fileName )
        {
            file& entry = *new file( fileName );

            PositionObject( entry );

            entry.dir = this;

            this->hostVFS->hostTranslator->InitializeFileMeta( entry.metaData );

            files.push_back( &entry );

            fileNameMap.insert( std::make_pair <std::string, file*> ( fileName, &entry ) );

            return entry;
        }

        inline void     UnlinkFile( file& entry )
        {
            files.remove( &entry );

            for ( fileNameMap_t::const_iterator iter = fileNameMap.begin(); iter != fileNameMap.end(); iter++ )
            {
                if ( iter->second == &entry )
                {
                    fileNameMap.erase( iter );
                    break;
                }
            }
        }

        inline void     UnlinkDirectory( directory& entry )
        {
            children.remove( &entry );

            for ( childrenMap_t::const_iterator iter = childrenMap.begin(); iter != childrenMap.end(); iter++ )
            {
                if ( iter->second == &entry )
                {
                    childrenMap.erase( iter );
                }
            }
        }

        inline bool     MoveTo( fsActiveEntry& entry )
        {
            // Make sure a file or directory named like this does not exist in this directory already!
            // You have to do it.
            if ( entry.isFile )
            {
                file *fileEntry = (file*)&entry;

                fileEntry->dir->UnlinkFile( *fileEntry );

                fileEntry->dir = this;

                files.push_back( fileEntry );
            }
            else if ( entry.isDirectory )
            {
                directory *theDir = (directory*)&entry;

                theDir->parent->UnlinkDirectory( *theDir );

                theDir->parent = this;

                children.push_back( theDir );
            }
            else
            {
                return false;
            }

            PositionObject( entry );
            return true;
        }

        inline bool     IsLocked( void ) const
        {
            // Check all files whether they are locked.
            {
                fileList::const_iterator iter = files.begin();

                for ( ; iter != files.end(); ++iter )
                {
                    file *theFile = *iter;

                    if ( theFile->metaData.IsLocked() )
                    {
                        return true;
                    }
                }
            }

            // Check all children directories, too.
            {
                subDirs::const_iterator iter = children.begin();

                for ( ; iter != children.end(); ++iter )
                {
                    directory *theDir = *iter;

                    if ( theDir->IsLocked() )
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        inline file*    GetFile( const filePath& fileName )
        {
            fileNameMap_t::const_iterator iter = fileNameMap.find( fileName );

            if ( iter == fileNameMap.end() )
                return NULL;

            return iter->second;
        }

        inline file*    MakeFile( const filePath& fileName )
        {
            file *entry = GetFile( fileName );

            if ( entry )
            {
                if ( entry->metaData.IsLocked() )
                    return NULL;

                entry->name = fileName;
                entry->OnRecreation();
                return entry;
            }

            return &AddFile( fileName );
        }

        inline const file*  GetFile( const filePath& fileName ) const
        {
            fileList::const_iterator iter = files.begin();

            for ( ; iter != files.end(); ++iter )
            {
                if ( (*iter)->name == fileName )
                    return *iter;
            }

            return NULL;
        }

        inline bool     RemoveFile( file& entry )
        {
            if ( entry.metaData.IsLocked() )
                return false;

            entry.metaData.OnFileDelete();

            deleteFile( &entry );

            UnlinkFile( entry );
            return true;
        }

        inline bool     RemoveFile( const filePath& fileName )
        {
            fileList::const_iterator iter = files.begin();

            for ( ; iter != files.end(); ++iter )
            {
                file *theFile = *iter;

                if ( theFile->name == fileName )
                {
                    return RemoveFile( *theFile );
                }
            }
            return false;
        }

        inline bool     IsEmpty( void ) const
        {
            return ( files.empty() && children.empty() );
        }

        inline VirtualFileSystem::directoryInterface* GetParent( void )
        {
            return this->parent;
        }

        inline bool InheritsFrom( const directory *theDir ) const
        {
            directory *parent = this->parent;

            while ( parent )
            {
                if ( parent == theDir )
                    return true;

                parent = parent->parent;
            }

            return false;
        }
    };

    // Root node of the tree.
    directory m_rootDir;

    inline directory&   GetRootDir( void )
    {
        return m_rootDir;
    }

    // Current virtual directory.
    directory*  m_curDirEntry;

    inline directory*   GetCurrentDir( void )
    {
        return m_curDirEntry;
    }

    // Helper methods for node traversal on a virtual file system.
    inline const fsActiveEntry* GetObjectNode( const char *path ) const
    {
        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTree( path, tree, isFile ) )
            return NULL;

        filePath fileName;
        
        if ( isFile )
        {
            fileName = tree.back();
            tree.pop_back();
        }

        const directory *dir = GetDeriviateDir( *m_curDirEntry, tree );

        const fsActiveEntry *theEntry = NULL;

        if ( isFile )
        {
            if ( dir )
            {
                theEntry = dir->GetFile( fileName );
            }
        }
        else
        {
            theEntry = dir;
        }

        return theEntry;
    }

    inline const directory* GetDirTree( const directory& root, dirTree::const_iterator iter, dirTree::const_iterator end ) const
    {
        const directory *curDir = &root;

        for ( ; iter != end; ++iter )
        {
            if ( !( curDir = root.FindDirectory( *iter ) ) )
                return NULL;
        }

        return curDir;
    }

    inline const directory* GetDirTree( const dirTree& tree ) const
    {
        return GetDirTree( this->m_rootDir, tree.begin(), tree.end() );
    }

    // For getting a directory.
    inline const directory* GetDeriviateDir( const directory& root, const dirTree& tree ) const
    {
        const directory *curDir = &root;
        dirTree::const_iterator iter = tree.begin();

        for ( ; iter != tree.end() && ( *iter == _dirBack ); ++iter )
        {
            curDir = curDir->parent;

            if ( !curDir )
                return NULL;
        }

        return GetDirTree( *curDir, iter, tree.end() );
    }

    // Common-purpose directory creation function.
    inline directory* MakeDeriviateDir( directory& root, const dirTree& tree )
    {
        directory *curDir = &root;
        dirTree::const_iterator iter = tree.begin();

        for ( ; iter != tree.end() && ( *iter == _dirBack ); ++iter )
        {
            curDir = curDir->parent;

            if ( !curDir )
                return NULL;
        }

        for ( ; iter != tree.end(); ++iter )
        {
            const filePath& thePathToken = *iter;

            curDir = &curDir->GetDirectory( thePathToken );
        }

        return curDir;
    }

    // USE THIS FUNCTION ONLY IF YOU KNOW THAT tree IS AN ABSOLUTE PATH!
    inline directory& _CreateDirTree( directory& dir, const dirTree& tree )
    {
        directory *curDir = &dir;
        dirTree::const_iterator iter = tree.begin();

        for ( ; iter != tree.end(); ++iter )
        {
            const filePath& thePath = *iter;

            curDir = &curDir->GetDirectory( thePath );
        }

        return *curDir;
    }

    // Making your life easier!
    inline file* ConstructFile( const dirTree& tree, const filePath& fileName )    // fileName is just the name of the file itself, not a path.
    {
        directory *targetDir = MakeDeriviateDir( *m_curDirEntry, tree );

        if ( targetDir )
        {
            return targetDir->MakeFile( fileName );
        }
        return NULL;
    }

    inline file* BrowseFile( const dirTree& tree, const filePath& fileName )
    {
        directory *dir = (directory*)GetDeriviateDir( *m_curDirEntry, tree );

        if ( dir )
        {
            return dir->GetFile( fileName );
        }
        
        return NULL;
    }

    inline bool RenameObject( const filePath& newName, const dirTree& tree, fsActiveEntry *object )
    {
        // Get the directory where to put the object.
        directory *targetDir = this->MakeDeriviateDir( *m_curDirEntry, tree );

        bool renameSuccess = false;

        if ( targetDir )
        {
            // Directory or file?
            if ( object->isFile )
            {
                file *fileEntry = (file*)object;

                if ( !fileEntry->metaData.IsLocked() )
                {
                    // Make sure a file with that name does not exist already.
                    if ( targetDir->GetFile( newName ) == NULL )
                    {
                        // First move all kinds of meta data.
                        bool metaMoveSuccess = fileEntry->metaData.OnFileRename( tree, newName );

                        if ( metaMoveSuccess )
                        {
                            // Give it a new name
                            fileEntry->name = newName;

                            targetDir->MoveTo( *fileEntry );

                            renameSuccess = true;
                        }
                    }
                }
            }
            else if ( object->isDirectory )
            {
                directory *dirEntry = (directory*)object;

                if ( !dirEntry->IsLocked() )
                {
                    // Make sure we do not uproot ourselves.
                    if ( !targetDir->InheritsFrom( dirEntry ) )
                    {
                        // Make sure a directory with that name does not exist already.
                        if ( targetDir->FindDirectory( newName ) == NULL )
                        {
                            // Give it a new name
                            dirEntry->name = newName;

                            targetDir->MoveTo( *dirEntry );

                            renameSuccess = true;
                        }
                    }
                }
            }
        }

        return renameSuccess;
    }

    inline bool CopyObject( const filePath& newName, const dirTree& tree, fsActiveEntry *object )
    {
        bool copySuccess = false;

        if ( object->isFile )
        {
            file *fileObject = (file*)object;

            // Attempt to copy the data to the new location.
            bool metaDataCopySuccess = fileObject->metaData.OnFileCopy( tree, newName );

            if ( metaDataCopySuccess )
            {
                file *dstEntry = ConstructFile( tree, newName );

                if ( dstEntry )
                {
                    // Copy parameters.
                    fileObject->metaData.CopyAttributesTo( dstEntry->metaData );

                    copySuccess = true;

                    // Delete if a failure happens (maybe useful in the future)
                    if ( !copySuccess )
                    {
                        bool deleteOnFailure = DeleteObject( dstEntry );

                        assert( deleteOnFailure == true );
                    }
                }

                // Handle the case when the new file could not be allocated.
                if ( !copySuccess )
                {
                    // TODO.
                }
            }
        }
        else if ( object->isDirectory )
        {
            // TODO
        }

        return copySuccess;
    }

    inline bool DeleteObject( fsActiveEntry *object )
    {
        bool deletionSuccess = false;

        if ( object->isFile )
        {
            file *fileObject = (file*)object;

            deletionSuccess = fileObject->dir->RemoveFile( *fileObject );
        }
        else if ( object->isDirectory )
        {
            directory *dirObject = (directory*)object;

            // We cannot remove the root directory!
            if ( dirObject->parent != NULL )
            {
                deletionSuccess = dirObject->parent->RemoveDirectory( dirObject );
            }
        }

        return deletionSuccess;
    }

    inline void StatObject( const fsActiveEntry *entry, struct stat *stats ) const
    {
        int fileMode = 0;

        if ( entry->isDirectory )
        {
            fileMode |= S_IFDIR;
        }

        fileMode |= ( S_IREAD | S_IWRITE );

        time_t mtime = 0;
        time_t ctime = 0;
        time_t atime = 0;
        dev_t diskID = 0;

        if ( entry->isFile )
        {
            file *fileEntry = (file*)entry;

            fileEntry->metaData.GetANSITimes( mtime, ctime, atime );
            fileEntry->metaData.GetDeviceIdentifier( diskID );
        }
        stats->st_atime = atime;
        stats->st_ctime = ctime;
        stats->st_mtime = mtime;
        stats->st_dev = diskID;
        stats->st_rdev = 0;
        stats->st_gid = 0;
        stats->st_ino = 0;
        stats->st_mode = fileMode;
        stats->st_nlink = 1;
        stats->st_uid = 0;
        stats->st_size = (off_t)entry->GetDataSize();
    }

    // Only use this function if you know that there are no file name conflicts!
    inline file* ConstructFileExclusive( const dirTree& tree, const filePath& fileName )
    {
        directory *targetDir = MakeDeriviateDir( *m_curDirEntry, tree );

        if ( targetDir )
        {
            return &targetDir->AddFile( fileName );
        }
        return NULL;
    }

    // Common purpose translator functions.
    inline bool CreateDir( const char *path )
    {
        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTree( path, tree, isFile ) )
            return false;

        if ( isFile )
            tree.pop_back();

        return ( MakeDeriviateDir( *m_curDirEntry, tree ) != NULL );
    }

    inline bool Exists( const char *path ) const
    {
        return ( GetObjectNode( path ) != NULL );
    }

    inline CFile* OpenStream( const char *path, const char *mode )
    {
        // TODO: add directory support; mainly for being able to lock them.
        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTree( path, tree, isFile ) || !isFile )
            return NULL;

        filePath name = tree.back();
        tree.pop_back();

        unsigned int m;
        unsigned int access;

        if ( !_File_ParseMode( *hostTranslator, path, mode, access, m ) )
            return NULL;

        file *entry = NULL;
        bool newFileEntry = false;

        if ( m == FILE_MODE_OPEN )
        {
            entry = BrowseFile( tree, name );
        }
        else if ( m == FILE_MODE_CREATE )
        {
            entry = ConstructFile( tree, name );

            newFileEntry = true;
        }
        else
        {
            // TODO: implement any unknown cases.
            return NULL;
        }

        if ( entry )
        {
            // Files start off clean
            entry->metaData.Reset();
        
            CFile *outputFile = hostTranslator->OpenNativeFileStream( entry, m, access );    

            if ( outputFile )
            {
                if ( m == FILE_MODE_OPEN )
                {
                    // Seek it
                    outputFile->Seek( 0, *mode == 'a' ? SEEK_END : SEEK_SET );
                }
            }
            else
            {
                if ( newFileEntry )
                {
                    // If we failed to create the destination file, and possibly the output stream, we failed to open the file.
                    // Hereby destroy our file entry in the VFS again.
                    DeleteObject( entry );
                }
            }

            return outputFile;
        }
        return NULL;
    }

    inline fsOffsetNumber_t Size( const char *path ) const
    {
        const fsActiveEntry *entry = GetObjectNode( path );

        fsOffsetNumber_t preferableSize = 0;

        if ( entry )
        {
            preferableSize = entry->GetDataSize();
        }

        return preferableSize;
    }

    inline bool Stat( const char *path, struct stat *stats ) const
    {
        const fsActiveEntry *entry = GetObjectNode( path );

        if ( !entry )
            return false;

        StatObject( entry, stats );
        return true;
    }

    inline bool Rename( const char *src, const char *dst )
    {
        fsActiveEntry *fsEntry = (fsActiveEntry*)GetObjectNode( src );

        if ( !fsEntry )
            return false;

        //todo: maybe check whether rename is possible at all, like in the past
        // for the .zip translator.

        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTree( dst, tree, isFile ) )
            return false;

        if ( tree.empty() )
            return false;

        filePath fileName = tree.back();
        tree.pop_back();

        // Move the file entry first.
        bool renameSuccess = RenameObject( fileName, tree, fsEntry );

        return renameSuccess;
    }

    inline bool Copy( const char *src, const char *dst )
    {
        fsActiveEntry *fsSrcEntry = (fsActiveEntry*)GetObjectNode( src );

        if ( !fsSrcEntry )
            return false;

        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTree( dst, tree, isFile ) )
            return false;

        if ( tree.empty() )
            return false;

        // Make sure the given path is the same type as the type of the object.
        if ( fsSrcEntry->isFile != isFile )
            return false;

        filePath fileName = tree.back();
        tree.pop_back();

        return CopyObject( fileName, tree, fsSrcEntry );
    }

    inline bool Delete( const char *path )
    {
        fsActiveEntry *toBeDeleted = (fsActiveEntry*)GetObjectNode( path );

        if ( !toBeDeleted )
            return false;

        return DeleteObject( toBeDeleted );
    }

    inline bool ChangeDirectory( const dirTree& dirTree )
    {
        directory *dir = (directory*)GetDirTree( dirTree );

        if ( !dir )
            return false;

        m_curDirEntry = dir;
        return true;
    }

    static void inline _ScanDirectory(
        const CVirtualFileSystem *trans,
        const dirTree& tree,
        directory *dir,
        filePattern_t *pattern, bool recurse,
        pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata )
    {
        // First scan for files.
        if ( fileCallback )
        {
            for ( fileList::const_iterator iter = dir->files.begin(); iter != dir->files.end(); ++iter )
            {
                file *item = *iter;

                if ( _File_MatchPattern( item->name.c_str(), pattern ) )
                {
                    filePath abs_path( "/" );
                    _File_OutputPathTree( tree, false, abs_path );

                    abs_path += item->name;

                    fileCallback( abs_path, userdata );
                }
            }
        }

        // Continue with the directories
        if ( dirCallback || recurse )
        {
            for ( directory::subDirs::const_iterator iter = dir->children.begin(); iter != dir->children.end(); ++iter )
            {
                directory *item = *iter;

                if ( dirCallback )
                {
                    filePath abs_path( "/" );
                    _File_OutputPathTree( tree, false, abs_path );

                    abs_path += item->name;
                    abs_path += "/";

                    _File_OnDirectoryFound( pattern, item->name, abs_path, dirCallback, userdata );
                }

                if ( recurse )
                {
                    dirTree newTree = tree;
                    newTree.push_back( item->name );

                    _ScanDirectory( trans, newTree, item, pattern, recurse, dirCallback, fileCallback, userdata );
                }
            }
        }
    }

    void ScanDirectory(
        const char *dirPath,
        const char *wildcard,
        bool recurse,
        pathCallback_t dirCallback, pathCallback_t fileCallback, void *userdata ) const
    {
        dirTree tree;
        bool isFile;

        if ( !hostTranslator->GetRelativePathTreeFromRoot( dirPath, tree, isFile ) )
            return;

        if ( isFile )
            tree.pop_back();

        directory *dir = (directory*)GetDirTree( tree );

        if ( !dir )
            return;

        // Create a cached file pattern.
        filePattern_t *pattern = _File_CreatePattern( wildcard );

        try
        {
            _ScanDirectory( this, tree, dir, pattern, recurse, dirCallback, fileCallback, userdata );
        }
        catch( ... )
        {
            // Any exception may be thrown; we have to clean up.
            _File_DestroyPattern( pattern );
            throw;
        }

        _File_DestroyPattern( pattern );
    }
};

#pragma warning(pop)

#endif //_FILESYSTEM_VFS_IMPLEMENTATION_