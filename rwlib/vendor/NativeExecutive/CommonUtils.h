#ifndef _COMMON_UTILITIES_
#define _COMMON_UTILITIES_

#include <MemoryUtils.h>

class defaultGrowableArrayAllocator
{
public:
    inline void* Allocate( size_t memSize, unsigned int flags )
    {
        return malloc( memSize );
    }

    inline void* Realloc( void *memPtr, size_t memSize, unsigned int flags )
    {
        return realloc( memPtr, memSize );
    }

    inline void Free( void *memPtr )
    {
        free( memPtr );
    }
};

template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename arrayMan, typename countType>
struct growableArray : growableArrayEx <dataType, pulseCount, allocFlags, arrayMan, countType, defaultGrowableArrayAllocator>
{
};

template <typename dataType>
struct iterativeGrowableArrayManager
{
    AINLINE void InitField( dataType& theField )
    {
        return;
    }
};

template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename countType>
struct iterativeGrowableArray : growableArray <dataType, pulseCount, allocFlags, iterativeGrowableArrayManager <dataType>, countType>
{
};

#endif //_COMMON_UTILITIES_