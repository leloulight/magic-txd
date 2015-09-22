/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        Shared/core/MemoryUtils.h
*  PURPOSE:     Memory management templates
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _GLOBAL_MEMORY_UTILITIES_
#define _GLOBAL_MEMORY_UTILITIES_

#include <list>
#include "rwlist.hpp"
#include <CFileSystem.common.h>

template <typename numberType>
class InfiniteCollisionlessBlockAllocator
{
public:
    typedef sliceOfData <numberType> memSlice_t;

    struct block_t
    {
        RwListEntry <block_t> node;

        memSlice_t slice;
        numberType alignment;
    };

    RwList <block_t> blockList;

    typedef RwListEntry <block_t>* blockIter_t;

    struct allocInfo
    {
        memSlice_t slice;
        numberType alignment;
        blockIter_t blockToAppendAt;
    };

    inline InfiniteCollisionlessBlockAllocator( void )
    {
        LIST_CLEAR( blockList.root );
    }

    inline bool FindSpace( numberType sizeOfBlock, allocInfo& infoOut, const numberType alignmentWidth = sizeof( void* ) )
    {
        // Try to allocate memory at the first position we find.
        memSlice_t newAllocSlice( 0, sizeOfBlock );

        blockIter_t appendNode = &blockList.root;

        // Make sure we align to the system integer (by default).
        // todo: maybe this is not correct all the time?

        LIST_FOREACH_BEGIN( block_t, blockList.root, node )
            // Intersect the current memory slice with the ones on our list.
            const memSlice_t& blockSlice = item->slice;

            memSlice_t::eIntersectionResult intResult = newAllocSlice.intersectWith( blockSlice );

            if ( !memSlice_t::isFloatingIntersect( intResult ) )
            {
                // Advance the try memory offset.
                {
                    numberType tryMemPosition = blockSlice.GetSliceEndPoint() + 1;
                    
                    tryMemPosition = ALIGN( tryMemPosition, alignmentWidth, alignmentWidth );

                    newAllocSlice.SetSlicePosition( tryMemPosition );
                }

                // Set the append node further.
                appendNode = &item->node;
            }
            else
            {
                // Make sure we do not get behind a memory block.
                assert( intResult != memSlice_t::INTERSECT_FLOATING_END );
                break;
            }
        LIST_FOREACH_END

        infoOut.slice = newAllocSlice;
        infoOut.alignment = alignmentWidth;
        infoOut.blockToAppendAt = appendNode;
        return true;
    }

    inline void PutBlock( block_t *allocatedStruct, allocInfo& info )
    {
        allocatedStruct->slice = info.slice;
        allocatedStruct->alignment = info.alignment;

        LIST_INSERT( *info.blockToAppendAt, allocatedStruct->node );
    }

    inline void RemoveBlock( block_t *theBlock )
    {
        LIST_REMOVE( theBlock->node );
    }

    inline numberType GetSpanSize( void ) const
    {
        numberType theSpanSize = 0;

        if ( LIST_EMPTY( this->blockList.root ) == false )
        {
            block_t *lastBlockItem = LIST_GETITEM( block_t, this->blockList.root.prev, node );

            // Thankfully, we depend on the sorting based on memory order.
            // Getting the span size is very easy then, since the last item is automatically at the top most memory offset.
            // Since we always start at position zero, its okay to just take the end point.
            theSpanSize = ( lastBlockItem->slice.GetSliceEndPoint() + 1 );
        }

        return theSpanSize;
    }
    
    template <typename collisionConditionalType>
    struct conditionalRegionIterator
    {
        const InfiniteCollisionlessBlockAllocator *manager;
        collisionConditionalType& conditional;

    private:
        numberType removalByteCount;

        RwListEntry <block_t> *iter;

    public:
        inline conditionalRegionIterator( const InfiniteCollisionlessBlockAllocator *manager, collisionConditionalType& conditional ) : conditional( conditional )
        {
            this->manager = manager;

            this->removalByteCount = 0;

            this->iter = manager->blockList.root.next;
        }

        inline conditionalRegionIterator( const conditionalRegionIterator& right )
        {
            this->manager = right.manager;

            this->removalByteCount = right.removalByteCount;

            this->iter = right.iter;
        }

        inline void operator = ( const conditionalRegionIterator& right )
        {
            this->manager = right.manager;

            this->removalByteCount = right.removalByteCount;

            this->iter = right.iter;
        }

        inline void Increment( void )
        {
            RwListEntry <block_t> *curItem = this->iter;

            block_t *curBlock = NULL;

            if ( curItem != &this->manager->blockList.root )
            {
                curBlock = LIST_GETITEM( block_t, curItem, node );
            }

            do
            {
                RwListEntry <block_t> *nextItem = curItem->next;

                bool shouldBreak = false;

                block_t *nextBlock = NULL;

                if ( nextItem != &this->manager->blockList.root )
                {
                    nextBlock = LIST_GETITEM( block_t, nextItem, node );

                    if ( curBlock == NULL )
                    {
                        this->removalByteCount = 0;
                    }
                    else
                    {
                        if ( conditional.DoIgnoreBlock( curBlock ) )
                        {
                            numberType ignoreByteCount = ( nextBlock->slice.GetSliceStartPoint() - curBlock->slice.GetSliceStartPoint() );

                            this->removalByteCount += ignoreByteCount;
                        }
                        else
                        {
                            shouldBreak = true;
                        }
                    }
                }
                else
                {
                    shouldBreak = true;
                }

                curItem = nextItem;
                curBlock = nextBlock;

                if ( shouldBreak )
                {
                    break;
                }
            }
            while ( curItem != &this->manager->blockList.root );

            this->iter = curItem;
        }

        inline void Decrement( void )
        {
            RwListEntry <block_t> *curItem = this->iter;

            block_t *curBlock = LIST_GETITEM( block_t, curItem, node );

            do
            {
                RwListEntry <block_t> *prevItem = curItem->prev;

                bool shouldBreak = false;

                block_t *prevBlock = NULL;

                if ( prevItem != &this->manager->blockList.root )
                {
                    prevBlock = LIST_GETITEM( block_t, prevItem, node );

                    if ( curBlock == NULL )
                    {
                        // TODO annoying shit.
                        // basically I must restore the state as if the guy went through the list straight.
                        assert( 0 );
                    }
                    else
                    {
                        if ( conditional.DoIgnoreBlock( prevBlock ) )
                        {
                            numberType ignoreByteCount = ( curBlock->slice.GetSliceStartPoint() - prevBlock->slice.GetSliceStartPoint() );

                            this->removalByteCount -= ignoreByteCount;
                        }
                        else
                        {
                            shouldBreak = true;
                        }
                    }
                }
                else
                {
                    shouldBreak = true;
                }

                curItem = prevItem;
                curBlock = prevBlock;

                if ( shouldBreak )
                {
                    break;
                }
            }
            while ( curItem != &this->manager->blockList.root );

            this->iter = curItem;
        }

        inline bool IsEnd( void ) const
        {
            return ( this->iter == &this->manager->blockList.root );
        }

        inline bool IsNextEnd( void ) const
        {
            return ( this->iter->next == &this->manager->blockList.root );
        }

        inline bool IsPrevEnd( void ) const
        {
            return ( this->iter->prev == &this->manager->blockList.root );
        }

        inline block_t* ResolveBlock( void ) const
        {
            return LIST_GETITEM( block_t, this->iter, node );
        }

        inline numberType ResolveOffset( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            return ( curBlock->slice.GetSliceStartPoint() - this->removalByteCount );
        }

        inline numberType ResolveOffsetAfter( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            bool ignoreCurrentBlock = conditional.DoIgnoreBlock( curBlock );

            if ( ignoreCurrentBlock )
            {
                return ( curBlock->slice.GetSliceStartPoint() - this->removalByteCount );
            }

            return ( ( curBlock->slice.GetSliceEndPoint() + 1 ) - this->removalByteCount );
        }
    };

    // Accelerated conditional look-up routines, so that you can still easily get block offsets and span size when you want to ignore
    // certain blocks.
    template <typename collisionConditionalType>
    inline numberType GetSpanSizeConditional( collisionConditionalType& conditional ) const
    {
        conditionalRegionIterator <collisionConditionalType> iterator( this, conditional );

        bool hasItem = false;
        
        if ( iterator.IsEnd() == false )
        {
            hasItem = true;

            while ( iterator.IsNextEnd() == false )
            {
                iterator.Increment();
            }
        }

        // If the list of blocks is empty, ignore.
        if ( hasItem == false )
        {
            return 0;
        }

        return iterator.ResolveOffsetAfter();
    }

    template <typename collisionConditionalType>
    inline bool GetBlockOffsetConditional( const block_t *theBlock, numberType& outOffset, collisionConditionalType& conditional ) const
    {
        // If the block that we request the offset of should be ignored anyway, we will simply return false.
        if ( conditional.DoIgnoreBlock( theBlock ) )
        {
            return false;
        }

        conditionalRegionIterator <collisionConditionalType> iterator( this, conditional );

        bool hasFoundTheBlock = false;

        while ( iterator.IsEnd() == false )
        {
            // We terminate if we found our block.
            if ( iterator.ResolveBlock() == theBlock )
            {
                hasFoundTheBlock = true;
                break;
            }

            iterator.Increment();
        }

        if ( hasFoundTheBlock == false )
        {
            // This should never happen.
            return false;
        }

        // Return the actual offset that preserves alignment.
        outOffset = iterator.ResolveOffset();
        return true;
    }

    // This is a very optimized algorithm for turning a static-allocation-offset into its conditional equivalent.
    template <typename collisionConditionalType>
    inline bool ResolveConditionalBlockOffset( numberType staticBlockOffset, numberType& outOffset, collisionConditionalType& conditional ) const
    {
        // If the block that we request the offset of should be ignored anyway, we will simply return false.
        if ( conditional.DoIgnoreBlock( theBlock ) )
        {
            return false;
        }

        conditionalRegionIterator <collisionConditionalType> iterator( this, conditional );

        bool hasFoundTheBlock = false;

        while ( iterator.IsEnd() == false )
        {
            // We terminate if we found our block.
            const block_t *theBlock = iterator.ResolveBlock();

            numberType thisBlockOffset = theBlock->slice.GetSliceStartPoint();

            if ( thisBlockOffset == staticBlockOffset )
            {
                hasFoundTheBlock = true;
                break;
            }
            else if ( thisBlockOffset > staticBlockOffset )
            {
                // We have not found it.
                // Terminate early.
                break;
            }

            iterator.Increment();
        }

        if ( hasFoundTheBlock == false )
        {
            // This should never happen.
            return false;
        }

        // Return the actual offset that preserves alignment.
        outOffset = iterator.ResolveOffset();
        return true;
    }
};

template <size_t memorySize>
class StaticMemoryAllocator
{
    typedef InfiniteCollisionlessBlockAllocator <size_t> blockAlloc_t;

    typedef blockAlloc_t::memSlice_t memSlice_t;

    blockAlloc_t blockRegions;
public:

    AINLINE StaticMemoryAllocator( void ) : validAllocationRegion( 0, memorySize )
    {
#ifdef _DEBUG
        memset( memData, 0, memorySize );
#endif
    }

    AINLINE ~StaticMemoryAllocator( void )
    {
        return;
    }

    AINLINE bool IsValidPointer( void *ptr )
    {
        return ( ptr >= memData && ptr <= ( memData + sizeof( memData ) ) );
    }

    AINLINE void* Allocate( size_t memSize )
    {
        // We cannot allocate zero size.
        if ( memSize == 0 )
            return NULL;

        // Get the actual mem block size.
        size_t actualMemSize = ( memSize + sizeof( memoryEntry ) );

        blockAlloc_t::allocInfo allocInfo;

        bool hasSpace = blockRegions.FindSpace( actualMemSize, allocInfo );

        // The space allocation could fail if there is not enough space given by the size_t type.
        // This is very unlikely to happen, but must be taken care of.
        if ( hasSpace == false )
            return NULL;

        // Make sure we allocate in the valid region.
        {
            memSlice_t::eIntersectionResult intResult = allocInfo.slice.intersectWith( validAllocationRegion );

            if ( intResult != memSlice_t::INTERSECT_EQUAL &&
                 intResult != memSlice_t::INTERSECT_INSIDE )
            {
                return NULL;
            }
        }

        // Create the allocation information structure and return it.
        memoryEntry *entry = (memoryEntry*)( (char*)memData + allocInfo.slice.GetSliceStartPoint() );

        entry->blockSize = memSize;

        // Insert into the block manager.
        blockRegions.PutBlock( entry, allocInfo );

        return entry->GetData();
    }

    AINLINE void Free( void *ptr )
    {
        // Make sure this structure is a valid pointer from our heap.
        assert( IsValidPointer( ptr ) == true );

        // Remove the structure from existance.
        memoryEntry *entry = (memoryEntry*)ptr - 1;

        blockRegions.RemoveBlock( entry );
    }

private:
    blockAlloc_t::memSlice_t validAllocationRegion;

    struct memoryEntry : public blockAlloc_t::block_t
    {
        inline void* GetData( void )
        {
            return this + 1;
        }

        size_t blockSize;
    };

    char memData[ memorySize ];
};

// Struct registry flavor is used to fine-tune the performance of said registry by extending it with features.
// The idea is that we maximize performance by only giving it the features you really want it to have.
template <typename abstractionType>
struct cachedMinimalStructRegistryFlavor
{
    size_t pluginAllocSize;

    cachedMinimalStructRegistryFlavor( void )
    {
        // Reset to zero as we have no plugins allocated.
        this->pluginAllocSize = 0;
    }

    template <typename managerType>
    inline bool GetPluginStructOffset( managerType *manPtr, size_t handleOffset, size_t& actualOffset ) const
    {
        actualOffset = handleOffset;
        return true;
    }

    template <typename managerType>
    inline bool GetPluginStructOffsetByObject( managerType *manPtr, const abstractionType *object, size_t handleOffset, size_t& actualOffset ) const
    {
        actualOffset = handleOffset;
        return true;
    }

    template <typename managerType>
    inline size_t GetPluginAllocSize( managerType *manPtr ) const
    {
        return this->pluginAllocSize;
    }

    template <typename managerType>
    inline size_t GetPluginAllocSizeByObject( managerType *manPtr, const abstractionType *object ) const
    {
        return this->pluginAllocSize;
    }

    template <typename managerType>
    inline void UpdatePluginRegion( managerType *manPtr )
    {
        // Update the overall class size.
        // It is determined by the end of this plugin struct.
        this->pluginAllocSize = manPtr->pluginRegions.GetSpanSize();    // often it will just be ( useOffset + pluginSize ), but we must not rely on that.
    }

    inline static bool DoesUseUnifiedPluginOffset( void )
    {
        return true;
    }

    template <typename managerType>
    struct pluginInterfaceBase
    {
        // Nothing really.
    };

    template <typename managerType>
    struct regionIterator
    {
        typedef typename managerType::blockAlloc_t::block_t block_t;

    private:
        managerType *manPtr;

        RwListEntry <block_t> *iterator;

    public:
        inline regionIterator( managerType *manPtr )
        {
            this->manPtr = manPtr;

            this->iterator = manPtr->pluginRegions.blockList.root.next;
        }

        inline regionIterator( const regionIterator& right )
        {
            this->manPtr = right.manPtr;
        }

        inline void operator = ( const regionIterator& right )
        {
            this->manPtr = right.manPtr;
        }

        inline void Increment( void )
        {
            this->iterator = this->iterator->next;
        }

        inline void Decrement( void )
        {
            this->iterator = this->iterator->prev;
        }

        inline bool IsEnd( void ) const
        {
            return ( this->iterator == &this->manPtr->pluginRegions.blockList.root );
        }

        inline bool IsNextEnd( void ) const
        {
            return ( this->iterator->next == &this->manPtr->pluginRegions.blockList.root );
        }

        inline bool IsPrevEnd( void ) const
        {
            return ( this->iterator->prev == &this->manPtr->pluginRegions.blockList.root );
        }

        inline block_t* ResolveBlock( void ) const
        {
            return LIST_GETITEM( block_t, this->iterator, node );
        }

        inline size_t ResolveOffset( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            return ( curBlock->slice.GetSliceStartPoint() );
        }

        inline size_t ResolveOffsetAfter( void ) const
        {
            block_t *curBlock = this->ResolveBlock();

            return ( curBlock->slice.GetSliceEndPoint() + 1 );
        }
    };
};

template <typename abstractionType>
struct conditionalStructRegistryFlavor
{
    conditionalStructRegistryFlavor( void )
    {
        return;
    }

    template <typename managerType>
    struct runtimeBlockConditional
    {
    private:
        const managerType *manPtr;

    public:
        inline runtimeBlockConditional( const managerType *manPtr )
        {
            this->manPtr = manPtr;
        }

        inline bool DoIgnoreBlock( typename managerType::blockAlloc_t::block_t *theBlock ) const
        {
            // The given block is always a plugin registration, so cast it appropriately.
            typename managerType::registered_plugin *pluginDesc = (typename managerType::registered_plugin*)theBlock;

            pluginInterfaceBase <managerType> *pluginInterface = (pluginInterfaceBase <managerType>*)pluginDesc->descriptor;

            // Lets just ask the vendor, whether he wants the block.
            bool isAvailable = pluginInterface->IsPluginAvailableDuringRuntime( pluginDesc->pluginId );

            // We ignore a block registration if it is not available.
            return ( isAvailable == false );
        }
    };

    template <typename managerType>
    struct regionIterator
    {
    private:
        typedef runtimeBlockConditional <managerType> usedConditional;

        usedConditional conditional;
        typename managerType::blockAlloc_t::template conditionalRegionIterator <usedConditional> iterator;

    public:
        typedef typename managerType::blockAlloc_t::block_t block_t;

        inline regionIterator( const managerType *manPtr ) : conditional( manPtr ), iterator( &manPtr->pluginRegions, conditional )
        {
            return;
        }

        inline regionIterator( const regionIterator& right ) : conditional( right.conditional ), iterator( right.iterator )
        {
            return;
        }

        inline void operator = ( const regionIterator& right )
        {
            this->conditional = right.conditional;
            this->iterator = right.iterator;
        }

        inline void Increment( void )
        {
            this->iterator.Increment();
        }

        inline void Decrement( void )
        {
            this->iterator.Decrement();
        }

        inline bool IsEnd( void ) const
        {
            return this->iterator.IsEnd();
        }

        inline bool IsNextEnd( void ) const
        {
            return this->iterator.IsNextEnd();
        }

        inline bool IsPrevEnd( void ) const
        {
            return this->iterator.IsPrevEnd();
        }

        inline block_t* ResolveBlock( void ) const
        {
            return this->iterator.ResolveBlock();
        }

        inline size_t ResolveOffset( void ) const
        {
            return this->iterator.ResolveOffset();
        }

        inline size_t ResolveOffsetAfter( void ) const
        {
            return this->iterator.ResolveOffsetAfter();
        }
    };

    template <typename managerType>
    inline size_t GetPluginAllocSize( managerType *manPtr ) const
    {
        runtimeBlockConditional <managerType> conditional( manPtr );

        return manPtr->pluginRegions.GetSpanSizeConditional( conditional );
    }

    template <typename managerType>
    struct objectBasedBlockConditional
    {
    private:
        managerType *manPtr;
        const abstractionType *aliveObject;

    public:
        inline objectBasedBlockConditional( managerType *manPtr, const abstractionType *aliveObject )
        {
            this->manPtr = manPtr;
            this->aliveObject = aliveObject;
        }

        inline bool DoIgnoreBlock( typename managerType::blockAlloc_t::block_t *theBlock ) const
        {
            // TODO.
            return false;
        }
    };

    template <typename managerType>
    inline size_t GetPluginAllocSizeByObject( managerType *manPtr, const abstractionType *object ) const
    {
        objectBasedBlockConditional <managerType> conditional( manPtr, object );

        return manPtr->pluginRegions.GetSpanSizeConditional( conditional );
    }

    template <typename managerType>
    inline bool GetPluginStructOffset( managerType *manPtr, size_t handleOffset, size_t& actualOffset ) const
    {
        runtimeBlockConditional <managerType> conditional( manPtr );

        return manPtr->pluginRegions.ResolveConditionalBlockOffset( handleOffset, actualOffset, conditional );
    }

    template <typename managerType>
    inline bool GetPluginStructOffsetByObject( managerType *manPtr, const abstractionType *object, size_t handleOffset, size_t& actualOffset ) const
    {
        objectBasedBlockConditional <managerType> conditional( manPtr, object );

        return manPtr->pluginRegions.ResolveConditionalBlockOffset( handleOffset, actualOffset, conditional );
    }

    template <typename managerType>
    inline void UpdatePluginRegion( managerType *manPtr )
    {
        // Whatever.
    }

    inline static bool DoesUseUnifiedPluginOffset( void )
    {
        return false;
    }

    template <typename managerType>
    struct pluginInterfaceBase
    {
        typedef typename managerType::pluginDescriptorType pluginDescriptorType;

        // Conditional interface. This is based on the state of the runtime.
        virtual bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const
        {
            return true;
        }

        virtual bool IsPluginAvailableAtObject( abstractionType *object, pluginDescriptorType pluginId ) const
        {
            // Make sure that the functionality of IsPluginAvailableDuringRuntime and IsPluginAvailableAtObject logically match.
            // Basically, object must store a snapshot of the runtime state and keep it immutable, while the runtime state
            // (and by that, the type layout) may change at any time. This is best stored by some kind of bool variable.
            return true;
        }
    };
};

// Class used to register anonymous structs that can be placed on top of a C++ type.
template <typename abstractionType, typename pluginDescriptorType, typename pluginAvailabilityDispatchType, typename... AdditionalArgs>
struct AnonymousPluginStructRegistry
{
    typedef typename pluginDescriptorType pluginDescriptorType;

    // A plugin struct registry is based on an infinite range of memory that can be allocated on, like a heap.
    // The structure of this memory heap is then applied onto the underlying type.
    typedef InfiniteCollisionlessBlockAllocator <size_t> blockAlloc_t;

    blockAlloc_t pluginRegions;

    typedef typename pluginDescriptorType::pluginOffset_t pluginOffset_t;

    pluginOffset_t preferredAlignment;

    inline AnonymousPluginStructRegistry( void )
    {
        this->preferredAlignment = (pluginOffset_t)4;
    }

    inline ~AnonymousPluginStructRegistry( void )
    {
        // TODO: allow custom memory allocators.
        return;
    }

    // Oh my fucking god. "template" can be used other than declaring a templated type?
    // Why does C++ not make it a job for the compiler to determine things... I mean it would be possible!
    // You cannot know what a pain in the head it is to find the solution to add the "template" keyword.
    // Bjarne, seriously.
    typedef typename pluginAvailabilityDispatchType::template pluginInterfaceBase <AnonymousPluginStructRegistry> pluginInterfaceBase;

    // Virtual interface used to describe plugin classes.
    // The construction process must be immutable across the runtime.
    struct pluginInterface : public pluginInterfaceBase
    {
        virtual ~pluginInterface( void )            {}

        virtual bool OnPluginConstruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // By default, construction of plugins should succeed.
            return true;
        }

        virtual void OnPluginDestruct( abstractionType *object, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            return;
        }

        virtual bool OnPluginAssign( abstractionType *dstObject, const abstractionType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... )
        {
            // Assignment of data to another plugin struct is optional.
            return false;
        }

        virtual void DeleteOnUnregister( void )
        {
            // Overwrite this method if unregistering should delete this class.
            return;
        }
    };

    // Struct that holds information about registered plugins.
    struct registered_plugin : public blockAlloc_t::block_t
    {
        size_t pluginSize;
        pluginDescriptorType pluginId;
        pluginOffset_t pluginOffset;
        pluginInterface *descriptor;
    };

    // Container that holds plugin information.
    // TODO: Using STL types is crap (?). Use a custom type instead!
    typedef std::list <registered_plugin> registeredPlugins_t;

    registeredPlugins_t regPlugins;

    // Holds things like the way to determine the size of all plugins.
    pluginAvailabilityDispatchType regBoundFlavor;

    inline size_t GetPluginSizeByRuntime( void ) const
    {
        return this->regBoundFlavor.GetPluginAllocSize( this );
    }

    inline size_t GetPluginSizeByObject( const abstractionType *object ) const
    {
        return this->regBoundFlavor.GetPluginAllocSizeByObject( this, object );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, const pluginDescriptorType& pluginId, pluginInterface *plugInterface )
    {
        // Make sure we have got valid parameters passed.
        if ( pluginSize == 0 || plugInterface == NULL )
            return 0;   // TODO: fix this crap, 0 is ambivalent since its a valid index!

        // Determine the plugin offset that should be used for allocation.
        blockAlloc_t::allocInfo blockAllocInfo;

        bool hasSpace = pluginRegions.FindSpace( pluginSize, blockAllocInfo, this->preferredAlignment );

        // Handle obscure errors.
        if ( hasSpace == false )
            return 0;

        // The beginning of the free space is where our plugin should be placed at.
        pluginOffset_t useOffset = blockAllocInfo.slice.GetSliceStartPoint();

        // Register ourselves.
        registered_plugin info;
        info.pluginSize = pluginSize;
        info.pluginId = pluginId;
        info.pluginOffset = useOffset;
        info.descriptor = plugInterface;
        
        regPlugins.push_back( info );

        // Register the pointer to the registered plugin.
        pluginRegions.PutBlock( &regPlugins.back(), blockAllocInfo );

        // Notify the flavor to update.
        this->regBoundFlavor.UpdatePluginRegion( this );

        return useOffset;
    }

    inline bool UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        bool hasDeleted = false;

        // Get the plugin information that represents this plugin offset.
        for ( registeredPlugins_t::iterator iter = regPlugins.begin(); iter != regPlugins.end(); iter++ )
        {
            registered_plugin& thePlugin = *iter;

            if ( thePlugin.pluginOffset == pluginOffset )
            {
                // We found it!
                // Now remove it and (probably) delete it.
                thePlugin.descriptor->DeleteOnUnregister();

                pluginRegions.RemoveBlock( &thePlugin );

                regPlugins.erase( iter );

                hasDeleted = true;
                break;  // there can be only one.
            }
        }

        if ( hasDeleted )
        {
            // Since we really have deleted, we need to readjust our struct memory size.
            // This is done by getting the span size of the plugin allocation region, which is a very fast operation!
            this->regBoundFlavor.UpdatePluginRegion( this );
        }

        return hasDeleted;
    }

private:
    typedef typename pluginAvailabilityDispatchType::template regionIterator <AnonymousPluginStructRegistry> regionIterator_t;

    inline void DestroyPluginBlockInternal( abstractionType *pluginObj, regionIterator_t& iter, AdditionalArgs... addArgs )
    {
        try
        {
            while ( iter.IsPrevEnd() == false )
            {
                iter.Decrement();

                const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

                const registered_plugin *constructedInfo = (const registered_plugin*)blockItem;

                // Destroy that plugin again.
                constructedInfo->descriptor->OnPluginDestruct(
                    pluginObj,
                    iter.ResolveOffset(),
                    constructedInfo->pluginId,
                    addArgs...
                );
            }
        }
        catch( ... )
        {
            // We must not fail destruction, in any way.
            assert( 0 );
        }
    }

public:
    inline bool ConstructPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs )
    {
        // Construct all plugins.
        bool pluginConstructionSuccessful = true;

        regionIterator_t iter( this );

        try
        {
            for ( ; iter.IsEnd() == false; iter.Increment() )
            {
                const blockAlloc_t::block_t *blockItem = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockItem;

                // TODO: add dispatch based on the reg bound flavor.
                // it should say whether this plugin exists and where it is ultimatively located.
                // in the cached handler, this is an O(1) operation, in the conditional it is rather funky.
                // this is why you really should not use the conditional handler too often.

                bool success =
                    pluginInfo->descriptor->OnPluginConstruct(
                        pluginObj,
                        iter.ResolveOffset(),
                        pluginInfo->pluginId,
                        addArgs...
                    );

                if ( !success )
                {
                    pluginConstructionSuccessful = false;
                    break;
                }
            }
        }
        catch( ... )
        {
            // There was an exception while trying to construct a plugin.
            // We do not let it pass and terminate here.
            pluginConstructionSuccessful = false;
        }

        if ( !pluginConstructionSuccessful )
        {
            // The plugin failed to construct, so destroy all plugins that
            // constructed up until that point.
            DestroyPluginBlockInternal( pluginObj, iter, addArgs... );
        }

        return pluginConstructionSuccessful;
    }

    inline bool AssignPluginBlock( abstractionType *dstPluginObj, const abstractionType *srcPluginObj, AdditionalArgs... addArgs )
    {
        // Call all assignment operators.
        bool cloneSuccess = true;

        regionIterator_t iter( this );

        try
        {
            for ( ; !iter.IsEnd(); iter.Increment() )
            {
                const blockAlloc_t::block_t *blockInfo = iter.ResolveBlock();

                const registered_plugin *pluginInfo = (const registered_plugin*)blockInfo;

                bool assignSuccess = pluginInfo->descriptor->OnPluginAssign(
                    dstPluginObj, srcPluginObj,
                    iter.ResolveOffset(),
                    pluginInfo->pluginId,
                    addArgs...
                );

                if ( !assignSuccess )
                {
                    cloneSuccess = false;
                    break;
                }
            }
        }
        catch( ... )
        {
            // There was an exception while cloning plugin data.
            // We do not let it pass and terminate here.
            cloneSuccess = false;
        }

        return cloneSuccess;
    }

    inline void DestroyPluginBlock( abstractionType *pluginObj, AdditionalArgs... addArgs )
    {
        // Call destructors of all registered plugins.
        regionIterator_t endIter( this );

#if 0
        // By decrementing this double-linked-list iterator by one, we get to the end.
        endIter.Decrement();
#else
        // We want to iterate to the end.
        while ( endIter.IsEnd() == false )
        {
            endIter.Increment();
        }
#endif

        DestroyPluginBlockInternal( pluginObj, endIter, addArgs... );
    }

    // Use this function whenever you receive a handle offset to a plugin struct.
    // It is optimized so that you cannot go wrong.
    inline pluginOffset_t ResolvePluginStructOffsetByRuntime( pluginOffset_t handleOffset )
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffset( this, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }

    inline pluginOffset_t ResolvePluginStructOffsetByObject( const abstractionType *obj, pluginOffset_t handleOffset )
    {
        size_t theActualOffset;

        bool gotOffset = this->regBoundFlavor.GetPluginStructOffsetByObject( this, obj, handleOffset, theActualOffset );

        return ( gotOffset ? theActualOffset : 0 );
    }
};

// Helper struct for common plugin system functions.
// THREAD-SAFE because this class itself is immutable and the systemType is THREAD-SAFE.
template <typename classType, typename systemType, typename pluginDescriptorType, typename... AdditionalArgs>
struct CommonPluginSystemDispatch
{
    systemType& sysType;

    typedef typename systemType::pluginOffset_t pluginOffset_t;

    inline CommonPluginSystemDispatch( systemType& sysType ) : sysType( sysType )
    {
        return;
    }

    template <typename interfaceType>
    inline pluginOffset_t RegisterCommonPluginInterface( interfaceType *plugInterface, size_t structSize, const pluginDescriptorType& pluginId )
    {
        pluginOffset_t pluginOffset = 0;

        if ( plugInterface )
        {
            // Register our plugin!
            pluginOffset = sysType.RegisterPlugin(
                structSize, pluginId,
                plugInterface
            );

            // Delete our interface again if the plugin offset is invalid.
            if ( !systemType::IsOffsetValid( pluginOffset ) )
            {
                delete plugInterface;
            }
        }
        return pluginOffset;
    }

    // Helper functions used to create common plugin templates.
    template <typename structType>
    inline pluginOffset_t RegisterStructPlugin( const pluginDescriptorType& pluginId )
    {
        struct structPluginInterface : systemType::pluginInterface
        {
            bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                void *structMem = pluginId.RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                if ( structMem == NULL )
                    return false;

                // Construct the struct!
                structType *theStruct = new (structMem) structType;

                return ( theStruct != NULL );
            }

            void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                structType *theStruct = pluginId.RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

                // Destruct the struct!
                theStruct->~structType();
            }

            bool OnPluginAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
            {
                // To an assignment operation.
                structType *dstStruct = pluginId.RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
                const structType *srcStruct = pluginId.RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

                *dstStruct = *srcStruct;
                return true;
            }

            void DeleteOnUnregister( void ) override
            {
                delete this;
            }
        };

        // Create the interface that should handle our plugin.
        structPluginInterface *plugInterface = new structPluginInterface();

        return RegisterCommonPluginInterface( plugInterface, sizeof( structType ), pluginId );
    }

    template <typename structType>
    struct dependantStructPluginInterface : systemType::pluginInterface
    {
        bool OnPluginConstruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            void *structMem = pluginId.RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            if ( structMem == NULL )
                return false;

            // Construct the struct!
            structType *theStruct = new (structMem) structType;

            if ( theStruct )
            {
                try
                {
                    // Initialize the manager.
                    theStruct->Initialize( obj );
                }
                catch( ... )
                {
                    // We have to destroy our struct again.
                    theStruct->~structType();

                    throw;
                }
            }

            return ( theStruct != NULL );
        }

        void OnPluginDestruct( classType *obj, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            structType *theStruct = pluginId.RESOLVE_STRUCT <structType> ( obj, pluginOffset, addArgs... );

            // Deinitialize the manager.
            theStruct->Shutdown( obj );

            // Destruct the struct!
            theStruct->~structType();
        }

        bool OnPluginAssign( classType *dstObject, const classType *srcObject, pluginOffset_t pluginOffset, pluginDescriptorType pluginId, AdditionalArgs... addArgs ) override
        {
            // To an assignment operation.
            structType *dstStruct = pluginId.RESOLVE_STRUCT <structType> ( dstObject, pluginOffset, addArgs... );
            const structType *srcStruct = pluginId.RESOLVE_STRUCT <structType> ( srcObject, pluginOffset, addArgs... );

            *dstStruct = *srcStruct;
            return true;
        }

        void DeleteOnUnregister( void ) override
        {
            delete this;
        }
    };

    template <typename structType>
    inline pluginOffset_t RegisterDependantStructPlugin( const pluginDescriptorType& pluginId, size_t structSize = sizeof(structType) )
    {
        typedef dependantStructPluginInterface <structType> structPluginInterface;

        // Create the interface that should handle our plugin.
        structPluginInterface *plugInterface = new structPluginInterface();

        return RegisterCommonPluginInterface( plugInterface, structSize, pluginId );
    }

    struct conditionalPluginStructInterface abstract
    {
        // Conditional interface. This is based on the state of the runtime.
        // Both functions here have to match logically.
        virtual bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const = 0;
        virtual bool IsPluginAvailableAtObject( classType *object, pluginDescriptorType pluginId ) const = 0;
    };

    // Helper to register a conditional type that acts as dependant struct.
    template <typename structType>
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( const pluginDescriptorType& pluginId, conditionalPluginStructInterface *conditional, size_t structSize = sizeof(structType) )
    {
        struct structPluginInterface : public dependantStructPluginInterface <structType>
        {
            bool IsPluginAvailableDuringRuntime( pluginDescriptorType pluginId ) const
            {
                return conditional->IsPluginAvailableDuringRuntime( pluginId );
            }

            bool IsPluginAvailableAtObject( classType *object, pluginDescriptorType pluginId ) const
            {
                return conditional->IsPluginAvailableAtObject( object, pluginId );
            }

            void DeleteOnUnregister( void )
            {
                // Delete our data.
                //TODO

                dependantStructPluginInterface <structType>::DeleteOnUnregister();
            }

            conditionaPluginStructInterface *conditional;
        };

        // Create the interface that should handle our plugin.
        structPluginInterface *plugInterface = new structPluginInterface();

        if ( plugInterface )
        {
            plugInterface->conditional = conditional;

            return RegisterCommonPluginInterface( plugInterface, structSize, pluginId );
        }

        return 0;
    }
};

// Static plugin system that constructs classes that can be extended at runtime.
// This one is inspired by the RenderWare plugin system.
// This container is NOT MULTI-THREAD SAFE.
// All operations are expected to be ATOMIC.
template <typename classType, typename flavorType = cachedMinimalStructRegistryFlavor <classType>>
struct StaticPluginClassFactory
{
    typedef classType hostType_t;

    static const unsigned int ANONYMOUS_PLUGIN_ID = 0xFFFFFFFF;

    unsigned int aliveClasses;

    inline StaticPluginClassFactory( void )
    {
        aliveClasses = 0;
    }

    inline ~StaticPluginClassFactory( void )
    {
        assert( aliveClasses == 0 );
    }

    // Number type used to store the plugin offset.
    typedef ptrdiff_t pluginOffset_t;

    // Helper functoid.
    struct pluginDescriptor
    {
        typedef pluginOffset_t pluginOffset_t;

        inline pluginDescriptor( void )
        {
            this->pluginId = StaticPluginClassFactory::ANONYMOUS_PLUGIN_ID;
        }

        inline pluginDescriptor( unsigned int pluginId )
        {
            this->pluginId = pluginId;
        }

        operator const unsigned int& ( void ) const
        {
            return this->pluginId;
        }

        template <typename pluginStructType>
        AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <pluginStructType> ( object, offset );
        }

        template <typename pluginStructType>
        AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset )
        {
            return StaticPluginClassFactory::RESOLVE_STRUCT <const pluginStructType> ( object, offset );
        }

        unsigned int pluginId;
    };

    typedef AnonymousPluginStructRegistry <classType, pluginDescriptor, flavorType> structRegistry_t;

    structRegistry_t structRegistry;

    // Localize certain plugin registry types.
    typedef typename structRegistry_t::pluginInterface pluginInterface;

    static const pluginOffset_t INVALID_PLUGIN_OFFSET = (pluginOffset_t)-1;

    AINLINE static bool IsOffsetValid( pluginOffset_t offset )
    {
        return ( offset != INVALID_PLUGIN_OFFSET );
    }

    template <typename pluginStructType>
    AINLINE static pluginStructType* RESOLVE_STRUCT( classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return NULL;

        return (pluginStructType*)( (char*)object + sizeof( classType ) + offset );
    }

    template <typename pluginStructType>
    AINLINE static const pluginStructType* RESOLVE_STRUCT( const classType *object, pluginOffset_t offset )
    {
        if ( IsOffsetValid( offset ) == false )
            return NULL;

        return (const pluginStructType*)( (const char*)object + sizeof( classType ) + offset );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, unsigned int pluginId, pluginInterface *plugInterface )
    {
        assert( this->aliveClasses == 0 );

        return structRegistry.RegisterPlugin( pluginSize, pluginId, plugInterface );
    }

    inline void UnregisterPlugin( pluginOffset_t pluginOffset )
    {
        assert( this->aliveClasses == 0 );

        structRegistry.UnregisterPlugin( pluginOffset );
    }

    typedef CommonPluginSystemDispatch <classType, StaticPluginClassFactory, pluginDescriptor> functoidHelper_t;

    // Helper functions used to create common plugin templates.
    template <typename structType>
    inline pluginOffset_t RegisterStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID )
    {
        return functoidHelper_t( *this ).RegisterStructPlugin <structType> ( pluginId );
    }

    template <typename structType>
    inline pluginOffset_t RegisterDependantStructPlugin( unsigned int pluginId = ANONYMOUS_PLUGIN_ID, size_t structSize = sizeof( structType ) )
    {
        return functoidHelper_t( *this ).RegisterDependantStructPlugin <structType> ( pluginId, structSize );
    }

    // Note that this function only guarrantees to return an object size that is correct at this point in time.
    inline size_t GetClassSize( void ) const
    {
        return ( sizeof( classType ) + this->structRegistry.GetPluginSizeByRuntime() );
    }

private:
    inline void DestroyBaseObject( classType *toBeDestroyed )
    {
        try
        {
            toBeDestroyed->~classType();
        }
        catch( ... )
        {
            // Throwing exceptions from destructors is lethal.
            // We have to notify the developer about this.
            assert( 0 );
        }
    }

public:
    template <typename constructorType>
    inline classType* ConstructPlacementEx( void *classMem, const constructorType& constructor )
    {
        classType *resultObject = NULL;
        {
            classType *intermediateClassObject = NULL;

            try
            {
                intermediateClassObject = constructor.Construct( classMem );
            }
            catch( ... )
            {
                // The base object failed to construct, so terminate here.
                intermediateClassObject = NULL;
            }

            if ( intermediateClassObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( intermediateClassObject );

                if ( pluginConstructionSuccessful )
                {
                    // We succeeded, so return our pointer.
                    // We promote it to a real class object.
                    resultObject = intermediateClassObject;
                }
                else
                {
                    // Else we cannot keep the intermediate class object anymore.
                    DestroyBaseObject( intermediateClassObject );
                }
            }
        }

        if ( resultObject )
        {
            this->aliveClasses++;
        }

        return resultObject;
    }

    template <typename allocatorType, typename constructorType>
    inline classType* ConstructTemplate( allocatorType& memAllocator, const constructorType& constructor )
    {
        // Attempt to allocate the necessary memory.
        const size_t baseClassSize = sizeof( classType );
        const size_t wholeClassSize = this->GetClassSize();

        void *classMem = memAllocator.Allocate( wholeClassSize );

        if ( !classMem )
            return NULL;

        classType *resultObj = ConstructPlacementEx( classMem, constructor );

        if ( !resultObj )
        {
            // Clean up.
            memAllocator.Free( classMem, wholeClassSize );
        }

        return resultObj;
    }

    template <typename constructorType>
    inline classType* ClonePlacementEx( void *classMem, const classType *srcObject, const constructorType& constructor )
    {
        classType *clonedObject = NULL;
        {
            // Construct a basic class where we assign stuff to.
            classType *dstObject = NULL;

            try
            {
                dstObject = constructor.CopyConstruct( classMem, srcObject );
            }
            catch( ... )
            {
                dstObject = NULL;
            }

            if ( dstObject )
            {
                bool pluginConstructionSuccessful = structRegistry.ConstructPluginBlock( dstObject );

                if ( pluginConstructionSuccessful )
                {
                    bool cloneSuccess = structRegistry.AssignPluginBlock( dstObject, srcObject );

                    if ( cloneSuccess )
                    {
                        clonedObject = dstObject;
                    }

                    if ( clonedObject == NULL )
                    {
                        structRegistry.DestroyPluginBlock( dstObject );
                    }
                }
                
                if ( clonedObject == NULL )
                {
                    // Since cloning plugin data has not succeeded, we have to destroy the constructed base object again.
                    // Make sure that we do not throw exceptions.
                    DestroyBaseObject( dstObject );

                    dstObject = NULL;
                }
            }
        }

        if ( clonedObject )
        {
            this->aliveClasses++;
        }

        return clonedObject;
    }

    template <typename allocatorType, typename constructorType>
    inline classType* CloneTemplate( allocatorType& memAllocator, const classType *srcObject, const constructorType& constructor )
    {
        // Attempt to allocate the necessary memory.
        const size_t baseClassSize = sizeof( classType );
        const size_t wholeClassSize = this->GetClassSize();

        void *classMem = memAllocator.Allocate( wholeClassSize );

        if ( !classMem )
            return NULL;

        classType *clonedObject = ClonePlacementEx( classMem, srcObject, constructor );

        if ( clonedObject == NULL )
        {
            memAllocator.Free( classMem );
        }

        return clonedObject;
    }

    struct basicClassConstructor
    {
        inline classType* Construct( void *mem ) const
        {
            return new (mem) classType;
        }

        inline classType* CopyConstruct( void *mem, const classType *srcMem ) const
        {
            return new (mem) classType( *srcMem );
        }
    };

    template <typename allocatorType>
    inline classType* Construct( allocatorType& memAllocator )
    {
        basicClassConstructor constructor;

        return ConstructTemplate( memAllocator, constructor );
    }

    inline classType* ConstructPlacement( void *memPtr )
    {
        basicClassConstructor constructor;

        return ConstructPlacementEx( memPtr, constructor );
    }

    template <typename allocatorType>
    inline classType* Clone( allocatorType& memAllocator, const classType *srcObject )
    {
        basicClassConstructor constructor;

        return CloneTemplate( memAllocator, srcObject, constructor );
    }

    inline classType* ClonePlacement( void *memPtr, const classType *srcObject )
    {
        basicClassConstructor constructor;

        return ClonePlacementEx( memPtr, srcObject, constructor );
    }

    inline void DestroyPlacement( classType *classObject )
    {
        // Destroy plugin data first.
        structRegistry.DestroyPluginBlock( classObject );

        try
        {
            // Destroy the base class object.
            classObject->~classType();
        }
        catch( ... )
        {
            // There was an exception while destroying the base class.
            // This must not happen either; we have to notify the guys!
            assert( 0 );
        }

        // Decrease the number of alive classes.
        this->aliveClasses--;
    }

    template <typename allocatorType>
    inline void Destroy( allocatorType& memAllocator, classType *classObject )
    {
        if ( classObject == NULL )
            return;

        // Invalidate the memory that is in "classObject".
        DestroyPlacement( classObject );

        // Free our memory.
        void *classMem = classObject;

        memAllocator.Free( classMem, this->GetClassSize() );
    }

    template <typename allocatorType>
    struct DeferredConstructor
    {
        StaticPluginClassFactory *pluginRegistry;
        allocatorType& memAllocator;

        inline DeferredConstructor( StaticPluginClassFactory *pluginRegistry, allocatorType& memAllocator ) : memAllocator( memAllocator )
        {
            this->pluginRegistry = pluginRegistry;
        }

        inline allocatorType& GetAllocator( void )
        {
            return memAllocator;
        }

        inline classType* Construct( void )
        {
            return pluginRegistry->Construct( memAllocator );
        }

        template <typename constructorType>
        inline classType* ConstructTemplate( constructorType& constructor )
        {
            return pluginRegistry->ConstructTemplate( memAllocator, constructor );
        }

        inline classType* Clone( const classType *srcObject )
        {
            return pluginRegistry->Clone( memAllocator, srcObject );
        }

        inline void Destroy( classType *object )
        {
            return pluginRegistry->Destroy( memAllocator, object );
        }
    };

    template <typename allocatorType>
    inline DeferredConstructor <allocatorType>* CreateConstructor( allocatorType& memAllocator )
    {
        typedef DeferredConstructor <allocatorType> Constructor;

        Constructor *result = NULL;

        {
            void *constructorMem = memAllocator.Allocate( sizeof( Constructor ) );

            if ( constructorMem )
            {
                result = new (constructorMem) Constructor( this, memAllocator );
            }
        }
        
        return result;
    }

    template <typename allocatorType>
    inline void DeleteConstructor( DeferredConstructor <allocatorType> *handle )
    {
        typedef DeferredConstructor <allocatorType> Constructor;

        allocatorType& memAlloc = handle->GetAllocator();
        {
            handle->~Constructor();
        }

        void *constructorMem = handle;

        memAlloc.Free( constructorMem, sizeof( Constructor ) );
    }
};

// Array implementation that extends on concepts found inside GTA:SA
// NOTE: This array type is a 'trusted type'.
// -> Use it whenever necessary.
template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename arrayMan, typename countType, typename allocatorType>
struct growableArrayEx
{
    typedef dataType dataType_t;

    allocatorType _memAllocator;

    AINLINE void* _memAlloc( size_t memSize, unsigned int flags )
    {
        return _memAllocator.Allocate( memSize, flags );
    }

    AINLINE void* _memRealloc( void *memPtr, size_t memSize, unsigned int flags )
    {
        return _memAllocator.Realloc( memPtr, memSize, flags );
    }

    AINLINE void _memFree( void *memPtr )
    {
        _memAllocator.Free( memPtr );
    }

    AINLINE growableArrayEx( void )
    {
        data = NULL;
        numActiveEntries = 0;
        sizeCount = 0;
    }

    AINLINE growableArrayEx( const growableArrayEx& right )
    {
        operator = ( right );
    }

    AINLINE void operator = ( const growableArrayEx& right )
    {
        SetSizeCount( right.GetSizeCount() );

        // Copy all data over.
        if ( sizeCount != 0 )
        {
            std::copy( right.data, right.data + sizeCount, data );
        }

        // Set the number of active entries.
        numActiveEntries = right.numActiveEntries;
    }

    AINLINE void SetArrayCachedTo( growableArrayEx& target )
    {
        countType targetSizeCount = GetSizeCount();
        countType oldTargetSizeCount = target.GetSizeCount();

        target.AllocateToIndex( targetSizeCount );

        if ( targetSizeCount != 0 )
        {
            std::copy( data, data + targetSizeCount, target.data );

            // Anything that is above the new target size count must be reset.
            for ( countType n = targetSizeCount; n < oldTargetSizeCount; n++ )
            {
                dataType *theField = &target.data[ n ];

                // Reset it.
                theField->~dataType();

                new (theField) dataType;

                // Tell it to the manager.
                manager.InitField( *theField );
            }
        }

        // Set the number of active entries.
        target.numActiveEntries = numActiveEntries;
    }

    AINLINE ~growableArrayEx( void )
    {
        Shutdown();
    }

    AINLINE void Init( void )
    { }

    AINLINE void Shutdown( void )
    {
        if ( data )
            SetSizeCount( 0 );

        numActiveEntries = 0;
        sizeCount = 0;
    }

    AINLINE void SetSizeCount( countType index )
    {
        if ( index != sizeCount )
        {
            countType oldCount = sizeCount;

            sizeCount = index;

            if ( data )
            {
                // Destroy any structures that got removed.
                for ( countType n = index; n < oldCount; n++ )
                {
                    data[n].~dataType();
                }
            }

            if ( index == 0 )
            {
                // Handle clearance requests.
                if ( data )
                {
                    _memFree( data );

                    data = NULL;
                }
            }
            else
            {
                size_t newArraySize = sizeCount * sizeof( dataType );

                if ( !data )
                    data = (dataType*)_memAlloc( newArraySize, allocFlags );
                else
                    data = (dataType*)_memRealloc( data, newArraySize, allocFlags );
            }

            if ( data )
            {
                // Fill the gap.
                for ( countType n = oldCount; n < index; n++ )
                {
                    new (&data[n]) dataType;

                    manager.InitField( data[n] );
                }
            }
            else
                sizeCount = 0;
        }
    }

    AINLINE void AllocateToIndex( countType index )
    {
        if ( index >= sizeCount )
        {
            SetSizeCount( index + ( pulseCount + 1 ) );
        }
    }

    AINLINE void SetItem( const dataType& dataField, countType index )
    {
        AllocateToIndex( index );

        data[index] = dataField;
    }

    AINLINE void SetFast( const dataType& dataField, countType index )
    {
        // God mercy the coder knows why and how he is using this.
        // We might introduce a hyper-paranoid assertion that even checks this...
        data[index] = dataField;
    }

    AINLINE dataType& GetFast( countType index ) const
    {
        // and that.
        return data[index];
    }

    AINLINE void AddItem( const dataType& data )
    {
        SetItem( data, numActiveEntries );

        numActiveEntries++;
    }

    AINLINE dataType& ObtainItem( countType obtainIndex )
    {
        AllocateToIndex( obtainIndex );

        return data[obtainIndex];
    }

    AINLINE dataType& ObtainItem( void )
    {
        return ObtainItem( numActiveEntries++ );
    }

    AINLINE countType GetCount( void ) const
    {
        return numActiveEntries;
    }

    AINLINE countType GetSizeCount( void ) const
    {
        return sizeCount;
    }

    AINLINE dataType& Get( countType index )
    {
        assert( index < sizeCount );

        return data[index];
    }

    AINLINE const dataType& Get( countType index ) const
    {
        assert( index < sizeCount );

        return data[index];
    }

    AINLINE bool Front( dataType& outVal ) const
    {
        bool success = ( GetCount() != 0 );

        if ( success )
        {
            outVal = data[ 0 ];
        }

        return success;
    }

    AINLINE bool Tail( dataType& outVal ) const
    {
        countType count = GetCount();

        bool success = ( count != 0 );

        if ( success )
        {
            outVal = data[ count - 1 ];
        }

        return success;
    }

    AINLINE bool Pop( dataType& item )
    {
        if ( numActiveEntries != 0 )
        {
            item = data[--numActiveEntries];
            return true;
        }

        return false;
    }

    AINLINE bool Pop( void )
    {
        if ( numActiveEntries != 0 )
        {
            --numActiveEntries;
            return true;
        }

        return false;
    }

    AINLINE void RemoveItem( countType foundSlot )
    {
        assert( foundSlot >= 0 && foundSlot < numActiveEntries );
        assert( numActiveEntries != 0 );

        countType moveCount = numActiveEntries - ( foundSlot + 1 );

        if ( moveCount != 0 )
            std::copy( data + foundSlot + 1, data + numActiveEntries, data + foundSlot );

        numActiveEntries--;
    }

    AINLINE bool RemoveItem( const dataType& item )
    {
        countType foundSlot = -1;
        
        if ( !Find( item, foundSlot ) )
            return false;

        RemoveItem( foundSlot );
        return true;
    }

    AINLINE bool Find( const dataType& inst, countType& indexOut ) const
    {
        for ( countType n = 0; n < numActiveEntries; n++ )
        {
            if ( data[n] == inst )
            {
                indexOut = n;
                return true;
            }
        }

        return false;
    }

    AINLINE bool Find( const dataType& inst ) const
    {
        countType trashIndex;

        return Find( inst, trashIndex );
    }

    AINLINE unsigned int Count( const dataType& inst ) const
    {
        unsigned int count = 0;

        for ( countType n = 0; n < numActiveEntries; n++ )
        {
            if ( data[n] == inst )
                count++;
        }

        return count;
    }

    AINLINE void Clear( void )
    {
        numActiveEntries = 0;
    }

    AINLINE void TrimTo( countType indexTo )
    {
        if ( numActiveEntries > indexTo )
            numActiveEntries = indexTo;
    }

    AINLINE void SwapContents( growableArrayEx& right )
    {
        dataType *myData = this->data;
        dataType *swapData = right.data;

        this->data = swapData;
        right.data = myData;

        countType myActiveCount = this->numActiveEntries;
        countType swapActiveCount = right.numActiveEntries;

        this->numActiveEntries = swapActiveCount;
        right.numActiveEntries = myActiveCount;

        countType mySizeCount = this->sizeCount;
        countType swapSizeCount = right.sizeCount;

        this->sizeCount = swapSizeCount;
        right.sizeCount = mySizeCount;
    }
    
    AINLINE void SetContents( growableArrayEx& right )
    {
        right.SetSizeCount( numActiveEntries );

        for ( countType n = 0; n < numActiveEntries; n++ )
            right.data[n] = data[n];

        right.numActiveEntries = numActiveEntries;
    }

    dataType* data;
    countType numActiveEntries;
    countType sizeCount;
    arrayMan manager;
};

template <typename dataType>
struct iterativeGrowableArrayExManager
{
    AINLINE void InitField( dataType& theField )
    {
        return;
    }
};

template <typename dataType, unsigned int pulseCount, unsigned int allocFlags, typename countType, typename allocatorType>
struct iterativeGrowableArrayEx : growableArrayEx <dataType, pulseCount, allocFlags, iterativeGrowableArrayExManager <dataType>, countType, allocatorType>
{
};

#endif //_GLOBAL_MEMORY_UTILITIES_