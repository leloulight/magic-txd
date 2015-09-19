/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.pathutil.cpp
*  PURPOSE:     FileSystem path utilities for all kinds of translators
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/
#include <StdInc.h>

// Include internal definitions.
#include "CFileSystem.internal.h"

// Include common fs utilitites.
#include "../CFileSystem.utils.hxx"

/*=======================================
    CSystemPathTranslator

    Filesystem path translation utility
=======================================*/

CSystemPathTranslator::CSystemPathTranslator( bool isSystemPath )
{
    m_isSystemPath = isSystemPath;
}

void CSystemPathTranslator::GetDirectory( filePath& output ) const
{
    output = m_currentDir;
}

void CSystemPathTranslator::SetCurrentDirectoryTree( dirTree&& tree )
{
    m_curDirTree = tree;

    m_currentDir.clear();
    _File_OutputPathTree( m_curDirTree, false, m_currentDir );
}

template <typename charType>
bool CSystemPathTranslator::GenChangeDirectory( const charType *path )
{
    dirTree tree;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return false;

    if ( file )
        tree.pop_back();

    bool hasConfirmed = OnConfirmDirectoryChange( tree );

    if ( hasConfirmed )
    {
        SetCurrentDirectoryTree( std::move( tree ) );
    }

    return hasConfirmed;
}

bool CSystemPathTranslator::ChangeDirectory( const char *path )         { return GenChangeDirectory( path ); }
bool CSystemPathTranslator::ChangeDirectory( const wchar_t *path )      { return GenChangeDirectory( path ); }

template <typename charType>
bool CSystemPathTranslator::GenGetFullPathTreeFromRoot( const charType *path, dirTree& tree, bool& file ) const
{
    dirTree output;
    tree = m_rootTree;

    if ( !GetRelativePathTreeFromRoot( path, output, file ) )
        return false;

    tree.insert( tree.end(), output.begin(), output.end() );
    return true;
}

bool CSystemPathTranslator::GetFullPathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const    { return GenGetFullPathTreeFromRoot( path, tree, file ); }
bool CSystemPathTranslator::GetFullPathTreeFromRoot( const wchar_t *path, dirTree& tree, bool& file ) const { return GenGetFullPathTreeFromRoot( path, tree, file ); }

template <typename charType>
bool CSystemPathTranslator::GenGetFullPathTree( const charType *path, dirTree& tree, bool& file ) const
{
    dirTree output;
    tree = m_rootTree;

    if ( IsTranslatorRootDescriptor( *path ) )
    {
        if ( !_File_ParseRelativePath( path + 1, output, file ) )
            return false;
    }
    else
    {
        output = m_curDirTree;

        if ( !_File_ParseRelativePath( path, output, file ) )
            return false;
    }

    tree.insert( tree.end(), output.begin(), output.end() );
    return true;
}

bool CSystemPathTranslator::GetFullPathTree( const char *path, dirTree& tree, bool& file ) const    { return GenGetFullPathTree( path, tree, file ); }
bool CSystemPathTranslator::GetFullPathTree( const wchar_t *path, dirTree& tree, bool& file ) const { return GenGetFullPathTree( path, tree, file ); }

template <typename charType>
bool CSystemPathTranslator::GenGetRelativePathTreeFromRoot( const charType *path, dirTree& tree, bool& file ) const
{
    if ( IsTranslatorRootDescriptor( *path ) )
        return _File_ParseRelativePath( path + 1, tree, file );

    tree = m_curDirTree;
    return _File_ParseRelativePath( path, tree, file );
}

bool CSystemPathTranslator::GetRelativePathTreeFromRoot( const char *path, dirTree& tree, bool& file ) const    { return GenGetRelativePathTreeFromRoot( path, tree, file ); }
bool CSystemPathTranslator::GetRelativePathTreeFromRoot( const wchar_t *path, dirTree& tree, bool& file ) const { return GenGetRelativePathTreeFromRoot( path, tree, file ); }

template <typename charType>
bool CSystemPathTranslator::GenGetRelativePathTree( const charType *path, dirTree& tree, bool& file ) const
{
    if ( IsTranslatorRootDescriptor( *path ) )
    {
        dirTree relTree;

        if ( !_File_ParseRelativePath( path + 1, relTree, file ) )
            return false;

        return _File_ParseDeriviateTreeRoot( relTree, m_curDirTree, tree, file );
    }

    return _File_ParseDeriviateTree( path, m_curDirTree, tree, file );
}

bool CSystemPathTranslator::GetRelativePathTree( const char *path, dirTree& tree, bool& file ) const    { return GenGetRelativePathTree( path, tree, file ); }
bool CSystemPathTranslator::GetRelativePathTree( const wchar_t *path, dirTree& tree, bool& file ) const { return GenGetRelativePathTree( path, tree, file ); }

template <typename charType>
bool CSystemPathTranslator::GenGetFullPathFromRoot( const charType *path, bool allowFile, filePath& output ) const
{
    output = m_root;
    return GetRelativePathFromRoot( path, allowFile, output );
}

bool CSystemPathTranslator::GetFullPathFromRoot( const char *path, bool allowFile, filePath& output ) const     { return GenGetFullPathFromRoot( path, allowFile, output ); }
bool CSystemPathTranslator::GetFullPathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const  { return GenGetFullPathFromRoot( path, allowFile, output ); }

template <typename charType>
bool CSystemPathTranslator::GenGetFullPath( const charType *path, bool allowFile, filePath& output ) const
{
    dirTree tree;
    bool file;

    if ( !GetFullPathTree( path, tree, file ) )
        return false;

    if ( file && !allowFile )
    {
        tree.pop_back();

        file = false;
    }

    _File_OutputPathTree( tree, file, output );
    return true;
}

bool CSystemPathTranslator::GetFullPath( const char *path, bool allowFile, filePath& output ) const     { return GenGetFullPath( path, allowFile, output ); }
bool CSystemPathTranslator::GetFullPath( const wchar_t *path, bool allowFile, filePath& output ) const  { return GenGetFullPath( path, allowFile, output ); }

template <typename charType>
bool CSystemPathTranslator::GenGetRelativePathFromRoot( const charType *path, bool allowFile, filePath& output ) const
{
    dirTree tree;
    bool file;

    if ( !GetRelativePathTreeFromRoot( path, tree, file ) )
        return false;

    if ( file && !allowFile )
    {
        tree.pop_back();

        file = false;
    }

    _File_OutputPathTree( tree, file, output );
    return true;
}

bool CSystemPathTranslator::GetRelativePathFromRoot( const char *path, bool allowFile, filePath& output ) const     { return GenGetRelativePathFromRoot( path, allowFile, output ); }
bool CSystemPathTranslator::GetRelativePathFromRoot( const wchar_t *path, bool allowFile, filePath& output ) const  { return GenGetRelativePathFromRoot( path, allowFile, output ); }

template <typename charType>
bool CSystemPathTranslator::GenGetRelativePath( const charType *path, bool allowFile, filePath& output ) const
{
    dirTree tree;
    bool file;

    if ( !GetRelativePathTree( path, tree, file ) )
        return false;

    if ( file && !allowFile )
    {
        tree.pop_back();

        file = false;
    }

    _File_OutputPathTree( tree, file, output );
    return true;
}

bool CSystemPathTranslator::GetRelativePath( const char *path, bool allowFile, filePath& output ) const     { return GenGetRelativePath( path, allowFile, output ); }
bool CSystemPathTranslator::GetRelativePath( const wchar_t *path, bool allowFile, filePath& output ) const  { return GenGetRelativePath( path, allowFile, output ); }