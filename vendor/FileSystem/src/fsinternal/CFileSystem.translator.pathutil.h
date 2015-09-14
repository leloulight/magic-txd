/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.pathutil.h
*  PURPOSE:     FileSystem path utilities for all kinds of translators
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_PATHUTIL_
#define _FILESYSTEM_TRANSLATOR_PATHUTIL_

class CSystemPathTranslator : public virtual CFileTranslator
{
public:
                    CSystemPathTranslator           ( bool isSystemPath );

    bool            GetFullPathTreeFromRoot         ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetFullPathTree                 ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetRelativePathTreeFromRoot     ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetRelativePathTree             ( const char *path, dirTree& tree, bool& file ) const;
    bool            GetFullPathFromRoot             ( const char *path, bool allowFile, filePath& output ) const;
    bool            GetFullPath                     ( const char *path, bool allowFile, filePath& output ) const;
    bool            GetRelativePathFromRoot         ( const char *path, bool allowFile, filePath& output ) const;
    bool            GetRelativePath                 ( const char *path, bool allowFile, filePath& output ) const;
    bool            ChangeDirectory                 ( const char *path );
    void            GetDirectory                    ( filePath& output ) const;

    inline bool IsTranslatorRootDescriptor( char character ) const
    {
        if ( !m_isSystemPath )
        {
            // On Windows platforms, absolute paths are based on drive letters (C:/).
            // This means we can use the linux way of absolute fs root linkling for the translators.
            // But this may confuse Linux users; on Linux this convention cannot work (reserved for abs paths).
            // Hence, usage of the '/' or '\\' descriptor is discouraged (only for backwards compatibility).
            // Disregard this thought for archive file systems, all file systems which are the root themselves.
            // They use the linux addressing convention.
            if ( character == '/' || character == '\\' )
                return true;
        }

        // Just like MTA:SA is using the '' character for private directories, we use it
        // for a platform independent translator root basing.
        // 'textfile.txt' will address 'textfile.txt' in the translator root, ignoring
        // the current directory setting.
        return character == '' || character == '@';
    }

protected:
    friend class CFileSystem;

    filePath        m_root;
    filePath        m_currentDir;
    dirTree         m_rootTree;
    dirTree         m_curDirTree;

private:
    bool            m_isSystemPath;
};

#endif //_FILESYSTEM_TRANSLATOR_PATHUTIL_