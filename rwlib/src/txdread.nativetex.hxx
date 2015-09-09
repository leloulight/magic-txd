#ifndef _RENDERWARE_NATIVE_TEXTURE_PRIVATE_
#define _RENDERWARE_NATIVE_TEXTURE_PRIVATE_

// Shared include header for native texture types.
namespace rw
{

struct nativeTextureBatchedInfo
{
    uint32 mipmapCount;
    uint32 baseWidth, baseHeight;
};

struct nativeTextureSizeRules
{
    inline nativeTextureSizeRules( void )
    {
        // You should fill out the fields you know about.
        // It is recommended to give as much info as possible, but it is okay if you forget about it
        // as long as your native texture is correctly represented in the library!
        this->powerOfTwo = false;
        this->squared = false;
    }

    // New fields may come, but addition is backwards compatible.
    bool powerOfTwo;
    bool squared;

    // Helper for mipmap size rule validation.
    // This model expects a texture that holds simple mipmaps (D3D8 style)
    inline bool IsMipmapSizeValid(
        uint32 layerWidth, uint32 layerHeight
    ) const
    {
        if ( this->powerOfTwo )
        {
            // Verify that the dimensions are power of two.
            double log2val = log(2.0);

            double expWidth = ( log((double)layerWidth) / log2val );
            double expHeight = ( log((double)layerHeight) / log2val );

            // Check that dimensions are power-of-two.
            if ( expWidth != floor(expWidth) || expHeight != floor(expHeight) )
                return false;
        }

        if ( this->squared )
        {
            // Verify that the dimensions are the same.
            if ( layerWidth != layerHeight )
                return false;
        }

        // We are valid, because we violate no rules.
        return true;
    }

    // Helper function to verify a whole pixel data container.
    inline bool verifyPixelData(
        const pixelDataTraversal& pixelData
    )
    {
        size_t mipmapCount = pixelData.mipmaps.size();

        for ( size_t n = 0; n < mipmapCount; n++ )
        {
            const pixelDataTraversal::mipmapResource& mipLevel = pixelData.mipmaps[ n ];

            bool isValid = this->IsMipmapSizeValid( mipLevel.mipWidth, mipLevel.mipHeight );

            if ( !isValid )
            {
                return false;
            }
        }

        return true;
    }
};

struct texNativeTypeProvider abstract
{
    inline texNativeTypeProvider( void )
    {
        this->managerData.rwTexType = NULL;
        this->managerData.isRegistered = false;
    }

    inline ~texNativeTypeProvider( void )
    {
        if ( this->managerData.isRegistered )
        {
            LIST_REMOVE( this->managerData.managerNode );

            this->managerData.isRegistered = false;
        }
    }

    // Type management.
    virtual void            ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) = 0;
    virtual void            CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) = 0;
    virtual void            DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) = 0;

    // Serialization functions.
    virtual eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const throw( ... ) = 0;

    virtual void            SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const throw( ... ) = 0;
    virtual void            DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const throw( ... ) = 0;

    // Conversion parameters.
    virtual void            GetPixelCapabilities( pixelCapabilities& capsOut ) const = 0;
    virtual void            GetStorageCapabilities( storageCapabilities& storeCaps ) const = 0;

    struct acquireFeedback_t
    {
        inline acquireFeedback_t( void )
        {
            this->hasDirectlyAcquired = true;
        }

        bool hasDirectlyAcquired;
    };

    virtual void            GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut ) throw( ... ) = 0;
    virtual void            SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut ) throw( ... ) = 0;
    virtual void            UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate ) = 0;

    // Native Texture version information API.
    virtual void            SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) = 0;
    virtual LibraryVersion  GetTextureVersion( const void *objMem ) = 0;

    // TODO: add meta-data traversal API using dynamic extendable object.

    // Mipmap manipulation API.
    virtual bool            GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut ) = 0;
    virtual bool            AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut ) = 0;
    virtual void            ClearMipmaps( Interface *engineInterface, void *objMem ) = 0;

    // Information API.
    virtual void            GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut ) = 0;
    virtual void            GetTextureFormatString( Interface *engineInterface, void *objMen, char *buf, size_t bufSize, size_t& lengthOut ) const = 0;

    virtual eRasterFormat   GetTextureRasterFormat( const void *objMem ) = 0;
    virtual ePaletteType    GetTexturePaletteType( const void *objMem ) = 0;
    virtual bool                IsTextureCompressed( const void *objMem ) = 0;
    virtual eCompressionType    GetTextureCompressionFormat( const void *objMem ) = 0;

    virtual bool            DoesTextureHaveAlpha( const void *objMem ) = 0;

    // Returns the byte-sized alignment that is used for the rows of each uncompressed texture.
    // It is expected that each pixel data coming out of this native texture conforms to this alignment.
    // Also, RenderWare will make sure that each pixel data that is given to this native texture conforms to it aswell.
    // It is recommended to align native textures to 4 bytes.
    virtual uint32          GetTextureDataRowAlignment( void ) const = 0;

    // Returns the size rules that apply to all textures in the mipmap-chain.
    // Before putting texture data into the native textures the texture data has to be made conformant to the rules.
    // Otherwise the native texture must error out in a visible way, to aid usage of this library.
    virtual void            GetTextureSizeRules( const void *objMem, nativeTextureSizeRules& rulesOut ) const = 0;

    // If you extend this method, your native texture can export a public API interface to the application.
    // This will be an optimized junction point between native internals and high level API, so use it with caution.
    // Note that if internals change the application must take that into account.
    virtual void*           GetNativeInterface( void *objMem )
    {
        return NULL;
    }

    // Returns the native interface that is linked to all native textures of this texture native type.
    // It should be a member of the manager struct.
    virtual void*           GetDriverNativeInterface( void ) const
    {
        return NULL;
    }

    virtual bool            GetDebugBitmap( Interface *engineInterface, void *objMem, Bitmap& drawTarget ) const
    {
        // Native textures type providers can override this method to provide visual insight into their structure.
        // Returning true means that anything has been drawn to drawTarget.
        return false;
    }

    // Native formats often have a more common texture format that they can output to.
    // We should allow them to implement one.
    virtual const char*     GetNativeImageFormatExtension( void ) const
    {
        // Return a valid c-string if there is an implementation for this.
        return NULL;
    }

    virtual bool            IsNativeImageFormat( Interface *engineInterface, Stream *outputStream ) const
    {
        return false;
    }

    virtual void            SerializeNativeImage( Interface *engineInterface, Stream *inputStream, void *objMem ) const
    {
        throw RwException( "native image format not implemented" );
    }

    virtual void            DeserializeNativeImage( Interface *engineInterface, Stream *outputSteam, void *objMem ) const
    {
        throw RwException( "native image format not implemente" );
    }

    // Driver identification functions.
    virtual uint32          GetDriverIdentifier( void *objMem ) const
    {
        // By returning 0, this texture type does not recommend a particular driver.
        return 0;
    }

    struct
    {
        RwTypeSystem::typeInfoBase *rwTexType;

        RwListEntry <texNativeTypeProvider> managerNode;

        bool isRegistered;
    } managerData;
};
 
// Complex native texture API.
bool RegisterNativeTextureType( Interface *engineInterface, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize );
bool UnregisterNativeTextureType( Interface *engineInterface, const char *nativeName );

// Private native texture API.
texNativeTypeProvider* GetNativeTextureTypeProvider( Interface *engineInterface, void *platformData );
uint32 GetNativeTextureMipmapCount( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider );

};

#endif //_RENDERWARE_NATIVE_TEXTURE_PRIVATE_