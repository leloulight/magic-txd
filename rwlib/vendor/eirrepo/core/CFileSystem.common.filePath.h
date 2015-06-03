/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        Shared/core/CFileSystem.common.filePath.h
*  PURPOSE:     Path resolution logic of the FileSystem module
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_COMMON_PATHRESOLUTION_
#define _FILESYSTEM_COMMON_PATHRESOLUTION_

#include <string>
#include <assert.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif //_WIN32

template <typename allocatorType>
class MemoryDataStream
{
    char *memData;
    size_t streamSeek;
    size_t streamSize;

public:
    inline MemoryDataStream( void )
    {
        this->memData = NULL;
        this->streamSeek = 0;
        this->streamSize = 0;
    }

    inline ~MemoryDataStream( void )
    {
        if ( void *memPtr = this->memData )
        {
            allocatorType::Free( memPtr );
        }
    }

    inline void SetSize( size_t size )
    {
        if ( this->memData )
        {
            allocatorType::Free( this->memData );

            this->memData = NULL;
        }

        this->memData = (char*)allocatorType::Allocate( size );
        this->streamSeek = 0;
        this->streamSize = size;
    }

    inline size_t GetUsageCount( void ) const
    {
        return this->streamSeek;
    }

    inline void* DetachData( void )
    {
        void *theData = this->memData;

        this->memData = NULL;
        this->streamSeek = 0;
        this->streamSize = 0;

        return theData;
    }

    inline void* ObtainData( size_t dataSize )
    {
        void *dataArray = NULL;

        if ( dataSize != 0 )
        {
            dataArray = ( this->memData + this->streamSeek );

            this->streamSeek += dataSize;
        }

        assert( this->streamSeek < this->streamSize );

        return dataArray;
    }

    inline void* GetData( size_t offset, size_t dataSize )
    {
        return ( this->memData + offset );
    }
};

// String container that acts as filesystem path.
// Supports both unicode and ANSI, can run case-sensitive or opposite.
class filePath
{
    struct rawAllocation
    {
        inline static void* Allocate( size_t memSize )
        {
            return malloc( memSize );
        }

        inline static void Free( void *memPtr )
        {
            free( memPtr );
        }
    };

    typedef MemoryDataStream <rawAllocation> StringDataStream;

    struct stringProvider abstract
    {
        virtual         ~stringProvider( void )         {}

        virtual stringProvider* Clone( void ) const = 0;
        virtual stringProvider* InheritConstruct( void ) const = 0;

        virtual void            Clear( void ) = 0;
        virtual void            SetSize( size_t strSize ) = 0;

        virtual void            Reserve( size_t memSize ) = 0;

        virtual size_t          GetLength( void ) const = 0;

        virtual void            InsertANSI( size_t offset, const char *src, size_t srcLen ) = 0;
        virtual void            InsertUnicode( size_t offset, const wchar_t *src, size_t srcLen ) = 0;

        virtual void            AppendANSI( const char *right, size_t rightLen ) = 0;
        virtual void            AppendUnicode( const wchar_t *right, size_t rightLen ) = 0;
        virtual void            Append( const stringProvider *right ) = 0;

        virtual bool            CompareToANSI( const std::string& right ) const = 0;
        virtual bool            CompareToUnicode( const std::wstring& right ) const = 0;
        virtual bool            CompareTo( const stringProvider *right ) const = 0;

        virtual char            GetCharacterANSI( size_t pos ) const = 0;
        virtual wchar_t         GetCharacterUnicode( size_t pos ) const = 0;

        virtual bool            CompareCharacterAtANSI( char refChar, size_t pos ) const = 0;
        virtual bool            CompareCharacterAtUnicode( wchar_t refChar, size_t pos ) const = 0;

        virtual std::string     ToANSI( void ) const = 0;
        virtual std::wstring    ToUnicode( void ) const = 0;

        virtual bool            IsCaseSensitive( void ) const = 0;

        virtual const char*     GetConstANSIString( void ) const = 0;
        virtual const wchar_t*  GetConstUnicodeString( void ) const = 0;
    };

    stringProvider *strData;

    // Thanks to http://codereview.stackexchange.com/questions/419/converting-between-stdwstring-and-stdstring !
    static std::wstring s2ws(const std::string& s)
    {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
        std::wstring r(len, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
        return r;
    }

    static std::string ws2s(const std::wstring& s)
    {
        int len;
        int slength = (int)s.length() + 1;
        len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0); 
        std::string r(len, '\0');
        WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0); 
        return r;
    }

    struct ansiStringProvider : public stringProvider
    {
        char *stringData;
        size_t dataSize;
        size_t stringLength;
        bool isCaseSensitive;

        inline ansiStringProvider( void )
        {
            this->stringData = NULL;
            this->dataSize = 0;
            this->stringLength = 0;
            this->isCaseSensitive = false;
        }

        inline ~ansiStringProvider( void )
        {
            if ( this->stringData )
            {
                free( this->stringData );
            }
        }

#pragma warning(push)
#pragma warning(disable: 4996)

        stringProvider* Clone( void ) const
        {
            ansiStringProvider *newProvider = new ansiStringProvider();

            char *newStringData = NULL;
            size_t newStringLength = this->stringLength;
            size_t newDataSize = 0;

            if ( newStringLength != 0 )
            {
                newDataSize = newStringLength + 1;
                newStringData = (char*)malloc( newDataSize );

                std::copy( this->stringData, this->stringData + newStringLength + 1, newStringData );
            }

            newProvider->stringData = newStringData;
            newProvider->dataSize = newDataSize;
            newProvider->stringLength = newStringLength;
            newProvider->isCaseSensitive = this->isCaseSensitive;

            return newProvider;
        }

#pragma warning(pop)

        stringProvider* InheritConstruct( void ) const
        {
            return new ansiStringProvider();
        }

        void Clear( void )
        {
            this->stringLength = 0;
        }

        void SetSize( size_t strSize )
        {
            size_t oldStrSize = this->stringLength;

            if ( oldStrSize != strSize )
            {
                Reserve( strSize + 1 );

                for ( size_t n = oldStrSize; n < strSize; n++ )
                {
                    *( this->stringData + n ) = '\0';
                }

                // Zero terminate.
                *( this->stringData + strSize ) = '\0';

                this->stringLength = strSize;
            }
        }

        void Reserve( size_t memSize )
        {
            size_t dataSize = this->dataSize;

            if ( dataSize < memSize )
            {
                this->stringData = (char*)realloc( this->stringData, memSize );
                this->dataSize = memSize;
            }
        }

        size_t GetLength( void ) const
        {
            return this->stringLength;
        }

#pragma warning(push)
#pragma warning(disable: 4996)

        void InsertANSI( size_t offset, const char *src, size_t srcLen )
        {
            // Do a slice intersect of the work region with the insertion region.
            typedef sliceOfData <size_t> stringSlice;

            stringSlice workingRegion( 0, this->stringLength );
            stringSlice intersectRegion( offset, srcLen );

            stringSlice::eIntersectionResult intResult = intersectRegion.intersectWith( workingRegion );

            // Get conflicted memory and save it into a temporary buffer.
            bool hasDataConflict = false;
            size_t conflictStart, conflictEnd;

            if ( this->stringData )
            {
                if ( intResult == stringSlice::INTERSECT_EQUAL )
                {
                    hasDataConflict = true;

                    conflictStart = 0;
                    conflictEnd = this->stringLength;
                }
                else if (
                    intResult == stringSlice::INTERSECT_INSIDE ||
                    intResult == stringSlice::INTERSECT_ENCLOSING )
                {
                    hasDataConflict = true;

                    conflictStart = intersectRegion.GetSliceStartPoint();
                    conflictEnd = intersectRegion.GetSliceEndPoint() + 1;
                }
                else if ( intResult == stringSlice::INTERSECT_ENCLOSING )
                {
                    hasDataConflict = true;

                    conflictStart = workingRegion.GetSliceStartPoint();
                    conflictEnd = workingRegion.GetSliceEndPoint() + 1;
                }
                else if ( intResult == stringSlice::INTERSECT_BORDER_START )
                {
                    hasDataConflict = true;

                    conflictStart = intersectRegion.GetSliceStartPoint();
                    conflictEnd = workingRegion.GetSliceEndPoint() + 1;
                }
                else if ( intResult == stringSlice::INTERSECT_BORDER_END )
                {
                    // Not happening, but added for completeness.
                    hasDataConflict = true;

                    conflictStart = workingRegion.GetSliceStartPoint();
                    conflictEnd = intersectRegion.GetSliceEndPoint();
                }
                else if ( intResult == stringSlice::INTERSECT_UNKNOWN )
                {
                    assert( 0 );
                }
            }

            // If we have conflicted memory, save it somewhere.
            char *conflictedMem = NULL;
            size_t conflictedCount = 0;

            if ( hasDataConflict )
            {
                conflictedCount = ( conflictEnd - conflictStart );
                conflictedMem = new char[ conflictedCount ];

                const char *conflictStartPtr = ( this->stringData + conflictStart );
                const char *conflictEndPtr = ( this->stringData + conflictEnd );

                std::copy( conflictStartPtr, conflictEndPtr, conflictedMem );
            }

            // Reserve enough memory so that we can write to shit.
            size_t lastWritePos = ( offset + srcLen );
            size_t dataShiftUpwardCount = 0;
            bool needUpwardShift = false;

            if ( lastWritePos < this->stringLength )
            {
                dataShiftUpwardCount = ( this->stringLength - lastWritePos );

                needUpwardShift = true;
            }

            size_t newStringLength = ( lastWritePos + this->stringLength );

            Reserve( newStringLength + 1 );

            // If there was any space between the insertion data and the working space, zero it.
            bool hasInvalidRegion = false;
            size_t invalidRegionStart, invalidRegionEnd;

            if ( intResult == stringSlice::INTERSECT_FLOATING_END )
            {
                hasInvalidRegion = true;

                invalidRegionStart = workingRegion.GetSliceEndPoint() + 1;
                invalidRegionEnd = intersectRegion.GetSliceStartPoint();
            }
            else if ( intResult == stringSlice::INTERSECT_FLOATING_START )
            {
                // Cannot happen, but here for completeness sake.
                hasInvalidRegion = true;

                invalidRegionStart = intersectRegion.GetSliceEndPoint() + 1;
                invalidRegionEnd = workingRegion.GetSliceStartPoint();
            }

            if ( hasInvalidRegion )
            {
                char *startPtr = ( this->stringData + invalidRegionStart );
                char *endPtr = ( this->stringData + invalidRegionEnd );

                for ( char *iter = startPtr; iter < endPtr; iter++ )
                {
                    *( iter++ ) = '\0';
                }
            }
            
            // Write the new data that should be inserted.
            std::copy( src, src + srcLen, this->stringData + offset );

            if ( needUpwardShift )
            {
                // Move the data to their correct positions now.
                // Start with incrementing the positions of the valid data.
                const char *sourceStartPtr = ( this->stringData + lastWritePos );
                const char *sourceEndPtr = sourceStartPtr + dataShiftUpwardCount;

                std::copy_backward( sourceStartPtr, sourceEndPtr, ( this->stringData + lastWritePos + conflictedCount ) + dataShiftUpwardCount );
            }

            // Now write the conflicted region, if available.
            if ( hasDataConflict )
            {
                std::copy( conflictedMem, conflictedMem + conflictedCount, this->stringData + lastWritePos );

                // Delete the conflicted memory.
                delete conflictedMem;
            }

            // Zero terminate.
            *( this->stringData + newStringLength ) = '\0';

            // Update parameters
            this->stringLength = newStringLength;
        }

#pragma warning(pop)

        void InsertUnicode( size_t offset, const wchar_t *src, size_t srcLen )
        {
            std::wstring wstrtmp( src, srcLen );
            std::string ansiOut = ws2s( wstrtmp );

            InsertANSI( offset, ansiOut.c_str(), ansiOut.length() );
        }

#pragma warning(push)
#pragma warning(disable: 4996)

        void AppendANSI( const char *right, size_t rightLen )
        {
            size_t ourLength = this->stringLength;
            size_t rightLength = rightLen;
            
            if ( rightLength != 0 )
            {
                // If the underlying data can still host our string, we should reuse our data.
                size_t newDataSize = ourLength + rightLength + 1;

                Reserve( newDataSize );

                // Just append the right string data.
                char *ourData = this->stringData;

                std::copy( right, right + rightLength, ourData + ourLength );

                // Zero terminate ourselves.
                *( ourData + ourLength + rightLength ) = '\0';

                this->stringLength = ( ourLength + rightLength );
            }
        }

#pragma warning(pop)

        void AppendUnicode( const wchar_t *right, size_t rightLen )
        {
            // Convert the string to ANSI before using it.
            std::wstring unicode_tmp = std::wstring( right, rightLen );
            std::string ansiOut = ws2s( unicode_tmp );

            AppendANSI( ansiOut.c_str(), ansiOut.length() );
        }

        void Append( const stringProvider *right )
        {
            std::string ansiOut = right->ToANSI();

            AppendANSI( ansiOut.c_str(), ansiOut.size() );
        }

        static inline bool IsCharacterEqual( char left, char right, bool caseSensitive )
        {
            bool isEqual = false;

            if ( caseSensitive )
            {
                isEqual = ( left == right );
            }
            else
            {
                isEqual = ( toupper( left ) == ( toupper( right ) ) );
            }

            return isEqual;
        }

        inline bool CompareToANSIConst( const char *ansiStr, size_t strLen ) const
        {
            size_t rightSize = strLen;
            size_t ourSize = this->stringLength;

            if ( rightSize != ourSize )
                return false;

            const char *rightStr = ansiStr;
            const char *ourStr = this->stringData;

            bool isCaseSensitive = this->isCaseSensitive;

            for ( size_t n = 0; n < ourSize; n++ )
            {
                char leftChar = *( ourStr + n );
                char rightChar = *( rightStr + n );

                bool isEqual = IsCharacterEqual( leftChar, rightChar, isCaseSensitive );

                if ( !isEqual )
                    return false;
            }

            return true;
        }

        bool CompareToANSI( const std::string& right ) const
        {
            return CompareToANSIConst( right.c_str(), right.size() );
        }

        inline bool CompareToUnicodeConst( const wchar_t *wideStr, size_t wideLen ) const
        {
            std::wstring wstr( wideStr, wideLen );
            std::string ansiStr = ws2s( wstr );

            return CompareToANSIConst( ansiStr.c_str(), ansiStr.size() );
        }

        bool CompareToUnicode( const std::wstring& right ) const
        {
            return CompareToUnicodeConst( right.c_str(), right.size() );
        }

        bool CompareTo( const stringProvider *right ) const
        {
            bool isEqual = false;
            
            if ( const char *ansiStr = right->GetConstANSIString() )
            {
                isEqual = CompareToANSIConst( ansiStr, right->GetLength() );
            }
            else if ( const wchar_t *wideStr = right->GetConstUnicodeString() )
            {
                isEqual = CompareToUnicodeConst( wideStr, right->GetLength() );
            }
            else if ( this->stringData == NULL )
            {
                isEqual = true;
            }

            return isEqual;
        }

        char GetCharacterANSI( size_t strPos ) const
        {
            unsigned char theChar = 0xCC;

            if ( strPos < this->stringLength )
            {
                theChar = *( this->stringData + strPos );
            }

            return (char)theChar;
        }

        wchar_t GetCharacterUnicode( size_t strPos ) const
        {
            wchar_t outputChar = 0xCC;

            if ( strPos < this->stringLength )
            {
                char ansiChar = *( this->stringData + strPos );

                MultiByteToWideChar( CP_ACP, MB_COMPOSITE, &ansiChar, 1, &outputChar, 1 );
            }

            return outputChar;
        }

        bool CompareCharacterAtANSI( char refChar, size_t strPos ) const
        {
            bool isEqual = false;

            if ( strPos < this->stringLength )
            {
                char ourChar = *( this->stringData + strPos );

                isEqual = IsCharacterEqual( refChar, ourChar, this->isCaseSensitive );
            }

            return isEqual;
        }

        bool CompareCharacterAtUnicode( wchar_t refChar, size_t strPos ) const
        {
            bool isEqual = false;

            if ( strPos < this->stringLength )
            {
                char ansiChar = *( this->stringData + strPos );
                wchar_t outputChar;

                int result = MultiByteToWideChar( CP_ACP, MB_COMPOSITE, &ansiChar, 1, &outputChar, 1 );

                if ( result != 0 )
                {
                    isEqual = ( outputChar == refChar );
                }
            }

            return isEqual;
        }

        std::string ToANSI( void ) const
        {
            return std::string( this->stringData, this->stringLength );
        }

        std::wstring ToUnicode( void ) const
        {
            return s2ws( ToANSI() );
        }

        bool IsCaseSensitive( void ) const
        {
            return this->isCaseSensitive;
        }

        // Dangerous function!
        const char* GetConstANSIString( void ) const
        {
            return ( this->stringData ) ? ( this->stringData ) : "";
        }

        const wchar_t* GetConstUnicodeString( void ) const
        {
            return NULL;
        }
    };
    
public:
    inline filePath( void )
    {
        this->strData = NULL;
    }

    inline filePath( const char *right, size_t len )
    {
        this->strData = new ansiStringProvider();

        this->strData->AppendANSI( right, len );
    }

    explicit inline filePath( const char *right )
    {
        this->strData = new ansiStringProvider();

        this->strData->AppendANSI( right, strlen( right ) );
    }

    inline filePath( const filePath& right )
    {
        this->strData = NULL;

        if ( right.strData )
        {
            this->strData = right.strData->Clone();
        }
    }

    inline ~filePath( void )
    {
        if ( stringProvider *provider = this->strData )
        {
            delete provider;
        }
    }

    inline size_t size( void ) const
    {
        size_t outSize = 0;

        if ( stringProvider *provider = this->strData )
        {
            outSize = provider->GetLength();
        }

        return outSize;
    }

    inline void reserve( size_t memSize )
    {
        if ( !this->strData )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->Reserve( memSize );
    }

    inline void resize( size_t strSize )
    {
        if ( this->strData == NULL )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->SetSize( strSize );
    }

    inline bool empty( void ) const
    {
        bool isEmpty = false;
        
        if ( stringProvider *provider = this->strData )
        {
            isEmpty = ( provider->GetLength() == 0 );
        }

        return isEmpty;
    }

    inline void clear( void )
    {
        if ( stringProvider *provider = this->strData )
        {
            provider->Clear();
        }
    }

    inline char at( size_t strPos ) const
    {
        if ( strPos >= size() || !this->strData )
        {
            throw std::out_of_range( "strPos too big!" );
        }

        return this->strData->GetCharacterANSI( strPos );
    }

    inline bool compareCharAt( char refChar, size_t strPos ) const
    {
        bool isEqual = false;

        if ( stringProvider *provider = this->strData )
        {
            isEqual = provider->CompareCharacterAtANSI( refChar, strPos );
        }

        return isEqual;
    }

    inline void insert( size_t offset, const char *src, size_t srcCount )
    {
        if ( !this->strData )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->InsertANSI( offset, src, srcCount );
    }

    inline filePath& operator = ( const filePath& right )
    {
        if ( this->strData )
        {
            delete this->strData;

            this->strData = NULL;
        }

        if ( right.strData )
        {
            this->strData = right.strData->Clone();
        }

        return *this;
    }

    inline filePath& operator = ( const std::string& right )
    {
        if ( !this->strData )
        {
            this->strData = new ansiStringProvider();
        }
        else
        {
            this->strData->Clear();
        }

        this->strData->AppendANSI( right.c_str(), right.size() );

        return *this;
    }

    inline bool operator == ( const filePath& right ) const
    {
        bool isEqual = false;

        if ( this->strData && right.strData )
        {
            isEqual = this->strData->CompareTo( right.strData );
        }

        return isEqual;
    }

    inline bool operator != ( const filePath& right ) const
    {
        return !( this->operator == ( right ) );
    }

    inline filePath operator + ( const filePath& right ) const
    {
        filePath newPath( *this );

        if ( right.strData != NULL )
        {
            if ( newPath.strData == NULL )
            {
                newPath.strData = right.strData->InheritConstruct();
            }

            newPath.strData->Append( right.strData );
        }

        return newPath;
    }

    inline filePath operator + ( const char *right ) const
    {
        filePath newPath( *this );

        if ( newPath.strData == NULL )
        {
            newPath.strData = new ansiStringProvider();
        }

        newPath.strData->AppendANSI( right, strlen( right ) );

        return newPath;
    }

    inline filePath& operator += ( const filePath& right )
    {
        if ( right.strData != NULL )
        {
            if ( this->strData == NULL )
            {
                this->strData = right.strData->InheritConstruct();
            }

            this->strData->Append( right.strData );
        }

        return *this;
    }

    inline filePath& operator += ( const std::string& right )
    {
        if ( this->strData == NULL )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->AppendANSI( right.c_str(), right.size() );

        return *this;
    }

    inline filePath& operator += ( const char *right )
    {
        if ( this->strData == NULL )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->AppendANSI( right, strlen( right ) );
        
        return *this;
    }

    inline filePath& operator += ( const char right )
    {
        if ( this->strData == NULL )
        {
            this->strData = new ansiStringProvider();
        }

        this->strData->AppendANSI( &right, 1 );

        return *this;
    }

    inline operator std::string ( void ) const
    {
        std::string outputString;

        if ( stringProvider *provider = this->strData )
        {
            outputString = provider->ToANSI();
        }

        return outputString;
    }

    inline const char* c_str( void ) const
    {
        const char *outString = "";

        if ( stringProvider *provider = this->strData )
        {
            outString = provider->GetConstANSIString();
        }

        return outString;
    }

    inline operator const char* ( void ) const
    {
        return this->c_str();
    }

    inline const wchar_t *w_str( void ) const
    {
        const wchar_t *outString = L"";

        if ( stringProvider *provider = this->strData )
        {
            outString = provider->GetConstUnicodeString();
        }

        return outString;
    }

    // More advanced functions.
    inline std::string convert_ansi( void ) const
    {
        std::string ansiOut;

        if ( stringProvider *provider = this->strData )
        {
            ansiOut = provider->ToANSI();
        }

        return ansiOut;
    }

    inline std::wstring convert_unicode( void ) const
    {
        std::wstring unicodeOut;

        if ( stringProvider *provider = this->strData )
        {
            unicodeOut = provider->ToUnicode();
        }

        return unicodeOut;
    }

#if 0
    // Useful functions.
    filePath GetFilename( void ) const
    {
        size_t pos = rfind( '/' );

        if ( pos == 0xFFFFFFFF )
        {
            if ( ( pos = rfind( '\\' ) ) == 0xFFFFFFFF )
            {
                return filePath( *this );
            }
        }

        return substr( pos + 1, size() );
    }

    filePath GetPath( void ) const
    {
        size_t pos = rfind( '/' );

        if ( pos == 0xFFFFFFFF )
        {
            if ( ( pos = rfind( '\\' ) ) == 0xFFFFFFFF )
            {
                return filePath();
            }
        }

        return substr( 0, pos );
    }

    const char* GetExtension( void ) const
    {
        size_t pos = rfind( '.' );
        size_t posSlash;

        if ( pos == 0xFFFFFFFF )
            return NULL;

        posSlash = rfind( '/' );

        if ( posSlash == 0xFFFFFFFF )
        {
            if ( ( posSlash = rfind( '\\' ) ) == 0xFFFFFFFF )
            {
                return c_str() + pos;
            }
        }

        if ( posSlash > pos )
            return NULL;

        return c_str() + pos;
    }

    bool IsDirectory( void ) const
    {
        if ( empty() )
            return false;

        return *rbegin() == '/' || *rbegin() == '\\';
    }
#endif
};

#if 0

#ifdef _WIN32
#define _File_PathCharComp( c1, c2 ) ( tolower( c1 ) == tolower( c2 ) )

struct char_traits_i : public std::char_traits <char>
{
    static bool __CLRCALL_OR_CDECL eq( const char left, const char right )
    {
        return toupper(left) == toupper(right);
    }

    static bool __CLRCALL_OR_CDECL ne( const char left, const char right )
    {
        return toupper(left) != toupper(right);
    }

    static bool __CLRCALL_OR_CDECL lt( const char left, const char right )
    {
        return toupper(left) < toupper(right);
    }

    static int __CLRCALL_OR_CDECL compare( const char *s1, const char *s2, size_t n )
    {
        while ( n-- )
        {
            if ( toupper(*s1) < toupper(*s2) )
                return -1;

            if ( toupper(*s1) > toupper(*s2) )
                return 1;

            ++s1; ++s2;
        }

        return 0;
    }

    static const char* __CLRCALL_OR_CDECL find( const char *s, size_t cnt, char a )
    {
        while ( cnt-- && toupper(*s++) != toupper(a) );

        return s;
    }
};

class filePath : public std::basic_string <char, char_traits_i>
{
    typedef std::basic_string <char, char_traits_i> _baseString;

public:
    filePath( const std::string& right )
        : _baseString( right.c_str(), right.size() )
    { }

    bool operator ==( const filePath& right ) const
    {
        if ( right.size() != size() )
            return false;

        return compare( 0, size(), right.c_str() ) == 0;
    }

    bool operator ==( const char *right ) const
    {
        return compare( right ) == 0;
    }

    bool operator ==( const std::string& right ) const
    {
        if ( right.size() != size() )
            return false;

        return compare( 0, size(), right.c_str() ) == 0;
    }

    filePath& operator +=( char right )
    {
        push_back( right );
        return *this;
    }

    filePath& operator +=( const char *right )
    {
        append( right );
        return *this;
    }

    filePath& operator +=( const filePath& right )
    {
        append( right.c_str(), right.size() );
        return *this;
    }

    filePath& operator +=( const std::string& right )
    {
        append( right.c_str(), right.size() );
        return *this;
    }

    filePath operator +( const std::string& right ) const
    {
        return filePath( *this ).append( right.c_str(), right.size() );
    }

    filePath operator +( const char right ) const
    {
        filePath outPath( *this );
        outPath.push_back( right );
        return outPath;
    }

    operator std::string () const
    {
        return std::string( c_str(), size() );
    }
#else
#define _File_PathCharComp( c1, c2 ) ( c1 == c2 )

class filePath : public std::string
{
    typedef std::string _baseString;
public:
#endif
    filePath()
        : _baseString()
    { }

    AINLINE ~filePath()
    { }

    filePath( const filePath& right )
        : _baseString( right.c_str(), right.size() )
    { }

    filePath( const _baseString& right )
        : _baseString( right )
    { }

    explicit filePath( const SString& right )
        : _baseString( right.c_str(), right.size() )
    { }

    filePath( const char *right, size_t len )
        : _baseString( right, len )
    { }

    filePath( const char *right )
        : _baseString( right )
    { }

    char operator []( int idx ) const
    {
        return _baseString::operator []( idx );
    }

    operator const char* () const
    {
        return c_str();
    }

    const char* operator* () const
    {
        return c_str();
    }

    operator SString () const
    {
        return SString( c_str(), size() );
    }

    filePath operator +( const _baseString& right ) const
    {
        return _baseString( *this ) + right;
    }

    filePath operator +( const char *right ) const
    {
        return _baseString( *this ) + right;
    }
};

#endif //0

#endif //_FILESYSTEM_COMMON_PATHRESOLUTION_