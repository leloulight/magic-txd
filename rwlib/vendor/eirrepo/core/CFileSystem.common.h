/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        Shared/core/CFileSystem.common.h
*  PURPOSE:     Common definitions of the FileSystem module
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_COMMON_DEFINITIONS_
#define _FILESYSTEM_COMMON_DEFINITIONS_

#include <vector>

// Basic always inline definition.
#ifndef AINLINE
#ifdef _MSC_VER
#define AINLINE __forceinline
#elif __linux__
#define AINLINE __attribute__((always_inline))
#else
#define AINLINE
#endif
#endif

// Mathematically correct data slice logic.
// It is one of the most important theorems in computing abstraction.
template <typename numberType>
class sliceOfData
{
public:
    inline sliceOfData( void )
    {
        this->startOffset = 0;
        this->endOffset = -1;
    }

    inline sliceOfData( numberType startOffset, numberType dataSize )
    {
        this->startOffset = startOffset;
        this->endOffset = startOffset + ( dataSize - 1 );
    }

    enum eIntersectionResult
    {
        INTERSECT_EQUAL,
        INTERSECT_INSIDE,
        INTERSECT_BORDER_START,
        INTERSECT_BORDER_END,
        INTERSECT_ENCLOSING,
        INTERSECT_FLOATING_START,
        INTERSECT_FLOATING_END,
        INTERSECT_UNKNOWN   // if something went horribly wrong (like NaNs).
    };

    // Static methods for easier result management.
    static bool isBorderIntersect( eIntersectionResult result )
    {
        return ( result == INTERSECT_BORDER_START || result == INTERSECT_BORDER_END );
    }

    static bool isFloatingIntersect( eIntersectionResult result )
    {
        return ( result == INTERSECT_FLOATING_START || result == INTERSECT_FLOATING_END );
    }

    inline numberType GetSliceSize( void ) const
    {
        return ( this->endOffset - this->startOffset ) + 1;
    }

    inline void SetSlicePosition( numberType val )
    {
        const numberType sliceSize = GetSliceSize();

        this->startOffset = val;
        this->endOffset = val + ( sliceSize - 1 );
    }

    inline void OffsetSliceBy( numberType val )
    {
        SetSlicePosition( this->startOffset + val );
    }

    inline void SetSliceStartPoint( numberType val )
    {
        this->startOffset = val;
    }

    inline void SetSliceEndPoint( numberType val )
    {
        this->endOffset = val;
    }

    inline numberType GetSliceStartPoint( void ) const
    {
        return startOffset;
    }

    inline numberType GetSliceEndPoint( void ) const
    {
        return endOffset;
    }

    inline eIntersectionResult intersectWith( const sliceOfData& right ) const
    {
        // Make sure the slice has a valid size.
        if ( this->endOffset >= this->startOffset &&
             right.endOffset >= right.startOffset )
        {
            // Get generic stuff.
            numberType sliceStartA = startOffset, sliceEndA = endOffset;
            numberType sliceStartB = right.startOffset, sliceEndB = right.endOffset;

            // slice A -> this
            // slice B -> right

            // Handle all cases.
            // We only implement the logic with comparisons only, as it is the most transparent for all number types.
            if ( sliceStartA == sliceStartB && sliceEndA == sliceEndB )
            {
                // Slice A is equal to Slice B
                return INTERSECT_EQUAL;
            }

            if ( sliceStartB >= sliceStartA && sliceEndB <= sliceEndA )
            {
                // Slice A is enclosing Slice B
                return INTERSECT_ENCLOSING;
            }

            if ( sliceStartB <= sliceStartA && sliceEndB >= sliceEndA )
            {
                // Slice A is inside Slice B
                return INTERSECT_INSIDE;
            }

            if ( sliceStartB < sliceStartA && ( sliceEndB >= sliceStartA && sliceEndB <= sliceEndA ) )
            {
                // Slice A is being intersected at the starting point.
                return INTERSECT_BORDER_START;
            }

            if ( sliceEndB > sliceEndA && ( sliceStartB >= sliceStartA && sliceStartB <= sliceEndA ) )
            {
                // Slice A is being intersected at the ending point.
                return INTERSECT_BORDER_END;
            }

            if ( sliceStartB < sliceStartA && sliceEndB < sliceStartA )
            {
                // Slice A is after Slice B
                return INTERSECT_FLOATING_END;
            }

            if ( sliceStartB > sliceEndA && sliceEndB > sliceEndA )
            {
                // Slice A is before Slice B
                return INTERSECT_FLOATING_START;
            }
        }

        return INTERSECT_UNKNOWN;
    }

private:
    numberType startOffset;
    numberType endOffset;
};

#include "CFileSystem.common.filePath.h"

#include <bitset>

// Macro that defines how alignment works.
//  num: base of the number to be aligned
//  sector: aligned-offset that should be added to num
//  align: number of bytes to align to
// EXAMPLE: ALIGN( 0x1001, 4, 4 ) -> 0x1000 (equivalent of compiler structure padding alignment)
//          ALIGN( 0x1003, 1, 4 ) -> 0x1000
//          ALIGN( 0x1003, 2, 4 ) -> 0x1004
template <typename numberType>
AINLINE numberType _ALIGN_GP( numberType num, numberType sector, numberType align )
{
	// General purpose alignment routine.
    // Not as fast as the bitfield version.
    numberType sectorOffset = ((num) + (sector) - 1);

    return sectorOffset - ( sectorOffset % align );
}

template <typename numberType>
AINLINE numberType _ALIGN_NATIVE( numberType num, numberType sector, numberType align )
{
	const size_t bitCount = sizeof( align ) * 8;

    // assume math based on x86 bits.
    if ( std::bitset <bitCount> ( align ).count() == 1 )
    {
        //bitfield version. not compatible with non-bitfield alignments.
        return (((num) + (sector) - 1) & (~((align) - 1)));
    }
    else
    {
		return _ALIGN_GP( num, sector, align );
    }
}

template <typename numberType>
AINLINE numberType ALIGN( numberType num, numberType sector, numberType align )
{
	return _ALIGN_GP( num, sector, align );
}

// Optimized primitives.
template <> AINLINE char			ALIGN( char num, char sector, char align )								{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned char	ALIGN( unsigned char num, unsigned char sector, unsigned char align )	{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE short			ALIGN( short num, short sector, short align )							{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned short	ALIGN( unsigned short num, unsigned short sector, unsigned short align ){ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE int				ALIGN( int num, int sector, int align )									{ return _ALIGN_NATIVE( num, sector, align ); }
template <> AINLINE unsigned int	ALIGN( unsigned int num, unsigned int sector, unsigned int align )
{
	return (unsigned int)_ALIGN_NATIVE( (int)num, (int)sector, (int)align );
}

// Helper macro (equivalent of EXAMPLE 1)
template <typename numberType>
inline numberType ALIGN_SIZE( numberType num, numberType sector )
{
    return ( ALIGN( (num), (sector), (sector) ) );
}

// Definition of the offset type that accomodates for all kinds of files.
// Realistically speaking, the system is not supposed to have files that are bigger than
// this number type can handle.
// Use this number type whenever correct operations are required.
// REMEMBER: be sure to check that your compiler supports the number representation you want!
typedef long long int fsOffsetNumber_t;

// Types used to keep binary compatibility between compiler implementations and architectures.
// Binary compatibility is required in streams, so that reading streams is not influenced by
// compilation mode.
typedef bool fsBool_t;
typedef char fsChar_t;
typedef unsigned char fsUChar_t;
typedef short fsShort_t;
typedef unsigned short fsUShort_t;
typedef int fsInt_t;
typedef unsigned int fsUInt_t;
typedef long long int fsWideInt_t;
typedef unsigned long long int fsUWideInt_t;
typedef float fsFloat_t;
typedef double fsDouble_t;

// Compiler compatibility
#ifndef _MSC_VER
#define abstract
#endif //_MSC:VER

#ifdef __linux__
#include <sys/stat.h>

#define _strdup strdup
#define _vsnprintf vsnprintf
#endif //__linux__

typedef std::vector <filePath> dirTree;

enum eFileException
{
    FILE_STREAM_TERMINATED  // user attempts to perform on a terminated file stream, ie. http timeout
};

#endif //_FILESYSTEM_COMMON_DEFINITIONS_