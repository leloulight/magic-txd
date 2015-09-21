/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/CFileSystem.common.filePath.h
*  PURPOSE:     Path resolution logic of the FileSystem module
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_COMMON_PATHRESOLUTION_
#define _FILESYSTEM_COMMON_PATHRESOLUTION_

#include <string>
#include <locale>
#include <cwchar>
#include <assert.h>

#include "CFileSystem.common.unichar.h"

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

protected:
    struct stringProvider abstract
    {
        virtual         ~stringProvider( void )         {}

        virtual stringProvider* Clone( void ) const = 0;
        virtual stringProvider* InheritConstruct( void ) const = 0;

        virtual void            Clear( void ) = 0;
        virtual void            SetSize( size_t strSize ) = 0;

        virtual void            Reserve( size_t memSize ) = 0;

        virtual size_t          GetLength( void ) const = 0;
        virtual size_t          GetDecodedLength( void ) const = 0;

        virtual void            InsertANSI( size_t offset, const char *src, size_t srcLen ) = 0;
        virtual void            InsertUnicode( size_t offset, const wchar_t *src, size_t srcLen ) = 0;
        virtual void            InsertUniChars( size_t offset, const char32_t *src, size_t srcLen ) = 0;
        virtual void            Insert( size_t offset, const stringProvider *right, size_t srcLen ) = 0;

        virtual void            AppendANSI( const char *right, size_t rightLen ) = 0;
        virtual void            AppendUnicode( const wchar_t *right, size_t rightLen ) = 0;
        virtual void            AppendUniChars( const char32_t *right, size_t rightLen ) = 0;
        virtual void            Append( const stringProvider *right ) = 0;

        virtual bool            CompareToANSI( const std::string& right, bool caseSensitive ) const = 0;
        virtual bool            CompareToUnicode( const std::wstring& right, bool caseSensitive ) const = 0;
        virtual bool            CompareToUniChars( const std::u32string& right, bool caseSensitive ) const = 0;
        virtual bool            CompareTo( const stringProvider *right, bool caseSensitive ) const = 0;

        virtual char            GetCharacterANSI( size_t pos ) const = 0;
        virtual wchar_t         GetCharacterUnicode( size_t pos ) const = 0;
        virtual char32_t        GetCharacter( size_t pos ) const = 0;

        virtual bool            CompareCharacterAtANSI( char refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual bool            CompareCharacterAtUnicode( wchar_t refChar, size_t pos, bool caseSensitive ) const = 0;
        virtual bool            CompareCharacterAt( char32_t refChat, size_t pos, bool caseSensitive ) const = 0;

        virtual std::string     ToANSI( void ) const = 0;
        virtual std::wstring    ToUnicode( void ) const = 0;

        virtual const char*     GetConstANSIString( void ) const = 0;
        virtual const wchar_t*  GetConstUnicodeString( void ) const = 0;
    };

    stringProvider *strData;

    // Thanks to http://codereview.stackexchange.com/questions/419/converting-between-stdwstring-and-stdstring !
    template <typename wideType>
    static std::basic_string <wideType> s2ws(const std::string& s)
    {
        std::wstring_convert <std::codecvt <wideType, char, std::mbstate_t>, wideType> wideConv;

        return wideConv.from_bytes( s );
    }

#if (_MSC_VER == 1900)
    // FIX FOR VS 2015.
    template <>
    static std::u32string s2ws(const std::string& s)
    {
        std::wstring_convert <std::codecvt <unsigned int, char, std::mbstate_t>, unsigned int> wideConv;

        return reinterpret_cast <std::u32string&&> ( wideConv.from_bytes( s ) );
    }
#endif // VS2015

    template <typename wideType>
    static std::string ws2s(const std::basic_string <wideType>& s)
    {
        std::wstring_convert <std::codecvt <wideType, char, std::mbstate_t>, wideType> wideConv;

        return wideConv.to_bytes( s );
    }

#if (_MSC_VER == 1900)
    template <>
    static std::string ws2s(const std::u32string& s)
    {
        std::wstring_convert <std::codecvt <unsigned int, char, std::mbstate_t>, unsigned int> wideConv;

        return wideConv.to_bytes( *(const std::basic_string <unsigned int>*)&s );
    }
#endif // VS2015

    template <typename charType>
    inline static const charType* GetEmptyStringLiteral( void )
    {
        static_assert( false, "unknown character type" );;
    }

    template <>
    inline static const char* GetEmptyStringLiteral <char> ( void )
    {
        return "";
    }

    template <>
    inline static const wchar_t* GetEmptyStringLiteral <wchar_t> ( void )
    {
        return L"";
    }

    template <typename charType>
    struct customCharString
    {
        charType *stringData;
        size_t dataSize;
        size_t stringLength;

        inline customCharString( void )
        {
            this->stringData = NULL;
            this->dataSize = 0;
            this->stringLength = 0;
        }

        inline customCharString( const customCharString& right )
        {
            charType *newStringData = NULL;
            size_t newStringLength = right.stringLength;
            size_t newDataSize = 0;

            if ( newStringLength != 0 )
            {
                newDataSize = ( newStringLength + 1 ) * sizeof( charType );
                newStringData = (charType*)malloc( newDataSize );

                std::copy( right.stringData, right.stringData + newStringLength + 1, newStringData );
            }

            this->stringData = newStringData;
            this->dataSize = newDataSize;
            this->stringLength = newStringLength;
        }

        inline ~customCharString( void )
        {
            if ( charType *strData = this->stringData )
            {
                free( strData );
            }
        }

        inline void clear( void )
        {
            this->stringLength = 0;
        }

        inline void resize( size_t strSize )
        {
            size_t oldStrSize = this->stringLength;

            if ( oldStrSize != strSize )
            {
                reserve( strSize + 1 );

                for ( size_t n = oldStrSize; n < strSize; n++ )
                {
                    *( this->stringData + n ) = '\0';
                }

                // Zero terminate.
                *( this->stringData + strSize ) = '\0';

                this->stringLength = strSize;
            }
        }

        inline void reserve( size_t strLen )
        {
            size_t dataSize = this->dataSize;

            size_t newDataSize = strLen * sizeof( charType );

            if ( dataSize < newDataSize )
            {
                charType *oldstrData = this->stringData;

                bool hadData = ( oldstrData != NULL );

                this->stringData = (charType*)realloc( oldstrData, newDataSize );
                this->dataSize = newDataSize;

                if ( hadData == false )
                {
                    this->stringData[0] = 0;
                }
            }
        }

        inline size_t length( void ) const
        {
            return this->stringLength;
        }

        inline bool empty( void ) const
        {
            return ( this->stringLength == 0 );
        }

        inline void insert( size_t offset, const charType *src, size_t srcLen )
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
            charType *conflictedMem = NULL;
            size_t conflictedCount = 0;

            if ( hasDataConflict )
            {
                conflictedCount = ( conflictEnd - conflictStart );
                conflictedMem = new charType[ conflictedCount ];

                const charType *conflictStartPtr = ( this->stringData + conflictStart );
                const charType *conflictEndPtr = ( this->stringData + conflictEnd );

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

            reserve( newStringLength + 1 );

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
                charType *startPtr = ( this->stringData + invalidRegionStart );
                charType *endPtr = ( this->stringData + invalidRegionEnd );

                for ( charType *iter = startPtr; iter < endPtr; iter++ )
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
                const charType *sourceStartPtr = ( this->stringData + lastWritePos );
                const charType *sourceEndPtr = sourceStartPtr + dataShiftUpwardCount;

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

        inline void append( const charType *right, size_t rightLen )
        {
            size_t ourLength = this->stringLength;
            size_t rightLength = rightLen;
            
            if ( rightLength != 0 )
            {
                // If the underlying data can still host our string, we should reuse our data.
                size_t newLen = ourLength + rightLength + 1;

                reserve( newLen );

                // Just append the right string data.
                charType *ourData = this->stringData;

                std::copy( right, right + rightLength, ourData + ourLength );

                // Zero terminate ourselves.
                *( ourData + ourLength + rightLength ) = '\0';

                this->stringLength = ( ourLength + rightLength );
            }
        }

        inline bool equal( const charType *str, size_t strLen, bool caseSensitive ) const
        {
            size_t rightSize = strLen;
            size_t ourSize = this->stringLength;

            if ( rightSize != ourSize )
                return false;

            const charType *rightStr = str;
            const charType *ourStr = this->stringData;

            for ( size_t n = 0; n < ourSize; n++ )
            {
                charType leftChar = *( ourStr + n );
                charType rightChar = *( rightStr + n );

                bool isEqual = IsCharacterEqual( leftChar, rightChar, caseSensitive );

                if ( !isEqual )
                    return false;
            }

            return true;
        }

        inline bool compare_at( charType refChar, size_t strPos, bool& equal, bool caseSensitive ) const
        {
            bool couldCompare = false;

            if ( strPos < this->stringLength )
            {
                charType ourChar = *( this->stringData + strPos );

                equal = IsCharacterEqual( refChar, ourChar, caseSensitive );

                couldCompare = true;
            }
            
            return couldCompare;
        }

        inline const charType* c_str( void ) const
        {
            return ( this->stringData ) ? ( this->stringData ) : GetEmptyStringLiteral <charType> ();
        }

        inline bool at( size_t n, charType& charOut ) const
        {
            bool gotChar = false;

            if ( n < this->stringLength )
            {
                charOut = *( this->stringData + n );

                gotChar = true;
            }

            return gotChar;
        }
    };

    struct ansiStringProvider : public stringProvider
    {
        customCharString <char> strData;

        stringProvider* Clone( void ) const
        {
            return new ansiStringProvider( *this );
        }

        stringProvider* InheritConstruct( void ) const
        {
            return new ansiStringProvider();
        }

        void Clear( void ) override                     { strData.clear(); }
        void SetSize( size_t strSize ) override         { strData.resize( strSize ); }
        void Reserve( size_t strSize ) override         { strData.reserve( strSize ); }
        size_t GetLength( void ) const override         { return strData.length(); }
        size_t GetDecodedLength( void ) const override  { return strData.length(); }

        void InsertANSI( size_t offset, const char *src, size_t srcLen )        { strData.insert( offset, src, srcLen ); }
        void InsertUnicode( size_t offset, const wchar_t *src, size_t srcLen )
        {
            std::string ansiOut = ws2s( std::wstring( src, srcLen ) );

            InsertANSI( offset, ansiOut.c_str(), ansiOut.length() );
        }
        void InsertUniChars( size_t offset, const char32_t *src, size_t srcLen )
        {
            std::string ansiOut = ws2s( std::u32string( src, srcLen ) );

            InsertANSI( offset, ansiOut.c_str(), ansiOut.length() );
        }

        void Insert( size_t offset, const stringProvider *right, size_t srcLen )
        {
            if ( const char *str = right->GetConstANSIString() )
            {
                InsertANSI( offset, str, srcLen );
            }
            else
            {
                std::string rightANSI = right->ToANSI();

                InsertANSI( offset, rightANSI.c_str(), srcLen );
            }
        }

        void AppendANSI( const char *right, size_t rightLen )       { strData.append( right, rightLen ); }
        void AppendUnicode( const wchar_t *right, size_t rightLen )
        {
            // Convert the string to ANSI before using it.
            std::string ansiOut = ws2s( std::wstring( right, rightLen ) );

            AppendANSI( ansiOut.c_str(), ansiOut.length() );
        }
        void AppendUniChars( const char32_t *right, size_t rightLen ) override
        {
            std::string ansiOut = ws2s( std::u32string( right, rightLen ) );

            AppendANSI( ansiOut.c_str(), ansiOut.length() );
        }

        void Append( const stringProvider *right )
        {
            std::string ansiOut = right->ToANSI();

            AppendANSI( ansiOut.c_str(), ansiOut.size() );
        }

        inline bool CompareToANSIConst( const char *ansiStr, size_t strLen, bool caseSensitive ) const
        {
            return strData.equal( ansiStr, strLen, caseSensitive );
        }

        bool CompareToANSI( const std::string& right, bool caseSensitive ) const
        {
            return CompareToANSIConst( right.c_str(), right.size(), caseSensitive );
        }

        inline bool CompareToUnicodeConst( const wchar_t *wideStr, size_t wideLen, bool caseSensitive ) const
        {
            std::string ansiStr = ws2s( std::wstring( wideStr, wideLen ) );

            return CompareToANSIConst( ansiStr.c_str(), ansiStr.size(), caseSensitive );
        }

        bool CompareToUnicode( const std::wstring& right, bool caseSensitive ) const
        {
            std::string ansiStr = ws2s( right );

            return CompareToANSIConst( ansiStr.c_str(), ansiStr.size(), caseSensitive );
        }

        bool CompareToUniChars( const std::u32string& right, bool caseSensitive ) const override
        {
            std::string ansiStr = ws2s( right );

            return CompareToANSIConst( ansiStr.c_str(), ansiStr.size(), caseSensitive );
        }

        bool CompareTo( const stringProvider *right, bool caseSensitive ) const
        {
            bool isEqual = false;
            
            if ( const char *ansiStr = right->GetConstANSIString() )
            {
                isEqual = CompareToANSIConst( ansiStr, right->GetLength(), caseSensitive );
            }
            else if ( const wchar_t *wideStr = right->GetConstUnicodeString() )
            {
                isEqual = CompareToUnicodeConst( wideStr, right->GetLength(), caseSensitive );
            }
            else if ( this->strData.empty() )
            {
                isEqual = true;
            }

            return isEqual;
        }

        char GetCharacterANSI( size_t strPos ) const
        {
            char theChar = (char)0xCC;

            strData.at( strPos, theChar );

            return (char)theChar;
        }

    private:
        bool unicode_fetch( size_t strPos, wchar_t& charOut ) const
        {
            char ansiChar;

            bool gotChar = strData.at( strPos, ansiChar );

            if ( gotChar )
            {
                int unicResult = MultiByteToWideChar( CP_ACP, MB_COMPOSITE, &ansiChar, 1, &charOut, 1 );

                if ( unicResult != 0 )
                {
                    return true;
                }
            }

            return false;
        }

    public:
        wchar_t GetCharacterUnicode( size_t strPos ) const
        {
            wchar_t outputChar = 0xCC;

            unicode_fetch( strPos, outputChar );

            return outputChar;
        }

        char32_t GetCharacter( size_t strPos ) const override
        {
            wchar_t outputChar = 0xCC;

            unicode_fetch( strPos, outputChar );

            return outputChar;
        }

        bool CompareCharacterAtANSI( char refChar, size_t strPos, bool caseSensitive ) const
        {
            bool equals;

            return strData.compare_at( refChar, strPos, equals, caseSensitive ) && equals;
        }

        bool CompareCharacterAtUnicode( wchar_t refChar, size_t strPos, bool caseSensitive ) const
        {
            wchar_t ourChar;

            return unicode_fetch( strPos, ourChar ) && IsCharacterEqual( ourChar, refChar, caseSensitive );
        }

        bool CompareCharacterAt( char32_t refChar, size_t strPos, bool caseSensitive ) const override
        {
            wchar_t ourChar;

            return unicode_fetch( strPos, ourChar ) && IsCharacterEqual( (char32_t)ourChar, refChar, caseSensitive );
        }

        std::string ToANSI( void ) const
        {
            return std::string( this->strData.c_str(), this->strData.length() );
        }

        std::wstring ToUnicode( void ) const
        {
            return s2ws <wchar_t> ( ToANSI() );
        }

        // Dangerous function!
        const char* GetConstANSIString( void ) const
        {
            return strData.c_str();
        }

        const wchar_t* GetConstUnicodeString( void ) const
        {
            return NULL;
        }
    };

    struct wideStringProvider : public stringProvider
    {
        typedef character_env <wchar_t> char_env;

        customCharString <wchar_t> strData;

        stringProvider* Clone( void ) const
        {
            return new wideStringProvider( *this );
        }

        stringProvider* InheritConstruct( void ) const
        {
            return new wideStringProvider();
        }

        void Clear( void ) override                     { strData.clear(); }
        void SetSize( size_t strSize ) override         { strData.resize( strSize ); }
        void Reserve( size_t strSize ) override         { strData.reserve( strSize ); }
        size_t GetLength( void ) const override         { return strData.length(); }
        size_t GetDecodedLength( void ) const override
        {
            size_t len = 0;

            for ( char_env::const_iterator iter( strData.c_str() ); !iter.IsEnd(); iter.Increment() )
            {
                len++;
            }

            return len;
        }

        inline static size_t get_encoding_strpos( const wchar_t *data, size_t charPos )
        {
            char_env::const_iterator iter( data );

            size_t cur_i = 0;

            for ( ; !iter.IsEnd(), cur_i < charPos; iter.Increment(), cur_i++ );

            return ( iter.GetPointer() - data );
        }

        void InsertUnicode( size_t offset, const wchar_t *src, size_t srcLen )
        {
            // Get a valid string point to insert our wide-string code points into.

            strData.insert( get_encoding_strpos( strData.c_str(), offset ), src, srcLen );
        }
        void InsertANSI( size_t offset, const char *src, size_t srcLen )
        {
            std::wstring wideOut = s2ws <wchar_t> ( std::string( src, srcLen ) );

            InsertUnicode( offset, wideOut.c_str(), wideOut.length() );
        }

    private:
        inline std::wstring encode_to_wide( const char32_t *src, size_t srcLen )
        {
            size_t n = 0;
            std::wstring wideString;

            for ( char_env::encoding_iterator iter( src, srcLen ); !iter.IsEnd(); iter.Increment() )
            {
                char_env::enc_result data;
                iter.Resolve( data );

                for ( size_t n = 0; n < data.numData; n++ )
                {
                    wideString += data.data[ n ];
                }
            }

            return wideString;
        }

    public:
        void InsertUniChars( size_t offset, const char32_t *src, size_t srcLen ) override
        {
            std::wstring wideString = encode_to_wide( src, srcLen );

            InsertUnicode( offset, wideString.c_str(), wideString.length() );
        }

        void Insert( size_t offset, const stringProvider *right, size_t srcLen )
        {
            if ( const wchar_t *str = right->GetConstUnicodeString() )
            {
                InsertUnicode( offset, str, srcLen );
            }
            else
            {
                std::wstring uniStr = right->ToUnicode();

                InsertUnicode( offset, uniStr.c_str(), srcLen );
            }
        }

        void AppendUnicode( const wchar_t *right, size_t rightLen ) { strData.append( right, rightLen ); }
        void AppendANSI( const char *right, size_t rightLen )
        {
            std::wstring wideOut = s2ws <wchar_t> ( std::string( right, rightLen ) );

            AppendUnicode( wideOut.c_str(), wideOut.length() );
        }
        void AppendUniChars( const char32_t *right, size_t rightLen ) override
        {
            std::wstring wideOut = encode_to_wide( right, rightLen );

            AppendUnicode( wideOut.c_str(), wideOut.length() );
        }

        void Append( const stringProvider *right )
        {
            std::wstring wideOut = right->ToUnicode();

            AppendUnicode( wideOut.c_str(), wideOut.size() );
        }

        inline bool CompareToUnicodeConst( const wchar_t *wideStr, size_t wideLen, bool caseSensitive ) const
        {
            return UniversalCompareStrings( this->strData.c_str(), this->strData.length(), wideStr, wideLen, caseSensitive );
        }

        inline bool CompareToANSIConst( const char *ansiStr, size_t strLen, bool caseSensitive ) const
        {
            std::wstring wideStr = s2ws <wchar_t> ( std::string( ansiStr, strLen ) );

            return CompareToUnicodeConst( wideStr.c_str(), wideStr.length(), caseSensitive );
        }

        bool CompareToANSI( const std::string& right, bool caseSensitive ) const
        {
            return CompareToANSIConst( right.c_str(), right.size(), caseSensitive );
        }

        bool CompareToUnicode( const std::wstring& right, bool caseSensitive ) const
        {
            return CompareToUnicodeConst( right.c_str(), right.size(), caseSensitive );
        }

        bool CompareToUniChars( const std::u32string& right, bool caseSensitive ) const override
        {
            return UniversalCompareStrings( this->strData.c_str(), this->strData.length(), right.c_str(), right.length(), caseSensitive );
        }

        bool CompareTo( const stringProvider *right, bool caseSensitive ) const
        {
            bool isEqual = false;
            
            if ( const char *ansiStr = right->GetConstANSIString() )
            {
                isEqual = CompareToANSIConst( ansiStr, right->GetLength(), caseSensitive );
            }
            else if ( const wchar_t *wideStr = right->GetConstUnicodeString() )
            {
                isEqual = CompareToUnicodeConst( wideStr, right->GetLength(), caseSensitive );
            }
            else if ( this->strData.empty() )
            {
                isEqual = true;
            }

            return isEqual;
        }

    private:
        bool ansi_fetch( size_t strPos, char& charOut ) const
        {
            wchar_t wideChar;

            bool gotChar = strData.at( strPos, wideChar );

            if ( gotChar )
            {
                char default_char = '_';

                int unicResult = WideCharToMultiByte( CP_ACP, MB_COMPOSITE, &wideChar, 1, &charOut, 1, &default_char, NULL );

                if ( unicResult != 0 )
                {
                    return true;
                }
            }

            return false;
        }

    public:
        char GetCharacterANSI( size_t strPos ) const
        {
            char theChar = (char)0xCC;

            ansi_fetch( strPos, theChar );

            return (char)theChar;
        }

        wchar_t GetCharacterUnicode( size_t strPos ) const
        {
            wchar_t outputChar = 0xCC;

            strData.at( strPos, outputChar );

            return outputChar;
        }
         
        char32_t GetCharacter( size_t strPos ) const override
        {
            char_env::ucp_t resVal = 0xCC;
            
            const wchar_t *ourStr = this->strData.c_str();

            size_t realPos = get_encoding_strpos( ourStr, strPos );

            if ( realPos < this->strData.length() )
            {
                resVal = char_env::const_iterator( ourStr + realPos ).Resolve();
            }

            return resVal;
        }

        bool CompareCharacterAtANSI( char refChar, size_t strPos, bool caseSensitive ) const
        {
            char ourChar;

            return ansi_fetch( strPos, ourChar ) && IsCharacterEqual( ourChar, refChar, caseSensitive );
        }

        bool CompareCharacterAtUnicode( wchar_t refChar, size_t strPos, bool caseSensitive ) const
        {
            bool equals;

            return strData.compare_at( refChar, strPos, equals, caseSensitive ) && equals;
        }

        bool CompareCharacterAt( char32_t refChar, size_t strPos, bool caseSensitive ) const override
        {
            const wchar_t *ourStr = this->strData.c_str();

            size_t realPos = get_encoding_strpos( ourStr, strPos );

            bool isEqual = false;

            if ( realPos < this->strData.length() )
            {
                char_env::ucp_t ourVal = char_env::const_iterator( ourStr + realPos ).Resolve();

                isEqual = IsCharacterEqual( ourVal, refChar, caseSensitive );
            }

            return isEqual;
        }

        std::string ToANSI( void ) const
        {
            return ws2s( ToUnicode() );
        }

        std::wstring ToUnicode( void ) const
        {
            return std::wstring( this->strData.c_str(), this->strData.length() );
        }

        // Dangerous function!
        const char* GetConstANSIString( void ) const
        {
            return NULL;
        }

        const wchar_t* GetConstUnicodeString( void ) const
        {
            return strData.c_str();
        }
    };

    template <typename charType>
    inline static stringProvider* CreateStringProvider( void )
    {
        return NULL;
    }

    template <>
    inline static stringProvider* CreateStringProvider <char> ( void )
    {
        return new ansiStringProvider();
    }

    template <>
    inline static stringProvider* CreateStringProvider <wchar_t> ( void )
    {
        return NULL;
    }

    template <typename charType>
    inline static void AppendStringProvider( stringProvider *prov, const charType *str, size_t len )
    {
        static_assert( false, "invalid character type for string provider append" );
    }

    template <>
    inline static void AppendStringProvider( stringProvider *prov, const char *str, size_t len )
    {
        prov->AppendANSI( str, len );
    }

    template <>
    inline static void AppendStringProvider( stringProvider *prov, const wchar_t *str, size_t len )
    {
        prov->AppendUnicode( str, len );
    }

    template <typename charType>
    inline static const charType* GetConstCharStringProvider( stringProvider *prov )
    {
        return NULL;
    }

    template <>
    inline static const char* GetConstCharStringProvider <char> ( stringProvider *prov )
    {
        return prov->GetConstANSIString();
    }

    template <>
    inline static const wchar_t* GetConstCharStringProvider <wchar_t> ( stringProvider *prov )
    {
        return prov->GetConstUnicodeString();
    }

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

    inline filePath( const wchar_t *right, size_t len )
    {
        this->strData = new wideStringProvider();

        this->strData->AppendUnicode( right, len );
    }

    explicit inline filePath( const char *right )
    {
        this->strData = new ansiStringProvider();

        this->strData->AppendANSI( right, strlen( right ) );
    }

    explicit inline filePath( const wchar_t *right )
    {
        this->strData = new wideStringProvider();
       
        this->strData->AppendUnicode( right, std::char_traits <wchar_t>::length( right ) );
    }

    template <typename charType>
    AINLINE static filePath Make( const charType *str, size_t len )
    {
        filePath path;

        path.strData = CreateStringProvider <charType> ();

        AppendStringProvider <charType> ( path.strData, str, len );

        return path;
    }

    inline filePath( const filePath& right )
    {
        this->strData = NULL;

        if ( right.strData )
        {
            this->strData = right.strData->Clone();
        }
    }

    inline filePath( filePath&& right )
    {
        this->strData = right.strData;

        right.strData = NULL;
    }

    inline ~filePath( void )
    {
        if ( stringProvider *provider = this->strData )
        {
            delete provider;
        }
    }

    inline void upgrade_unicode( void )
    {
        if ( stringProvider *provider = this->strData )
        {
            if ( const char *ansiConst = provider->GetConstANSIString() )
            {
                std::wstring wideStr = s2ws <wchar_t> ( std::string( ansiConst, provider->GetLength() ) );

                delete provider;

                this->strData = new wideStringProvider();

                this->strData->AppendUnicode( wideStr.c_str(), wideStr.length() );
            }
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

    inline size_t charlen( void ) const
    {
        size_t outLen = 0;

        if ( stringProvider *provider = this->strData )
        {
            outLen = provider->GetDecodedLength();
        }

        return outLen;
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

    inline wchar_t at_w( size_t strPos ) const
    {
        if ( strPos >= size() || !this->strData )
        {
            throw std::out_of_range( "strPos too big!" );
        }

        return this->strData->GetCharacterUnicode( strPos );
    }

    inline bool compareCharAt( char refChar, size_t strPos, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( stringProvider *provider = this->strData )
        {
            isEqual = provider->CompareCharacterAtANSI( refChar, strPos, caseSensitive );
        }

        return isEqual;
    }

    inline bool compareCharAt( wchar_t refChar, size_t strPos, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( stringProvider *provider = this->strData )
        {
            isEqual = provider->CompareCharacterAtUnicode( refChar, strPos, caseSensitive );
        }

        return isEqual;
    }

    inline bool compareCharAt( char32_t refChar, size_t strPos, bool caseSensitive = true ) const
    {
        bool isEqual = false;

        if ( stringProvider *provider = this->strData )
        {
            isEqual = provider->CompareCharacterAt( refChar, strPos, caseSensitive );
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

    inline void insert( size_t offset, const wchar_t *src, size_t srcCount )
    {
        this->upgrade_unicode();

        if ( !this->strData )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->InsertUnicode( offset, src, srcCount );
    }

    inline void insert( size_t offset, const char32_t *src, size_t srcLen )
    {
        this->upgrade_unicode();

        if ( !this->strData )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->InsertUniChars( offset, src, srcLen );
    }

    inline void insert( size_t offset, const filePath& src, size_t srcLen )
    {
        if ( src.is_underlying_char <wchar_t> () )
        {
            this->upgrade_unicode();

            if ( !this->strData )
            {
                this->strData = new wideStringProvider();
            }
        }
        else
        {
            if ( !this->strData )
            {
                this->strData = new ansiStringProvider();
            }
        }

        if ( const stringProvider *rightProv = src.strData )
        {
            this->strData->Insert( offset, rightProv, srcLen );
        }
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

    inline filePath& operator = ( filePath&& right )
    {
        if ( this->strData )
        {
            delete this->strData;

            this->strData = NULL;
        }

        this->strData = right.strData;

        right.strData = NULL;

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
            isEqual = this->strData->CompareTo( right.strData, true );
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

    inline filePath operator + ( const wchar_t *right ) const
    {
        filePath newPath( *this );

        if ( newPath.strData == NULL )
        {
            newPath.strData = new wideStringProvider();
        }

        newPath.strData->AppendUnicode( right, std::char_traits <wchar_t>::length( right ) );

        return newPath;
    }

    inline filePath operator + ( const char32_t *right ) const
    {
        filePath newPath( *this );

        if ( newPath.strData == NULL )
        {
            newPath.strData = new wideStringProvider();
        }

        newPath.strData->AppendUniChars( right, std::char_traits <char32_t>::length( right ) );

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

    inline filePath& operator += ( const std::wstring& right )
    {
        this->upgrade_unicode();

        if ( this->strData == NULL )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUnicode( right.c_str(), right.length() );

        return *this;
    }

    inline filePath& operator += ( const std::u32string& right )
    {
        this->upgrade_unicode();

        if ( this->strData == NULL )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUniChars( right.c_str(), right.length() );

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

    inline filePath& operator += ( const wchar_t *right )
    {
        this->upgrade_unicode();

        if ( !this->strData )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUnicode( right, std::char_traits <wchar_t>::length( right ) );

        return *this;
    }

    inline filePath& operator += ( const char32_t *right )
    {
        this->upgrade_unicode();

        if ( !this->strData )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUniChars( right, std::char_traits <char32_t>::length( right ) );

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

    inline filePath& operator += ( const wchar_t right )
    {
        this->upgrade_unicode();

        if ( this->strData == NULL )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUnicode( &right, 1 );

        return *this;
    }

    inline filePath& operator += ( const char32_t right )
    {
        this->upgrade_unicode();

        if ( this->strData == NULL )
        {
            this->strData = new wideStringProvider();
        }

        this->strData->AppendUniChars( &right, 1 );

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

    // Functions to determine the underlying string type.
    template <typename charType>
    inline bool is_underlying_char( void ) const
    {
        return false;
    }

    template <>
    inline bool is_underlying_char <char> ( void ) const
    {
        return ( this->strData->GetConstANSIString() != NULL );
    }

    template <>
    inline bool is_underlying_char <wchar_t> ( void ) const
    {
        return ( this->strData->GetConstUnicodeString() != NULL );
    }

    inline stringProvider* string_prov( void ) const
    {
        return this->strData;
    }

    static inline bool is_system_case_sensitive( void )
    {
#if defined(_WIN32)
        return false;
#elif defined(__linux__)
        return true;
#else
#error Unknown platform
#endif
    }
};

// Temporary special string.
template <typename charType>
struct filePathLink : public filePath
{
    inline filePathLink( const filePath& origObj ) : filePath()
    {
        bool canMove = ( origObj.is_underlying_char <charType> () == true );

        if ( canMove )
        {
            this->strData = origObj.string_prov();

            this->hasMoved = true;
        }
        else
        {
            this->strData = CreateStringProvider <charType> ();

            assert( this->strData != NULL );

            if ( stringProvider *origProv = origObj.string_prov() )
            {
                this->strData->Append( origProv );
            }

            this->hasMoved = false;
        }
    }

    inline ~filePathLink( void )
    {
        if ( this->hasMoved )
        {
            this->strData = NULL;
        }
    }

    // This function is sure to succeed.
    inline const charType* to_char( void ) const
    {
        return GetConstCharStringProvider <charType> ( this->strData );
    }

    bool hasMoved;
};

#endif //_FILESYSTEM_COMMON_PATHRESOLUTION_