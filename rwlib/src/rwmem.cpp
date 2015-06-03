#include "StdInc.h"

namespace rw
{

// General memory allocation routines.
// These should be used by the entire library.
void* Interface::MemAllocate( size_t memSize )
{
    return new uint8[ memSize ];
}

void Interface::MemFree( void *ptr )
{
    delete [] ptr;
}

void* Interface::PixelAllocate( size_t memSize )
{
    return new uint8[ memSize ];
}

void Interface::PixelFree( void *ptr )
{
    delete [] ptr;
}

};