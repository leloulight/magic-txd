// Dynamic runtime type abstraction system.
#ifndef _DYN_TYPE_ABSTRACTION_SYSTEM_
#define _DYN_TYPE_ABSTRACTION_SYSTEM_

#include "rwlist.hpp"
#include "MemoryUtils.h"

// Memory allocation for boot-strapping.
template <typename structType, typename allocatorType>
inline structType* _newstruct( allocatorType& allocData )
{
    structType *valOut = NULL;
    {
        // Attempt to allocate a block of memory for bootstrapping.
        void *mem = allocData.Allocate( sizeof( structType ) );

        if ( mem )
        {
            valOut = new (mem) structType;
        }
    }
    return valOut;
}

template <typename structType, typename allocatorType>
inline void _delstruct( structType *theStruct, allocatorType& allocData )
{
    theStruct->~structType();

    void *mem = theStruct;

    allocData.Free( mem, sizeof( structType ) );
}

// Type sentry struct of the dynamic type system.
// It notes the programmer that the struct has RTTI.
// Every type constructed through DynamicTypeSystem has this struct before it.
struct GenericRTTI
{
#ifdef _DEBUG
    void *typesys_ptr;      // pointer to the DynamicTypeSystem manager (for debugging)
#endif //_DEBUG
    void *type_meta;        // pointer to the struct that denotes the runtime type
};

// Assertions that can be thrown by the type system
#ifndef rtti_assert
#include <assert.h>

#define rtti_assert( x )    assert( x )
#endif //rtti_assert

// This class manages runtime type information.
// It allows for dynamic C++ class extension depending on runtime conditions.
// Main purpose for its creation are tight memory requirements.
template <typename allocatorType, typename systemPointer_t, typename structFlavorDispatchType = cachedMinimalStructRegistryFlavor <GenericRTTI>>
struct DynamicTypeSystem
{
    typedef allocatorType memAllocType;

    static const unsigned int ANONYMOUS_PLUGIN_ID = 0xFFFFFFFF;

    // Exceptions thrown by this system.
    class abstraction_construction_exception    {};
    class type_name_conflict_exception          {};

    inline DynamicTypeSystem( void )
    {
        LIST_CLEAR( registeredTypes.root );
        
        this->_memAlloc = NULL;
    }

    inline ~DynamicTypeSystem( void )
    {
        // Clean up our memory by deleting all types.
        while ( !LIST_EMPTY( registeredTypes.root ) )
        {
            typeInfoBase *info = LIST_GETITEM( typeInfoBase, registeredTypes.root.next, node );

            DeleteType( info );
        }
    }

    // This field has to be set by the application runtime.
    memAllocType *_memAlloc;

    // Interface for type lifetime management.
    struct typeInterface abstract
    {
        virtual void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const = 0;
        virtual void CopyConstruct( void *mem, const void *srcMem ) const = 0;
        virtual void Destruct( void *mem ) const = 0;

        virtual size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const = 0;

        virtual size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *mem ) const = 0;
    };

    struct typeInfoBase;

    struct pluginDescriptor
    {
        typedef ptrdiff_t pluginOffset_t;

        inline pluginDescriptor( void )
        {
            this->pluginId = DynamicTypeSystem::ANONYMOUS_PLUGIN_ID;
            this->typeInfo = NULL;
        }

        inline pluginDescriptor( unsigned int id, typeInfoBase *typeInfo )
        {
            this->pluginId = id;
            this->typeInfo = typeInfo;
        }

        unsigned int pluginId;
        typeInfoBase *typeInfo;

        template <typename pluginStructType>
        AINLINE pluginStructType* RESOLVE_STRUCT( GenericRTTI *object, pluginOffset_t offset, systemPointer_t *sysPtr )
        {
            return DynamicTypeSystem::RESOLVE_STRUCT <pluginStructType> ( sysPtr, object, this->typeInfo, offset );
        }

        template <typename pluginStructType>
        AINLINE const pluginStructType* RESOLVE_STRUCT( const GenericRTTI *object, pluginOffset_t offset, systemPointer_t *sysPtr )
        {
            return DynamicTypeSystem::RESOLVE_STRUCT <const pluginStructType> ( sysPtr, object, this->typeInfo, offset );
        }
    };

    typedef AnonymousPluginStructRegistry <GenericRTTI, pluginDescriptor, structFlavorDispatchType, systemPointer_t*> structRegistry_t;

    // Localize important struct details.
    typedef typename pluginDescriptor::pluginOffset_t pluginOffset_t;
    typedef typename structRegistry_t::pluginInterface pluginInterface;

    static const pluginOffset_t INVALID_PLUGIN_OFFSET = (pluginOffset_t)-1;

    struct typeInfoBase abstract
    {
        virtual ~typeInfoBase( void )
        { }

        virtual void Cleanup( memAllocType& memAlloc ) = 0;

        const char *name;   // name of this type
        typeInterface *tInterface;  // type construction information

        unsigned long refCount; // number of entities that use the type

        // WARNING: as long as a type is referenced, it MUST not change!!!
        inline bool IsImmutable( void ) const
        {
            return ( this->refCount != 0 );
        }

        unsigned long inheritanceCount; // number of types inheriting from this type

        inline bool IsEndType( void ) const
        {
            return ( this->inheritanceCount == 0 );
        }

        // Special properties.
        bool isExclusive;       // can be used by the runtime to control the creation of objects.
        bool isAbstract;        // can this type be constructed (set internally)

        // Inheritance information.
        typeInfoBase *inheritsFrom; // type that this type inherits from

        // Plugin information.
        structRegistry_t structRegistry;

        RwListEntry <typeInfoBase> node;
    };

    AINLINE static size_t GetTypePluginOffset( const GenericRTTI *rtObj, typeInfoBase *subclassTypeInfo, typeInfoBase *offsetInfo )
    {
        size_t offset = 0;

        if ( typeInfoBase *inheritsFrom = offsetInfo->inheritsFrom )
        {
            offset += GetTypePluginOffset( rtObj, subclassTypeInfo, inheritsFrom );

            offset += inheritsFrom->structRegistry.GetPluginSizeByObject( rtObj );
        }

        return offset;
    }

    AINLINE static typeInfoBase* GetTypeInfoFromTypeStruct( const GenericRTTI *rtObj )
    {
        return (typeInfoBase*)rtObj->type_meta;
    }

    // Function to get the offset of a type information on an object.
    AINLINE static pluginOffset_t GetTypeInfoStructOffset( systemPointer_t *sysPtr, GenericRTTI *rtObj, typeInfoBase *offsetInfo )
    {
        pluginOffset_t baseOffset = 0;

        // Get generic type information.
        typeInfoBase *subclassTypeInfo = GetTypeInfoFromTypeStruct( rtObj );

        // Get the pointer to the language object.
        void *langObj = GetObjectFromTypeStruct( rtObj );

        baseOffset += sizeof( GenericRTTI );

        // Add the dynamic size of the language object.
        baseOffset += subclassTypeInfo->tInterface->GetTypeSizeByObject( sysPtr, langObj );

        // Add the plugin offset.
        baseOffset += GetTypePluginOffset( rtObj, subclassTypeInfo, offsetInfo );

        return baseOffset;
    }

    AINLINE static bool IsOffsetValid( pluginOffset_t offset )
    {
        return ( offset != INVALID_PLUGIN_OFFSET );
    }

    template <typename pluginStructType>
    AINLINE static pluginStructType* RESOLVE_STRUCT( systemPointer_t *sysPtr, GenericRTTI *rtObj, typeInfoBase *typeInfo, pluginOffset_t offset )
    {
        if ( !IsOffsetValid( offset ) )
            return NULL;

        size_t baseOffset = GetTypeInfoStructOffset( sysPtr, rtObj, typeInfo );

        pluginOffset_t realOffset = GetTypeRegisteredPluginLocation( typeInfo, rtObj, offset );

        return (pluginStructType*)( (char*)rtObj + baseOffset + realOffset );
    }

    template <typename pluginStructType>
    AINLINE static const pluginStructType* RESOLVE_STRUCT( systemPointer_t *sysPtr, const GenericRTTI *rtObj, typeInfoBase *typeInfo, pluginOffset_t offset )
    {
        if ( !IsOffsetValid( offset ) )
            return NULL;

        size_t baseOffset = GetTypeInfoStructOffset( sysPtr, (GenericRTTI*)rtObj, typeInfo );

        pluginOffset_t realOffset = GetTypeRegisteredPluginLocation( typeInfo, rtObj, offset );

        return (const pluginStructType*)( (char*)rtObj + baseOffset + realOffset );
    }

    // Function used to register a new plugin struct into the class.
    inline pluginOffset_t RegisterPlugin( size_t pluginSize, const pluginDescriptor& descriptor, pluginInterface *plugInterface )
    {
        rtti_assert( descriptor.typeInfo->IsImmutable() == false );

        return descriptor.typeInfo->structRegistry.RegisterPlugin( pluginSize, descriptor, plugInterface );
    }

    inline void UnregisterPlugin( typeInfoBase *typeInfo, pluginOffset_t pluginOffset )
    {
        rtti_assert( typeInfo->IsImmutable() == false );

        typeInfo->structRegistry.UnregisterPlugin( pluginOffset );
    }

    // Plugin registration functions.
    typedef CommonPluginSystemDispatch <GenericRTTI, DynamicTypeSystem, pluginDescriptor, systemPointer_t*> functoidHelper_t;

    template <typename structType>
    inline pluginOffset_t RegisterStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).RegisterStructPlugin <structType> ( descriptor );
    }

    template <typename structType>
    inline pluginOffset_t RegisterDependantStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId, size_t structSize = sizeof(structType) )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).RegisterDependantStructPlugin <structType> ( descriptor, structSize );
    }

    typedef typename functoidHelper_t::conditionalPluginStructInterface conditionalPluginStructInterface;

    template <typename structType>
    inline pluginOffset_t RegisterDependantConditionalStructPlugin( typeInfoBase *typeInfo, unsigned int pluginId, conditionalPluginStructInterface *conditional, size_t structSize = sizeof(structType) )
    {
        pluginDescriptor descriptor( pluginId, typeInfo );

        return functoidHelper_t( *this ).RegisterDependantConditionalStructPlugin( descriptor, conditional, structSize );
    }

    RwList <typeInfoBase> registeredTypes;

    inline void SetupTypeInfoBase( typeInfoBase *tInfo, const char *typeName, typeInterface *tInterface, typeInfoBase *inheritsFrom ) throw( ... )
    {
        // If we find a type info with this name already, throw an exception.
        {
            typeInfoBase *alreadyExisting = FindTypeInfo( typeName, inheritsFrom );

            if ( alreadyExisting != NULL )
            {
                throw type_name_conflict_exception();
            }
        }

        tInfo->name = typeName;
        tInfo->refCount = 0;
        tInfo->inheritanceCount = 0;
        tInfo->isExclusive = false;
        tInfo->isAbstract = false;
        tInfo->tInterface = tInterface;
        tInfo->inheritsFrom = NULL;
        LIST_INSERT( registeredTypes.root, tInfo->node );

        // Set inheritance.
        try
        {
            this->SetTypeInfoInheritingClass( tInfo, inheritsFrom );
        }
        catch( ... )
        {
            LIST_REMOVE( tInfo->node );

            throw;
        }
    }

    inline typeInfoBase* RegisterType( const char *typeName, typeInterface *typeInterface, typeInfoBase *inheritsFrom = NULL ) throw( ... )
    {
        struct typeInfoGeneral : public typeInfoBase
        {
            void Cleanup( memAllocType& memAlloc )
            {
                // Terminate ourselves.
                _delstruct( this, memAlloc );
            }
        };

        typeInfoGeneral *info = _newstruct <typeInfoGeneral> ( *_memAlloc );

        if ( info )
        {
            try
            {
                SetupTypeInfoBase( info, typeName, typeInterface, inheritsFrom );
            }
            catch( ... )
            {
                _delstruct <typeInfoGeneral> ( info, *_memAlloc );

                throw;
            }
        }

        return info;
    }

    template <typename structTypeTypeInterface>
    inline typeInfoBase* RegisterCommonTypeInterface( const char *typeName, structTypeTypeInterface *tInterface, typeInfoBase *inheritsFrom = NULL ) throw( ... )
    {
        struct typeInfoStruct : public typeInfoBase
        {
            void Cleanup( memAllocType& memAlloc )
            {
                _delstruct( tInterface, memAlloc );
                _delstruct( this, memAlloc );
            }

            structTypeTypeInterface *tInterface;
        };

        typeInfoStruct *tInfo = _newstruct <typeInfoStruct> ( *_memAlloc );

        if ( tInfo )
        {
            try
            {
                SetupTypeInfoBase( tInfo, typeName, tInterface, inheritsFrom );
            }
            catch( ... )
            {
                _delstruct <typeInfoStruct> ( tInfo, *_memAlloc );

                throw;
            }

            tInfo->tInterface = tInterface;
            return tInfo;
        }
        
        _delstruct( tInterface, *_memAlloc );
        return NULL;
    }

    template <typename structType>
    inline typeInfoBase* RegisterAbstractType( const char *typeName, typeInfoBase *inheritsFrom = NULL ) throw( ... )
    {
        struct structTypeInterface : public typeInterface
        {
            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const
            {
                throw abstraction_construction_exception();
            }

            void CopyConstruct( void *mem, const void *srcMem ) const
            {
                throw abstraction_construction_exception();
            }

            void Destruct( void *mem ) const
            {
                return;
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const
            {
                return sizeof( structType );
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *langObj ) const
            {
                return (size_t)0;
            }
        };

        structTypeInterface *tInterface = _newstruct <structTypeInterface> ( *_memAlloc );

        typeInfoBase *newTypeInfo = NULL;

        if ( tInterface )
        {
            try
            {
                newTypeInfo = RegisterCommonTypeInterface( typeName, tInterface, inheritsFrom );

                if ( newTypeInfo )
                {
                    newTypeInfo->isAbstract = true;
                }
            }
            catch( ... )
            {
                _delstruct <structTypeInterface> ( tInterface, *_memAlloc );

                throw;
            }
        }

        return newTypeInfo;
    }

    template <typename structType>
    inline typeInfoBase* RegisterStructType( const char *typeName, typeInfoBase *inheritsFrom = NULL, size_t structSize = sizeof( structType ) ) throw( ... )
    {
        struct structTypeInterface : public typeInterface
        {
            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const
            {
                new (mem) structType( sysPtr, construct_params );
            }

            void CopyConstruct( void *mem, const void *srcMem ) const
            {
                new (mem) structType( *(const structType*)srcMem );
            }

            void Destruct( void *mem ) const
            {
                ((structType*)mem)->~structType();
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const
            {
                return this->structSize;
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *langObj ) const
            {
                return this->structSize;
            }

            size_t structSize;
        };

        structTypeInterface *tInterface = _newstruct <structTypeInterface> ( *_memAlloc );

        typeInfoBase *newTypeInfo = NULL;

        if ( tInterface )
        {
            tInterface->structSize = structSize;

            try
            {
                newTypeInfo = RegisterCommonTypeInterface( typeName, tInterface, inheritsFrom );
            }
            catch( ... )
            {
                _delstruct <structTypeInterface> ( tInterface, *_memAlloc );

                throw;
            }
        }

        return newTypeInfo;
    }

    template <typename classType, typename staticRegistry>
    inline pluginOffset_t StaticPluginRegistryRegisterTypeConstruction( staticRegistry& registry, typeInfoBase *typeInfo, systemPointer_t *sysPtr, void *construction_params = NULL ) throw( ... )
    {
        struct structPluginInterface : staticRegistry::pluginInterface
        {
            bool OnPluginConstruct( staticRegistry::hostType_t *obj, staticRegistry::pluginOffset_t pluginOffset, staticRegistry::pluginDescriptor pluginId ) override
            {
                void *structMem = pluginId.RESOLVE_STRUCT <void> ( obj, pluginOffset );

                if ( structMem == NULL )
                    return false;

                // Construct the type.
                GenericRTTI *rtObj = typeSys->ConstructPlacement( obj, structMem, typeInfo, construction_params );

                // Hack: tell it about the struct.
                if ( rtObj != NULL )
                {
                    void *langObj = DynamicTypeSystem::GetObjectFromTypeStruct( rtObj );

                    ((classType*)langObj)->Initialize( obj );
                }
                
                return ( rtObj != NULL );
            }

            void OnPluginDestruct( staticRegistry::hostType_t *obj, staticRegistry::pluginOffset_t pluginOffset, staticRegistry::pluginDescriptor pluginId ) override
            {
                GenericRTTI *rtObj = pluginId.RESOLVE_STRUCT <GenericRTTI> ( obj, pluginOffset );

                // Hack: deinitialize the class.
                {
                    void *langObj = DynamicTypeSystem::GetObjectFromTypeStruct( rtObj );

                    ((classType*)langObj)->Shutdown( obj );
                }

                // Destruct the type.
                typeSys->DestroyPlacement( obj, rtObj );
            }

            bool OnPluginAssign( staticRegistry::hostType_t *dstObject, const staticRegistry::hostType_t *srcObject, staticRegistry::pluginOffset_t pluginOffset, staticRegistry::pluginDescriptor pluginId ) override
            {
                return false;
            }

            void DeleteOnUnregister( void )
            {
                _delstruct( this, *typeSys->_memAlloc );
            }

            DynamicTypeSystem *typeSys;
            typeInfoBase *typeInfo;
            void *construction_params;
        };

        staticRegistry::pluginOffset_t offset = 0;

        structPluginInterface *tInterface = _newstruct <structPluginInterface> ( *_memAlloc );

        if ( tInterface )
        {
            try
            {
                tInterface->typeSys = this;
                tInterface->typeInfo = typeInfo;
                tInterface->construction_params = construction_params;

                offset = registry.RegisterPlugin(
                    this->GetTypeStructSize( sysPtr, typeInfo, construction_params ),
                    staticRegistry::pluginDescriptor( staticRegistry::ANONYMOUS_PLUGIN_ID ),
                    tInterface
                );

                if ( !staticRegistry::IsOffsetValid( offset ) )
                {
                    _delstruct( tInterface, *_memAlloc );
                }
            }
            catch( ... )
            {
                _delstruct <structPluginInterface> ( tInterface, *_memAlloc );

                throw;
            }
        }

        return offset;
    }

    struct structTypeMetaInfo abstract
    {
        virtual ~structTypeMetaInfo( void )     {}

        virtual size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const = 0;

        virtual size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *mem ) const = 0;
    };

    template <typename structType>
    inline typeInfoBase* RegisterDynamicStructType( const char *typeName, structTypeMetaInfo *metaInfo, bool freeMetaInfo, typeInfoBase *inheritsFrom = NULL ) throw( ... )
    {
        struct structTypeInterface : public typeInterface
        {
            inline structTypeInterface( void )
            {
                this->meta_info = NULL;
                this->freeMetaInfo = false;
            }

            inline ~structTypeInterface( void )
            {
                if ( this->freeMetaInfo )
                {
                    if ( structTypeMetaInfo *metaInfo = this->meta_info )
                    {
                        delete metaInfo;

                        this->meta_info = NULL;
                    }
                }
            }

            void Construct( void *mem, systemPointer_t *sysPtr, void *construct_params ) const
            {
                new (mem) structType( sysPtr, construct_params );
            }

            void CopyConstruct( void *mem, const void *srcMem ) const
            {
                new (mem) structType( *(const structType*)srcMem );
            }

            void Destruct( void *mem ) const
            {
                ((structType*)mem)->~structType();
            }

            size_t GetTypeSize( systemPointer_t *sysPtr, void *construct_params ) const
            {
                return meta_info->GetTypeSize( sysPtr, construct_params );
            }

            size_t GetTypeSizeByObject( systemPointer_t *sysPtr, const void *obj ) const
            {
                return meta_info->GetTypeSizeByObject( sysPtr, obj );
            }
           
            structTypeMetaInfo *meta_info;
            bool freeMetaInfo;
        };

        typeInfoBase *newTypeInfo = NULL;

        structTypeInterface *tInterface = _newstruct <structTypeInterface> ( *_memAlloc );
        
        if ( tInterface )
        {
            // We inherit the meta information struct.
            // This means that we will also take care about deallocation.
            tInterface->meta_info = metaInfo;
            tInterface->freeMetaInfo = freeMetaInfo;

            try
            {
                newTypeInfo = RegisterCommonTypeInterface( typeName, tInterface, inheritsFrom );
            }
            catch( ... )
            {
                _delstruct <structTypeInterface> ( tInterface, *_memAlloc );

                throw;
            }
        }

        return newTypeInfo;
    }

    static inline size_t GetTypePluginSize( typeInfoBase *typeInfo )
    {
        // In the DynamicTypeSystem environment, we do not introduce conditional registry plugin structs.
        // That would complicate things too much, but support can be added if truly required.
        // Development does not have to be hell.
        // Without conditional struct support, this operation stays O(1).
        size_t sizeOut = (size_t)typeInfo->structRegistry.GetPluginSizeByRuntime();
        
        // Add the plugin sizes of all inherited classes.
        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            sizeOut += GetTypePluginSize( inheritedClass );
        }

        return sizeOut;
    }

    static inline pluginOffset_t GetTypeRegisteredPluginLocation( typeInfoBase *typeInfo, const GenericRTTI *theObject, pluginOffset_t pluginOffDesc )
    {
        return typeInfo->structRegistry.ResolvePluginStructOffsetByObject( theObject, pluginOffDesc );
    }

    inline bool ConstructPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *rtObj )
    {
        bool pluginConstructSuccess = false;

        // If we have no parents, it is success by default.
        bool parentConstructionSuccess = true;

        // First construct the parents.
        typeInfoBase *inheritedClass = typeInfo->inheritsFrom;

        if ( inheritedClass != NULL )
        {
            parentConstructionSuccess = ConstructPlugins( sysPtr, inheritedClass, rtObj );
        }

        // If the parents have constructed properly, do ourselves.
        if ( parentConstructionSuccess )
        {
            bool thisConstructionSuccess = typeInfo->structRegistry.ConstructPluginBlock( rtObj, sysPtr );

            // If anything has failed, we destroy everything we already constructed.
            if ( !thisConstructionSuccess )
            {
                if ( inheritedClass != NULL )
                {
                    DestructPlugins( sysPtr, inheritedClass, rtObj );
                }
            }
            else
            {
                pluginConstructSuccess = true;
            }
        }

        return pluginConstructSuccess;
    }

    inline bool AssignPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *dstRtObj, const GenericRTTI *srcRtObj )
    {
        // Assign plugins from the ground up.
        // If anything fails assignment, we just bail.
        // Failure in assignment does not imply an incorrect object state.
        bool assignmentSuccess = false;

        // First assign the parents.
        bool parentAssignmentSuccess = true;

        typeInfoBase *inheritedType = typeInfo->inheritsFrom;

        if ( inheritedType != NULL )
        {
            parentAssignmentSuccess = AssignPlugins( sysPtr, inheritedType, dstRtObj, srcRtObj );
        }

        // If all the parents successfully assigned, we can try assigning ourselves.
        if ( parentAssignmentSuccess )
        {
            bool thisAssignmentSuccess = typeInfo->structRegistry.AssignPluginBlock( dstRtObj, srcRtObj, sysPtr );

            // If assignment was successful, we are happy.
            // Otherwise we bail the entire thing.
            if ( thisAssignmentSuccess )
            {
                assignmentSuccess = true;
            }
        }

        return assignmentSuccess;
    }

    inline void DestructPlugins( systemPointer_t *sysPtr, typeInfoBase *typeInfo, GenericRTTI *rtObj )
    {
        try
        {
            typeInfo->structRegistry.DestroyPluginBlock( rtObj, sysPtr );
        }
        catch( ... )
        {
            rtti_assert( 0 );
        }

        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            DestructPlugins( sysPtr, inheritedClass, rtObj );
        }
    }

    inline size_t GetTypeStructSize( systemPointer_t *sysPtr, typeInfoBase *typeInfo, void *construct_params )
    {
        typeInterface *tInterface = typeInfo->tInterface;

        // Attempt to get the memory the language object will take.
        size_t objMemSize = tInterface->GetTypeSize( sysPtr, construct_params );

        if ( objMemSize != 0 )
        {
            // Adjust that objMemSize so we can store meta information + plugins.
            objMemSize += sizeof( GenericRTTI );

            // Calculate the memory that is required by all plugin structs.
            objMemSize += GetTypePluginSize( typeInfo );
        }

        return objMemSize;
    }

    inline size_t GetTypeStructSize( systemPointer_t *sysPtr, const GenericRTTI *rtObj )
    {
        typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( rtObj );
        typeInterface *tInterface = typeInfo->tInterface;

        // Get the pointer to the object.
        const void *langObj = GetConstObjectFromTypeStruct( rtObj );

        // Get the memory that is taken by the language object.
        size_t objMemSize = tInterface->GetTypeSizeByObject( sysPtr, langObj );

        if ( objMemSize != 0 )
        {
            // Take the meta data into account.
            objMemSize += sizeof( GenericRTTI );

            // Add the memory taken by the plugins.
            objMemSize += GetTypePluginSize( typeInfo );
        }
        
        return objMemSize;
    }

    inline void ReferenceTypeInfo( typeInfoBase *typeInfo )
    {
        typeInfo->refCount++;

        // For every type we inherit, we reference it as well.
        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            ReferenceTypeInfo( inheritedClass );
        }
    }

    inline void DereferenceTypeInfo( typeInfoBase *typeInfo )
    {
        // For every type we inherit, we dereference it as well.
        if ( typeInfoBase *inheritedClass = typeInfo->inheritsFrom )
        {
            DereferenceTypeInfo( inheritedClass );
        }

        typeInfo->refCount--;
    }

    inline GenericRTTI* ConstructPlacement( systemPointer_t *sysPtr, void *objMem, typeInfoBase *typeInfo, void *construct_params )
    {
        GenericRTTI *objOut = NULL;
        {
            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            // Get the specialization interface.
            typeInterface *tInterface = typeInfo->tInterface;

            // Get a pointer to GenericRTTI and the object memory.
            GenericRTTI *objTypeMeta = (GenericRTTI*)objMem;

            // Initialize the RTTI struct.
            objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
            objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

            // Initialize the language object.
            void *objStruct = objTypeMeta + 1;

            try
            {
                // Attempt to construct the language part.
                tInterface->Construct( objStruct, sysPtr, construct_params );
            }
            catch( ... )
            {
                // We failed to construct the object struct, so it is invalid.
                objStruct = NULL;
            }

            if ( objStruct )
            {
                // Only proceed if we have successfully constructed the object struct.
                // Now construct the plugins.
                bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                if ( pluginConstructSuccess )
                {
                    // We are finished! Return the meta info.
                    objOut = objTypeMeta;
                }
                else
                {
                    // We failed, so destruct the class again.
                    tInterface->Destruct( objStruct );
                }
            }

            if ( objOut == NULL )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

    inline GenericRTTI* Construct( systemPointer_t *sysPtr, typeInfoBase *typeInfo, void *construct_params )
    {
        GenericRTTI *objOut = NULL;
        {
            size_t objMemSize = GetTypeStructSize( sysPtr, typeInfo, construct_params );

            if ( objMemSize != 0 )
            {
                void *objMem = _memAlloc->Allocate( objMemSize );

                if ( objMem )
                {
                    // Attempt to construct the object on the memory.
                    objOut = ConstructPlacement( sysPtr, objMem, typeInfo, construct_params );
                }

                if ( !objOut )
                {
                    // Deallocate the memory again, as we seem to have failed.
                    _memAlloc->Free( objMem, objMemSize );
                }
            }
        }
        return objOut;
    }

    inline GenericRTTI* ClonePlacement( systemPointer_t *sysPtr, void *objMem, const GenericRTTI *toBeCloned )
    {
        GenericRTTI *objOut = NULL;
        {
            // Grab the type of toBeCloned.
            typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( toBeCloned );

            // Reference the type info.
            ReferenceTypeInfo( typeInfo );

            // Get the specialization interface.
            const typeInterface *tInterface = typeInfo->tInterface;

            // Get a pointer to GenericRTTI and the object memory.
            GenericRTTI *objTypeMeta = (GenericRTTI*)objMem;

            // Initialize the RTTI struct.
            objTypeMeta->type_meta = typeInfo;
#ifdef _DEBUG
            objTypeMeta->typesys_ptr = this;
#endif //_DEBUG

            // Initialize the language object.
            void *objStruct = objTypeMeta + 1;

            // Get the struct which we create from.
            const void *srcObjStruct = toBeCloned + 1;

            try
            {
                // Attempt to copy construct the language part.
                tInterface->CopyConstruct( objStruct, srcObjStruct );
            }
            catch( ... )
            {
                // We failed to construct the object struct, so it is invalid.
                objStruct = NULL;
            }

            if ( objStruct )
            {
                // Only proceed if we have successfully constructed the object struct.
                // First we have to construct the plugins.
                bool pluginSuccess = false;

                bool pluginConstructSuccess = ConstructPlugins( sysPtr, typeInfo, objTypeMeta );

                if ( pluginConstructSuccess )
                {
                    // Now assign the plugins from the source object.
                    bool pluginAssignSuccess = AssignPlugins( sysPtr, typeInfo, objTypeMeta, toBeCloned );

                    if ( pluginAssignSuccess )
                    {
                        // We are finished! Return the meta info.
                        pluginSuccess = true;
                    }
                }
                
                if ( pluginSuccess )
                {
                    objOut = objTypeMeta;
                }
                else
                {
                    // We failed, so destruct the class again.
                    tInterface->Destruct( objStruct );
                }
            }

            if ( objOut == NULL )
            {
                // Since we did not return a proper object, dereference again.
                DereferenceTypeInfo( typeInfo );
            }
        }
        return objOut;
    }

    inline GenericRTTI* Clone( systemPointer_t *sysPtr, const GenericRTTI *toBeCloned )
    {
        GenericRTTI *objOut = NULL;
        {
            // Get the size toBeCloned currently takes.
            size_t objMemSize = GetTypeStructSize( sysPtr, toBeCloned );

            if ( objMemSize != 0 )
            {
                void *objMem = _memAlloc->Allocate( objMemSize );

                if ( objMem )
                {
                    // Attempt to clone the object on the memory.
                    objOut = ClonePlacement( sysPtr, objMem, toBeCloned );
                }

                if ( !objOut )
                {
                    // Deallocate the memory again, as we seem to have failed.
                    _memAlloc->Free( objMem, objMemSize );
                }
            }
        }
        return objOut;
    }

    inline void SetTypeInfoExclusive( typeInfoBase *typeInfo, bool isExclusive )
    {
        typeInfo->isExclusive = isExclusive;
    }

    inline bool IsTypeInfoExclusive( typeInfoBase *typeInfo )
    {
        return typeInfo->isExclusive;
    }

    inline bool IsTypeInfoAbstract( typeInfoBase *typeInfo )
    {
        return typeInfo->isAbstract;
    }

    inline void SetTypeInfoInheritingClass( typeInfoBase *subClass, typeInfoBase *inheritedClass ) throw( ... )
    {
        bool subClassImmutability = subClass->IsImmutable();

        rtti_assert( subClassImmutability == false );

        if ( subClassImmutability == false )
        {
            // Make sure we can even do that.
            // Verify that no other type with that name exists which inherits from said class.
            {
                typeInfoBase *alreadyExisting = FindTypeInfo( subClass->name, inheritedClass );

                if ( alreadyExisting && alreadyExisting != subClass )
                {
                    throw type_name_conflict_exception();
                }
            }

            if ( inheritedClass != NULL )
            {
                rtti_assert( IsTypeInheritingFrom( subClass, inheritedClass ) == false );
            }

            if ( typeInfoBase *prevInherit = subClass->inheritsFrom )
            {
                prevInherit->inheritanceCount--;
            }

            subClass->inheritsFrom = inheritedClass;

            if ( inheritedClass )
            {
                inheritedClass->inheritanceCount++;
            }
        }
    }

    inline bool IsTypeInheritingFrom( typeInfoBase *baseClass, typeInfoBase *subClass ) const
    {
        if ( IsSameType( baseClass, subClass ) )
        {
            return true;
        }

        typeInfoBase *inheritedClass = subClass->inheritsFrom;

        if ( inheritedClass )
        {
            return IsTypeInheritingFrom( baseClass, inheritedClass );
        }

        return false;
    }

    inline bool IsSameType( typeInfoBase *firstType, typeInfoBase *secondType ) const
    {
        return ( firstType == secondType );
    }

    static inline void* GetObjectFromTypeStruct( GenericRTTI *rtObj )
    {
        return (void*)( rtObj + 1 );
    }

    static inline const void* GetConstObjectFromTypeStruct( const GenericRTTI *rtObj )
    {
        return (void*)( rtObj + 1 );
    }

    static inline GenericRTTI* GetTypeStructFromObject( void *langObj )
    {
        return ( (GenericRTTI*)langObj - 1 );
    }

    static inline const GenericRTTI* GetTypeStructFromConstObject( const void *langObj )
    {
        return ( (const GenericRTTI*)langObj - 1 );
    }

protected:
    inline void DebugRTTIStruct( const GenericRTTI *typeInfo ) const
    {
#ifdef _DEBUG
        // If this assertion fails, the runtime may have mixed up times from different contexts (i.e. different configurations in Lua)
        // The problem is certainly found in the application itself.
        rtti_assert( typeInfo->typesys_ptr == this );
#endif //_DEBUG
    }

public:
    inline GenericRTTI* GetTypeStructFromAbstractObject( void *langObj )
    {
        GenericRTTI *typeInfo = ( (GenericRTTI*)langObj - 1 );

        // Make sure it is valid.
        DebugRTTIStruct( typeInfo );

        return typeInfo;
    }

    inline const GenericRTTI* GetTypeStructFromConstAbstractObject( const void *langObj ) const
    {
        const GenericRTTI *typeInfo = ( (const GenericRTTI*)langObj - 1 );

        // Make sure it is valid.
        DebugRTTIStruct( typeInfo );

        return typeInfo;
    }

    inline void DestroyPlacement( systemPointer_t *sysPtr, GenericRTTI *typeStruct )
    {
        typeInfoBase *typeInfo = GetTypeInfoFromTypeStruct( typeStruct );
        typeInterface *tInterface = typeInfo->tInterface;

        // Destroy all object plugins.
        DestructPlugins( sysPtr, typeInfo, typeStruct );

        // Pointer to the language object.
        void *langObj = (void*)( typeStruct + 1 );

        // Destroy the actual object.
        try
        {
            tInterface->Destruct( langObj );
        }
        catch( ... )
        {
            // We cannot handle this, since it must not happen in the first place.
            rtti_assert( 0 );
        }

        // Dereference the type info since we do not require it anymore.
        DereferenceTypeInfo( typeInfo );
    }

    inline void Destroy( systemPointer_t *sysPtr, GenericRTTI *typeStruct )
    {
        // Get the actual type struct size.
        size_t objMemSize = GetTypeStructSize( sysPtr, typeStruct );

        rtti_assert( objMemSize != 0 );  // it cannot be zero.

        // Delete the object from the memory.
        DestroyPlacement( sysPtr, typeStruct );

        // Free the memory.
        void *rttiMem = (void*)typeStruct;

        _memAlloc->Free( rttiMem, objMemSize );
    }

    inline void DeleteType( typeInfoBase *typeInfo )
    {
        LIST_REMOVE( typeInfo->node );

        // Make sure we do not inherit from anything anymore.
        SetTypeInfoInheritingClass( typeInfo, NULL );

        // Make sure all classes that inherit from us do not do that anymore.
        LIST_FOREACH_BEGIN( typeInfoBase, this->registeredTypes.root, node )

            if ( item->inheritsFrom == typeInfo )
            {
                SetTypeInfoInheritingClass( item, NULL );
            }

        LIST_FOREACH_END

        typeInfo->Cleanup( *_memAlloc );
    }

    struct type_iterator
    {
        RwList <typeInfoBase>& listRoot;
        RwListEntry <typeInfoBase> *curNode;

        inline type_iterator( DynamicTypeSystem& typeSys ) : listRoot( typeSys.registeredTypes )
        {
            this->curNode = listRoot.root.next;
        }

        inline bool IsEnd( void ) const
        {
            return ( &listRoot.root == curNode );
        }

        inline typeInfoBase* Resolve( void ) const
        {
            return LIST_GETITEM( typeInfoBase, curNode, node );
        }

        inline void Increment( void )
        {
            this->curNode = this->curNode->next;
        }
    };

    inline type_iterator GetTypeIterator( void )
    {
        return type_iterator( *this );
    }

    struct type_resolution_iterator
    {
        const char *typePath;
        const char *type_iter_ptr;
        size_t tokenLen;

        inline type_resolution_iterator( const char *typePath )
        {
            this->typePath = typePath;
            this->type_iter_ptr = typePath;
            this->tokenLen = 0;

            this->Increment();
        }

        inline std::string Resolve( void ) const
        {
            const char *returnedPathToken = this->typePath;

            size_t nameLength = this->tokenLen;

            return std::string( returnedPathToken, nameLength );
        }

        inline void Increment( void )
        {
            this->typePath = this->type_iter_ptr;

            size_t currentTokenLen = 0;

            while ( true )
            {
                const char *curPtr = this->type_iter_ptr;

                char c = *curPtr;

                if ( c == 0 )
                {
                    currentTokenLen = ( curPtr - this->typePath );

                    break;
                }

                if ( strncmp( curPtr, "::", 2 ) == 0 )
                {
                    currentTokenLen = ( curPtr - this->typePath );

                    this->type_iter_ptr = curPtr + 2;

                    break;
                }

                this->type_iter_ptr++;
            }

            // OK.
            this->tokenLen = currentTokenLen;
        }

        inline bool IsEnd( void ) const
        {
            return ( this->typePath == this->type_iter_ptr );
        }
    };

    inline typeInfoBase* FindTypeInfo( const char *typeName, typeInfoBase *baseType ) const
    {
        LIST_FOREACH_BEGIN( typeInfoBase, this->registeredTypes.root, node )

            bool isInterestingType = true;

            if ( baseType != item->inheritsFrom )
            {
                isInterestingType = false;
            }

            if ( isInterestingType && strcmp( item->name, typeName ) == 0 )
            {
                return item;
            }

        LIST_FOREACH_END

        return NULL;
    }

    // Type resolution based on type descriptors.
    inline typeInfoBase* ResolveTypeInfo( const char *typePath, typeInfoBase *baseTypeInfo = NULL )
    {
        typeInfoBase *returnedTypeInfo = NULL;
        {
            typeInfoBase *currentType = baseTypeInfo;

            // Find a base type.
            type_resolution_iterator type_path_iter( typePath );

            while ( type_path_iter.IsEnd() == false )
            {
                std::string curToken = type_path_iter.Resolve();

                currentType = FindTypeInfo( curToken.c_str(), currentType );

                if ( currentType == NULL )
                {
                    break;
                }

                type_path_iter.Increment();
            }

            returnedTypeInfo = currentType;
        }
        return returnedTypeInfo;
    }
};

#endif //_DYN_TYPE_ABSTRACTION_SYSTEM_