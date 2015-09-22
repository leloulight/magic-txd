/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.internal.repo.h
*  PURPOSE:     Internal repository creation utilities
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_REPOSITORY_UTILITIES_
#define _FILESYSTEM_REPOSITORY_UTILITIES_

#include <sstream>

class CRepository
{
public:
    inline CRepository( void )
    {
        this->infiniteSequence = 0;
        this->trans = NULL;
    }

    inline ~CRepository( void )
    {
        if ( this->trans )
        {
            CFileTranslator *sysTmp = fileSystem->GetSystemTempTranslator();

            filePath tmpDir;

            if ( sysTmp )
            {
                this->trans->GetFullPath( "@", false, tmpDir );
            }

            delete this->trans;

            this->trans = NULL;

            if ( sysTmp )
            {
                if ( const char *sysPath = tmpDir.c_str() )
                {
                    sysTmp->Delete( sysPath );
                }
                else if ( const wchar_t *sysPath = tmpDir.w_str() )
                {
                    sysTmp->Delete( sysPath );
                }
            }
        }
    }

    inline CFileTranslator* GetTranslator( void )
    {
        if ( !this->trans )
        {
            this->trans = fileSystem->GenerateTempRepository();
        }

        return this->trans;
    }

    inline CFileTranslator* GenerateUniqueDirectory( void )
    {
        // Attempt to get the handle to our temporary directory.
        CFileTranslator *sysTmpRoot = GetTranslator();

        if ( sysTmpRoot )
        {
            // Create temporary storage
            filePath path;
            {
                std::stringstream number_stream;

                number_stream << this->infiniteSequence++;

                std::string dirName( "/" );
                dirName += number_stream.str();
                dirName += "/";

                sysTmpRoot->CreateDir( dirName.c_str() );
                sysTmpRoot->GetFullPathFromRoot( dirName.c_str(), false, path );
            }

            CFileTranslator *uniqueRoot = NULL;

            if ( const char *sysPath = path.c_str() )
            {
                uniqueRoot = fileSystem->CreateTranslator( sysPath );
            }
            else if ( const wchar_t *sysPath = path.w_str() )
            {
                uniqueRoot = fileSystem->CreateTranslator( sysPath );
            }

            return uniqueRoot;
        }
        return NULL;
    }

    static AINLINE CFileTranslator* AcquireDirectoryTranslator( CFileTranslator *fileRoot, const char *dirName )
    {
        CFileTranslator *resultTranslator = NULL;

        bool canBeAcquired = true;

        if ( !fileRoot->Exists( dirName ) )
        {
            canBeAcquired = fileRoot->CreateDir( dirName );
        }

        if ( canBeAcquired )
        {
            filePath rootPath;

            bool gotRoot = fileRoot->GetFullPath( "", false, rootPath );

            if ( gotRoot )
            {
                rootPath += dirName;

                if ( const char *sysPath = rootPath.c_str() )
                {
                    resultTranslator = fileSystem->CreateTranslator( sysPath );
                }
                else if ( const wchar_t *sysPath = rootPath.w_str() )
                {
                    resultTranslator = fileSystem->CreateTranslator( sysPath );
                }
            }
        }

        return resultTranslator;
    }

private:     
    // Sequence used to create unique directories with.
    unsigned int infiniteSequence;

    // The translator of this repository.
    CFileTranslator *trans;
};

#endif //_FILESYSTEM_REPOSITORY_UTILITIES_