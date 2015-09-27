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
                sysTmp->Delete( tmpDir );
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

            return fileSystem->CreateTranslator( path );
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

                resultTranslator = fileSystem->CreateTranslator( rootPath );
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