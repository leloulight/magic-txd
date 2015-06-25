#include "StdInc.h"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <assert.h>
#include <bitset>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>
#include <algorithm>
#include <cmath>

#include "pixelformat.hxx"

#include "txdread.d3d.hxx"

#include "pluginutil.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.dxt.hxx"

namespace rw
{

/*
 * Texture Dictionary
 */

TexDictionary* texDictionaryStreamPlugin::CreateTexDictionary( Interface *engineInterface ) const
{
    GenericRTTI *rttiObj = engineInterface->typeSystem.Construct( engineInterface, this->txdTypeInfo, NULL );

    if ( rttiObj == NULL )
    {
        return NULL;
    }
    
    TexDictionary *txdObj = (TexDictionary*)RwTypeSystem::GetObjectFromTypeStruct( rttiObj );

    return txdObj;
}

inline bool isRwObjectInheritingFrom( Interface *engineInterface, RwObject *rwObj, RwTypeSystem::typeInfoBase *baseType )
{
    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( rwObj );

    if ( rtObj )
    {
        // Check whether the type of the dynamic object matches that of TXD.
        RwTypeSystem::typeInfoBase *objTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        if ( engineInterface->typeSystem.IsTypeInheritingFrom( baseType, objTypeInfo ) )
        {
            return true;
        }
    }

    return false;
}

TexDictionary* texDictionaryStreamPlugin::ToTexDictionary( Interface *engineInterface, RwObject *rwObj )
{
    if ( isRwObjectInheritingFrom( engineInterface, rwObj, this->txdTypeInfo ) )
    {
        return (TexDictionary*)rwObj;
    }

    return NULL;
}

void texDictionaryStreamPlugin::Deserialize( Interface *engineInterface, BlockProvider& inputProvider, RwObject *objectToDeserialize ) const
{
    // Cast our object.
    TexDictionary *txdObj = (TexDictionary*)objectToDeserialize;

    // Read the textures.
    {
        BlockProvider texDictMetaStructBlock( &inputProvider );

        uint32 textureBlockCount = 0;
        bool requiresRecommendedPlatform = true;

        texDictMetaStructBlock.EnterContext();

        if ( texDictMetaStructBlock.getBlockID() == CHUNK_STRUCT )
        {
            // Read the header block depending on version.
            LibraryVersion libVer = texDictMetaStructBlock.getBlockVersion();

            if (libVer.rwLibMinor <= 5)
            {
                textureBlockCount = texDictMetaStructBlock.readUInt32();
            }
            else
            {
                textureBlockCount = texDictMetaStructBlock.readUInt16();
                uint16 recommendedPlatform = texDictMetaStructBlock.readUInt16();

                // So if there is a recommended platform set, we will also give it one if we will write it.
                requiresRecommendedPlatform = ( recommendedPlatform != 0 );
            }
        }
        else
        {
            engineInterface->PushWarning( "could not find texture dictionary meta information" );
        }

        txdObj->hasRecommendedPlatform = requiresRecommendedPlatform;

        // We finished reading meta data.
        texDictMetaStructBlock.LeaveContext();

        // Now follow multiple TEXTURENATIVE blocks.
        // Deserialize all of them.

        for ( uint32 n = 0; n < textureBlockCount; n++ )
        {
            BlockProvider textureNativeBlock( &inputProvider );

            // Deserialize this block.
            RwObject *rwObj = NULL;

            std::string errDebugMsg;

            try
            {
                rwObj = engineInterface->DeserializeBlock( textureNativeBlock );
            }
            catch( RwException& except )
            {
                // Catch the exception and try to continue.
                rwObj = NULL;

                if ( textureNativeBlock.doesIgnoreBlockRegions() )
                {
                    // If we failed any texture parsing in the "ignoreBlockRegions" parse mode,
                    // there is no point in continuing, since the environment does not recover.
                    throw;
                }

                errDebugMsg = except.message;
            }

            if ( rwObj )
            {
                // If it is a texture, add it to our TXD.
                bool hasBeenAddedToTXD = false;

                GenericRTTI *rttiObj = RwTypeSystem::GetTypeStructFromObject( rwObj );

                RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rttiObj );

                if ( engineInterface->typeSystem.IsTypeInheritingFrom( engineInterface->textureTypeInfo, typeInfo ) )
                {
                    TextureBase *texture = (TextureBase*)rwObj;

                    texture->AddToDictionary( txdObj );

                    hasBeenAddedToTXD = true;
                }

                // If it has not been added, delete it.
                if ( hasBeenAddedToTXD == false )
                {
                    engineInterface->DeleteRwObject( rwObj );
                }
            }
            else
            {
                std::string pushWarning;

                if ( errDebugMsg.empty() == false )
                {
                    pushWarning = "texture native reading failure: ";
                    pushWarning += errDebugMsg;
                }
                else
                {
                    pushWarning = "failed to deserialize texture native block in texture dictionary";
                }

                engineInterface->PushWarning( pushWarning.c_str() );
            }
        }
    }

    // Read extensions.
    engineInterface->DeserializeExtensions( txdObj, inputProvider );
}

TexDictionary::TexDictionary( const TexDictionary& right ) : RwObject( right )
{
    // Create a new dictionary with all the textures.
    this->hasRecommendedPlatform = right.hasRecommendedPlatform;
    
    this->numTextures = 0;

    LIST_CLEAR( textures.root );

    Interface *engineInterface = right.engineInterface;

    LIST_FOREACH_BEGIN( TextureBase, right.textures.root, texDictNode )

        TextureBase *texture = item;

        // Clone the texture and insert it into us.
        TextureBase *newTex = (TextureBase*)engineInterface->CloneRwObject( texture );

        if ( newTex )
        {
            newTex->AddToDictionary( this );
        }

    LIST_FOREACH_END
}

TexDictionary::~TexDictionary( void )
{
    // Delete all textures that are part of this dictionary.
    while ( LIST_EMPTY( this->textures.root ) == false )
    {
        TextureBase *theTexture = LIST_GETITEM( TextureBase, this->textures.root.next, texDictNode );

        // Delete us.
        // This should automatically remove us from this TXD.
        this->engineInterface->DeleteRwObject( theTexture );
    }
}

static PluginDependantStructRegister <texDictionaryStreamPlugin, RwInterfaceFactory_t> texDictionaryStreamStore;

void initializeTXDEnvironment( Interface *theInterface )
{
    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( (EngineInterface*)theInterface );

    if ( txdStream )
    {
        theInterface->RegisterSerialization( CHUNK_TEXDICTIONARY, txdStream->txdTypeInfo, txdStream, RWSERIALIZE_ISOF );
    }
}

void shutdownTXDEnvironment( Interface *theInterface )
{
    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( (EngineInterface*)theInterface );

    if ( txdStream )
    {
        theInterface->UnregisterSerialization( CHUNK_TEXDICTIONARY, txdStream->txdTypeInfo, txdStream );
    }
}

void TexDictionary::clear(void)
{
	// We remove the links of all textures inside of us.
    while ( LIST_EMPTY( this->textures.root ) == false )
    {
        TextureBase *texture = LIST_GETITEM( TextureBase, this->textures.root.next, texDictNode );

        // Call the texture's own removal.
        texture->RemoveFromDictionary();
    }
}

TexDictionary* CreateTexDictionary( Interface *engineInterface )
{
    TexDictionary *texDictOut = NULL;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( txdStream )
    {
        texDictOut = txdStream->CreateTexDictionary( engineInterface );
    }

    return texDictOut;
}

TexDictionary* ToTexDictionary( Interface *engineInterface, RwObject *rwObj )
{
    TexDictionary *texDictOut = NULL;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( txdStream )
    {
        texDictOut = txdStream->ToTexDictionary( engineInterface, rwObj );
    }

    return texDictOut;
}

/*
 * Texture Base
 */

TextureBase::TextureBase( const TextureBase& right ) : RwObject( right )
{
    // General cloning business.
    this->texRaster = AcquireRaster( right.texRaster );
    this->name = right.name;
    this->maskName = right.maskName;
    this->filterMode = right.filterMode;
    this->uAddressing = right.uAddressing;
    this->vAddressing = right.vAddressing;
    
    // We do not want to belong to a TXD by default.
    // Even if the original texture belonged to one.
    this->texDict = NULL;
}

TextureBase::~TextureBase( void )
{
    // Clear our raster.
    this->SetRaster( NULL );

    // Make sure we are not in a texture dictionary.
    this->RemoveFromDictionary();
}

void TextureBase::SetRaster( Raster *texRaster )
{
    // If we had a previous raster, unlink it.
    if ( Raster *prevRaster = this->texRaster )
    {
        DeleteRaster( prevRaster );

        this->texRaster = NULL;
    }

    if ( texRaster )
    {
        // We get a new reference to the raster.
        this->texRaster = AcquireRaster( texRaster );

        if ( Raster *ourRaster = this->texRaster )
        {
            // Set the virtual version of this texture.
            this->objVersion = ourRaster->GetEngineVersion();
        }
    }
}

void TextureBase::AddToDictionary( TexDictionary *dict )
{
    this->RemoveFromDictionary();

    // Note: original RenderWare performs an insert, not an append.
    // I switched this around, so that textures stay in the correct order.
    LIST_APPEND( dict->textures.root, texDictNode );

    dict->numTextures++;

    this->texDict = dict;
}

void TextureBase::RemoveFromDictionary( void )
{
    TexDictionary *belongingTXD = this->texDict;

    if ( belongingTXD != NULL )
    {
        LIST_REMOVE( this->texDictNode );

        belongingTXD->numTextures--;

        this->texDict = NULL;
    }
}

TexDictionary* TextureBase::GetTexDictionary( void ) const
{
    return this->texDict;
}

TextureBase* CreateTexture( Interface *engineInterface, Raster *texRaster )
{
    TextureBase *textureOut = NULL;

    if ( RwTypeSystem::typeInfoBase *textureTypeInfo = engineInterface->textureTypeInfo )
    {
        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, textureTypeInfo, NULL );

        if ( rtObj )
        {
            textureOut = (TextureBase*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            // Set the raster into the texture.
            textureOut->SetRaster( texRaster );
        }
    }

    return textureOut;
}

TextureBase* ToTexture( Interface *engineInterface, RwObject *rwObj )
{
    if ( isRwObjectInheritingFrom( engineInterface, rwObj, engineInterface->textureTypeInfo ) )
    {
        return (TextureBase*)rwObj;
    }

    return NULL;
}

/*
 * Native Texture
 */

static PlatformTexture* CreateNativeTexture( Interface *engineInterface, RwTypeSystem::typeInfoBase *nativeTexType )
{
    PlatformTexture *texOut = NULL;
    {
        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, nativeTexType, NULL );

        if ( rtObj )
        {
            texOut = (PlatformTexture*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
        }
    }
    return texOut;
}

static PlatformTexture* CloneNativeTexture( Interface *engineInterface, const PlatformTexture *srcNativeTex )
{
    PlatformTexture *texOut = NULL;
    {
        const GenericRTTI *srcRtObj = engineInterface->typeSystem.GetTypeStructFromConstAbstractObject( srcNativeTex );

        if ( srcRtObj )
        {
            GenericRTTI *dstRtObj = engineInterface->typeSystem.Clone( engineInterface, srcRtObj );

            if ( dstRtObj )
            {
                texOut = (PlatformTexture*)RwTypeSystem::GetObjectFromTypeStruct( dstRtObj );
            }
        }
    }
    return texOut;
}

static void DeleteNativeTexture( Interface *engineInterface, PlatformTexture *nativeTexture )
{
    GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( nativeTexture );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

uint32 GetNativeTextureMipmapCount( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider )
{
    nativeTextureBatchedInfo info;

    texTypeProvider->GetTextureInfo( engineInterface, nativeTexture, info );

    return info.mipmapCount;
}

inline void GetNativeTextureBaseDimensions( Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider, uint32& baseWidth, uint32& baseHeight )
{
    nativeTextureBatchedInfo info;

    texTypeProvider->GetTextureInfo( engineInterface, nativeTexture, info );

    baseWidth = info.baseWidth;
    baseHeight = info.baseHeight;
}

inline bool GetNativeTextureRawBitmapData(
    Interface *engineInterface, PlatformTexture *nativeTexture, texNativeTypeProvider *texTypeProvider, uint32 mipIndex, bool supportsPalette, rawBitmapFetchResult& rawBitmapOut
)
{
    bool fetchSuccessful = false;

    // Fetch mipmap data result and convert it to raw texels.
    rawMipmapLayer mipData;

    bool gotMipLayer = texTypeProvider->GetMipmapLayer( engineInterface, nativeTexture, mipIndex, mipData );

    if ( gotMipLayer )
    {
        // Get the important data onto the stack.
        uint32 width = mipData.mipData.width;
        uint32 height = mipData.mipData.height;

        uint32 layerWidth = mipData.mipData.mipWidth;
        uint32 layerHeight = mipData.mipData.mipHeight;

        void *srcTexels = mipData.mipData.texels;
        uint32 dataSize = mipData.mipData.dataSize;

        eRasterFormat rasterFormat = mipData.rasterFormat;
        uint32 depth = mipData.depth;
        eColorOrdering colorOrder = mipData.colorOrder;
        ePaletteType paletteType = mipData.paletteType;
        void *paletteData = mipData.paletteData;
        uint32 paletteSize = mipData.paletteSize;
        
        eCompressionType compressionType = mipData.compressionType;

        bool isNewlyAllocated = mipData.isNewlyAllocated;

        // If we are not raw compressed yet, then we have to make us raw compressed.
        if ( compressionType != RWCOMPRESS_NONE )
        {
            // Compresion types do not support a valid source raster format.
            // We should determine one.
            eRasterFormat targetRasterFormat;
            uint32 targetDepth;

            if ( compressionType == RWCOMPRESS_DXT1 && mipData.hasAlpha == false )
            {
                targetRasterFormat = RASTER_888;
                targetDepth = 32;
            }
            else
            {
                targetRasterFormat = RASTER_8888;
                targetDepth = 32;
            }

            uint32 uncWidth, uncHeight;

            void *dstTexels = NULL;
            uint32 dstDataSize = 0;

            bool doConvert =
                ConvertMipmapLayerNative(
                    engineInterface,
                    width, height, layerWidth, layerHeight, srcTexels, dataSize,
                    rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                    targetRasterFormat, targetDepth, colorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                    false,
                    uncWidth, uncHeight,
                    dstTexels, dstDataSize
                );

            assert( doConvert == true );

            if ( isNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );
            }

            // Update stuff.
            width = uncWidth;
            height = uncHeight;

            srcTexels = dstTexels;
            dataSize = dstDataSize;

            rasterFormat = targetRasterFormat;
            depth = targetDepth;

            compressionType = RWCOMPRESS_NONE;

            isNewlyAllocated = true;
        }

        // Filter out palette if requested.
        if ( paletteType != PALETTE_NONE && supportsPalette == false )
        {
            uint32 palWidth, palHeight;

            void *dstPalTexels = NULL;
            uint32 dstPalDataSize = 0;

            uint32 dstDepth = Bitmap::getRasterFormatDepth(rasterFormat);

            bool hasConverted =
                ConvertMipmapLayerNative(
                    engineInterface,
                    width, height, layerWidth, layerHeight, srcTexels, dataSize,
                    rasterFormat, depth, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                    rasterFormat, dstDepth, colorOrder, PALETTE_NONE, NULL, 0, compressionType,
                    false,
                    palWidth, palHeight,
                    dstPalTexels, dstPalDataSize
                );

            if ( isNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );
                engineInterface->PixelFree( paletteData );
            }

            width = palWidth;
            height = palHeight;

            srcTexels = dstPalTexels;
            dataSize = dstPalDataSize;

            depth = dstDepth;

            paletteType = PALETTE_NONE;
            paletteData = NULL;
            paletteSize = 0;

            isNewlyAllocated = true;
        }

        // Put data into the output.
        rawBitmapOut.texelData = srcTexels;
        rawBitmapOut.dataSize = dataSize;
        
        rawBitmapOut.width = layerWidth;
        rawBitmapOut.height = layerHeight;

        rawBitmapOut.isNewlyAllocated = isNewlyAllocated;

        rawBitmapOut.depth = depth;
        rawBitmapOut.rasterFormat = rasterFormat;
        rawBitmapOut.colorOrder = colorOrder;
        rawBitmapOut.paletteData = paletteData;
        rawBitmapOut.paletteSize = paletteSize;
        rawBitmapOut.paletteType = paletteType;

        // Success.
        fetchSuccessful = true;
    }

    return fetchSuccessful;
}

struct nativeTextureStreamPlugin : public serializationProvider
{
    inline void Initialize( Interface *engineInterface )
    {
        this->platformTexType = engineInterface->typeSystem.RegisterAbstractType <PlatformTexture> ( "native_texture" );

        // Initialize the list that will keep all native texture types.
        LIST_CLEAR( this->texNativeTypes.root );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        // Unregister all type providers.
        LIST_FOREACH_BEGIN( texNativeTypeProvider, this->texNativeTypes.root, managerData.managerNode )
            // We just set them to false.
            item->managerData.isRegistered = false;
        LIST_FOREACH_END

        LIST_CLEAR( this->texNativeTypes.root );

        if ( RwTypeSystem::typeInfoBase *platformTexType = this->platformTexType )
        {
            engineInterface->typeSystem.DeleteType( platformTexType );

            this->platformTexType = NULL;
        }
    }

    void Serialize( Interface *engineInterface, BlockProvider& outputProvider, RwObject *objectToStore ) const
    {
        // Make sure we are a valid texture.
        if ( !isRwObjectInheritingFrom( engineInterface, objectToStore, engineInterface->textureTypeInfo ) )
        {
            throw RwException( "invalid RwObject at texture serialization (not a texture base)" );
        }

        TextureBase *theTexture = (TextureBase*)objectToStore;

        // Fetch the raster, which is the virtual interface to the platform texel data.
        if ( Raster *texRaster = theTexture->GetRaster() )
        {
            // The raster also requires GPU native data, the heart of the texture.
            if ( PlatformTexture *nativeTex = texRaster->platformData )
            {
                // Get the type information of this texture and find its type provider.
                GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( nativeTex );

                RwTypeSystem::typeInfoBase *nativeTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

                // Attempt to cast the type interface to our native texture type provider.
                // If successful, it indeeed is a native texture.
                {
                    nativeTextureCustomTypeInterface *nativeTexTypeInterface = dynamic_cast <nativeTextureCustomTypeInterface*> ( nativeTypeInfo->tInterface );

                    if ( nativeTexTypeInterface )
                    {
                        texNativeTypeProvider *texNativeProvider = nativeTexTypeInterface->texTypeProvider;

                        // Set the version to the version of the native texture.
                        // Texture objects only have a 'virtual' version.
                        LibraryVersion nativeVersion = texNativeProvider->GetTextureVersion( nativeTex );

                        outputProvider.setBlockVersion( nativeVersion );

                        // Serialize the texture.
                        texNativeProvider->SerializeTexture( theTexture, nativeTex, outputProvider );
                    }
                    else
                    {
                        throw RwException( "could not serialize texture: native data is not valid" );
                    }
                }
            }
            else
            {
                throw RwException( "could not serialize texture: no native data" );
            }
        }
        else
        {
            throw RwException( "could not serialize texture: no raster attached" );
        }
    }

    struct interestedNativeType
    {
        eTexNativeCompatibility typeOfInterest;
        texNativeTypeProvider *interestedParty;
    };

    struct QueuedWarningHandler : public WarningHandler
    {
        void OnWarningMessage( const std::string& theMessage )
        {
            this->message_list.push_back( theMessage );
        }

        typedef std::vector <std::string> messages_t;

        messages_t message_list;
    };

    void Deserialize( Interface *engineInterface, BlockProvider& inputProvider, RwObject *objectToDeserialize ) const
    {
        // This is a pretty complicated algorithm that will need revision later on, when networked streams are allowed.
        // It is required because tex native rules have been violated by War Drum Studios.
        // First, we need to analyze the given block; this is done by getting candidates from texNativeTypes that
        // have an interest in this block.
        typedef std::vector <interestedNativeType> interestList_t;

        interestList_t interestedTypeProviders;

        LIST_FOREACH_BEGIN( texNativeTypeProvider, this->texNativeTypes.root, managerData.managerNode )

            // Reset the input provider.
            inputProvider.seek( 0, RWSEEK_BEG );

            // Check this block's compatibility and if it is something, register it.
            eTexNativeCompatibility thisCompat = RWTEXCOMPAT_NONE;

            try
            {
                thisCompat = item->IsCompatibleTextureBlock( inputProvider );
            }
            catch( RwException& )
            {
                // If there was any exception, there is no point in selecting this provider
                // as a valid candidate.
                thisCompat = RWTEXCOMPAT_NONE;
            }

            if ( thisCompat != RWTEXCOMPAT_NONE )
            {
                interestedNativeType nativeInfo;
                nativeInfo.typeOfInterest = thisCompat;
                nativeInfo.interestedParty = item;

                interestedTypeProviders.push_back( nativeInfo );
            }

        LIST_FOREACH_END

        // Check whether the interest is valid.
        // There may only be one full-on interested party, but there can be multiple "maybe".
        struct providerInfo_t
        {
            inline providerInfo_t( void )
            {
                this->theProvider = NULL;
            }

            texNativeTypeProvider *theProvider;
            
            QueuedWarningHandler _warningQueue;
        };

        typedef std::vector <providerInfo_t> providers_t;

        providers_t maybeProviders;

        texNativeTypeProvider *definiteProvider = NULL;

        for ( interestList_t::const_iterator iter = interestedTypeProviders.begin(); iter != interestedTypeProviders.end(); iter++ )
        {
            const interestedNativeType& theInterest = *iter;

            eTexNativeCompatibility compatType = theInterest.typeOfInterest;

            if ( compatType == RWTEXCOMPAT_MAYBE )
            {
                providerInfo_t providerInfo;

                providerInfo.theProvider = theInterest.interestedParty;

                maybeProviders.push_back( providerInfo );
            }
            else if ( compatType == RWTEXCOMPAT_ABSOLUTE )
            {
                if ( definiteProvider == NULL )
                {
                    definiteProvider = theInterest.interestedParty;
                }
                else
                {
                    throw RwException( "texture native block compatibility conflict" );
                }
            }
        }

        // If we have no providers that recognized that texture block, we tell the runtime.
        if ( definiteProvider == NULL && maybeProviders.empty() )
        {
            throw RwException( "unknown texture native block" );
        }

        // If we have only one maybe provider, we set it as definite provider.
        if ( definiteProvider == NULL && maybeProviders.size() == 1 )
        {
            definiteProvider = maybeProviders.front().theProvider;

            maybeProviders.clear();
        }

        // If we have a definite provider, it is elected to parse the block.
        // Otherwise we try going through all "maybe" providers and select the first successful one.
        TextureBase *texOut = (TextureBase*)objectToDeserialize;

        // We will need a raster which is an interface to the native GPU data.
        // It serves as a container, so serialization will not access it directly.
        Raster *texRaster = CreateRaster( engineInterface );

        if ( texRaster )
        {
            // We require to allocate a platform texture, so lets keep a pointer.
            PlatformTexture *platformData = NULL;

            try
            {
                if ( definiteProvider != NULL )
                {
                    // If we have a definite provider, we do not need special warning dispatching.
                    // Good for us.

                    // Create a native texture for this provider.
                    platformData = CreateNativeTexture( engineInterface, definiteProvider->managerData.rwTexType );

                    if ( platformData )
                    {
                        // Set the version of the native texture.
                        definiteProvider->SetTextureVersion( engineInterface, platformData, inputProvider.getBlockVersion() );

                        // Attempt to deserialize the native texture.
                        inputProvider.seek( 0, RWSEEK_BEG );

                        definiteProvider->DeserializeTexture( texOut, platformData, inputProvider );
                    }
                }
                else
                {
                    // Loop through all maybe providers.
                    bool hasSuccessfullyDeserialized = false;

                    providerInfo_t *successfulProvider = NULL;

                    for ( providers_t::iterator iter = maybeProviders.begin(); iter != maybeProviders.end(); iter++ )
                    {
                        providerInfo_t& thisInfo = *iter;

                        texNativeTypeProvider *theProvider = thisInfo.theProvider;

                        // Just attempt deserialization.
                        bool success = false;

                        // For any warning that has been performed during this process, we need to queue it.
                        // In case we succeeded serializing an object, we just output the warning of its runtime.
                        // On failure we warn the runtime that the deserialization was ambiguous.
                        // Then we output warnings in sections, after the native type names.
                        WarningHandler *currentWarningHandler = &thisInfo._warningQueue;

                        GlobalPushWarningHandler( engineInterface, currentWarningHandler );

                        PlatformTexture *nativeData = NULL;

                        try
                        {
                            // We create a native texture for this provider.
                            nativeData = CreateNativeTexture( engineInterface, theProvider->managerData.rwTexType );

                            if ( nativeData )
                            {
                                try
                                {
                                    // Set the version of the native data.
                                    theProvider->SetTextureVersion( engineInterface, nativeData, inputProvider.getBlockVersion() );

                                    inputProvider.seek( 0, RWSEEK_BEG );

                                    try
                                    {
                                        theProvider->DeserializeTexture( texOut, nativeData, inputProvider );

                                        success = true;
                                    }
                                    catch( RwException& theError )
                                    {
                                        // We failed, try another deserialization.
                                        success = false;

                                        DeleteNativeTexture( engineInterface, nativeData );

                                        // We push this error as warning.
                                        if ( theError.message.size() != 0 )
                                        {
                                            engineInterface->PushWarning( theError.message );
                                        }
                                    }
                                }
                                catch( ... )
                                {
                                    // This catch is required if we encounter any other exception that wrecks the runtime
                                    // We do not want any memory leaks.
                                    DeleteNativeTexture( engineInterface, nativeData );

                                    nativeData = NULL;

                                    throw;
                                }
                            }
                            else
                            {
                                engineInterface->PushWarning( "failed to allocate native texture data for texture deserialization" );
                            }
                        }
                        catch( ... )
                        {
                            // We kinda failed somewhere, so lets unregister our warning handler.
                            GlobalPopWarningHandler( engineInterface );

                            throw;
                        }

                        GlobalPopWarningHandler( engineInterface );

                        if ( success )
                        {
                            successfulProvider = &thisInfo;
                            
                            hasSuccessfullyDeserialized = true;

                            // Give the native data to the runtime.
                            platformData = nativeData;
                            break;
                        }
                    }

                    if ( !hasSuccessfullyDeserialized )
                    {
                        // We need to inform the user of the warnings that he might have missed.
                        if ( maybeProviders.size() > 1 )
                        {
                            engineInterface->PushWarning( "ambiguous texture native block!" );
                        }

                        // Output all warnings in sections.
                        for ( providers_t::const_iterator iter = maybeProviders.begin(); iter != maybeProviders.end(); iter++ )
                        {
                            const providerInfo_t& thisInfo = *iter;

                            const QueuedWarningHandler& warningQueue = thisInfo._warningQueue;

                            texNativeTypeProvider *typeProvider = thisInfo.theProvider;

                            // Create a buffered warning output.
                            std::string typeWarnBuf;

                            typeWarnBuf += "[";
                            typeWarnBuf += typeProvider->managerData.rwTexType->name;
                            typeWarnBuf += "]:";

                            if ( warningQueue.message_list.empty() )
                            {
                                typeWarnBuf += "no warnings.";
                            }
                            else
                            {
                                typeWarnBuf += "\n";

                                bool isFirstItem = true;

                                for ( QueuedWarningHandler::messages_t::const_iterator iter = warningQueue.message_list.begin(); iter != warningQueue.message_list.end(); iter++ )
                                {
                                    if ( !isFirstItem )
                                    {
                                        typeWarnBuf += "\n";
                                    }

                                    typeWarnBuf += *iter;

                                    if ( isFirstItem )
                                    {
                                        isFirstItem = false;
                                    }
                                }
                            }

                            engineInterface->PushWarning( typeWarnBuf );
                        }

                        // On failure, just bail.
                    }
                    else
                    {
                        // Just output the warnings of the successful provider.
                        for ( QueuedWarningHandler::messages_t::const_iterator iter = successfulProvider->_warningQueue.message_list.begin(); iter != successfulProvider->_warningQueue.message_list.end(); iter++ )
                        {
                            engineInterface->PushWarning( *iter );
                        }
                    }
                }
            }
            catch( ... )
            {
                // If there was any exception, just pass it on.
                // We clear the texture beforehand.
                DeleteRaster( texRaster );

                texRaster = NULL;

                if ( platformData )
                {
                    DeleteNativeTexture( engineInterface, platformData );

                    platformData = NULL;
                }

                throw;
            }

            // Attempt to link all the data together.
            bool texLinkSuccess = false;

            if ( platformData )   
            {
                // We link the raster with the texture and put the platform data into the raster.
                texRaster->platformData = platformData;

                texOut->SetRaster( texRaster );

                // We clear our reference from the raster.
                DeleteRaster( texRaster );

                // We succeeded!
                texLinkSuccess = true;
            }

            if ( texLinkSuccess == false )
            {
                // Delete platform data if we had one.
                if ( platformData )
                {
                    DeleteNativeTexture( engineInterface, platformData );

                    platformData = NULL;
                }

                // Since we have no more texture object to store the raster into, we delete the raster.
                DeleteRaster( texRaster );

                throw RwException( "failed to link the raster object" );
            }
        }
        else
        {
            throw RwException( "failed to allocate raster object for deserialization" );
        }
    }

    struct nativeTextureCustomTypeInterface : public RwTypeSystem::typeInterface
    {
        void Construct( void *mem, Interface *engineInterface, void *construct_params ) const
        {
            this->texTypeProvider->ConstructTexture( this->engineInterface, mem, this->actualObjSize );
        }

        void CopyConstruct( void *mem, const void *srcMem ) const
        {
            this->texTypeProvider->CopyConstructTexture( this->engineInterface, mem, srcMem, this->actualObjSize );
        }

        void Destruct( void *mem ) const
        {
            this->texTypeProvider->DestroyTexture( this->engineInterface, mem, this->actualObjSize );
        }

        size_t GetTypeSize( Interface *engineInterface, void *construct_params ) const
        {
            return this->actualObjSize;
        }

        size_t GetTypeSizeByObject( Interface *engineInterface, const void *objMem ) const
        {
            return this->actualObjSize;
        }

        Interface *engineInterface;
        texNativeTypeProvider *texTypeProvider;
        size_t actualObjSize;
    };

    bool RegisterNativeTextureType( Interface *engineInterface, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize )
    {
        bool registerSuccess = false;

        if ( typeProvider->managerData.isRegistered == false )
        {
            RwTypeSystem::typeInfoBase *platformTexType = this->platformTexType;

            if ( platformTexType != NULL )
            {
                // Register this type.
                nativeTextureCustomTypeInterface *newNativeTypeInterface = _newstruct <nativeTextureCustomTypeInterface> ( *engineInterface->typeSystem._memAlloc );

                if ( newNativeTypeInterface )
                {
                    // Set up our type.
                    newNativeTypeInterface->engineInterface = engineInterface;
                    newNativeTypeInterface->texTypeProvider = typeProvider;
                    newNativeTypeInterface->actualObjSize = memSize;

                    RwTypeSystem::typeInfoBase *newType = NULL;

                    try
                    {
                        newType = engineInterface->typeSystem.RegisterCommonTypeInterface( nativeName, newNativeTypeInterface, platformTexType );
                    }
                    catch( ... )
                    {
                        _delstruct <nativeTextureCustomTypeInterface> ( newNativeTypeInterface, *engineInterface->typeSystem._memAlloc );

                        registerSuccess = false;
                    }

                    // Alright, register us.
                    if ( newType )
                    {
                        typeProvider->managerData.rwTexType = newType;

                        LIST_APPEND( this->texNativeTypes.root, typeProvider->managerData.managerNode );

                        typeProvider->managerData.isRegistered = true;

                        registerSuccess = true;
                    }
                }
            }
            else
            {
                engineInterface->PushWarning( "tried to register native texture type with no platform type around" );
            }
        }

        return registerSuccess;
    }

    bool UnregisterNativeTextureType( Interface *engineInterface, const char *nativeName )
    {
        bool unregisterSuccess = false;

        // Try removing the type with said name.
        {
            RwTypeSystem::typeInfoBase *nativeTypeInfo = engineInterface->typeSystem.FindTypeInfo( nativeName, this->platformTexType );

            if ( nativeTypeInfo && nativeTypeInfo->IsImmutable() == false )
            {
                // We can cast the type interface to get the type provider.
                nativeTextureCustomTypeInterface *nativeTypeInterface = dynamic_cast <nativeTextureCustomTypeInterface*> ( nativeTypeInfo->tInterface );

                if ( nativeTypeInterface )
                {
                    texNativeTypeProvider *texProvider = nativeTypeInterface->texTypeProvider;

                    // Remove it.
                    LIST_REMOVE( texProvider->managerData.managerNode );

                    texProvider->managerData.isRegistered = false;

                    // Delete the type.
                    engineInterface->typeSystem.DeleteType( nativeTypeInfo );
                }
            }
        }

        return unregisterSuccess;
    }
    
    RwTypeSystem::typeInfoBase *platformTexType;

    RwList <texNativeTypeProvider> texNativeTypes;
};

static PluginDependantStructRegister <nativeTextureStreamPlugin, RwInterfaceFactory_t> nativeTextureStreamStore;

// Texture native type registrations.
void registerD3DNativeTexture( Interface *engineInterface );
void registerXBOXNativeTexture( Interface *engineInterface );
void registerPS2NativeTexture( Interface *engineInterface );
void registerMobileDXTNativeTexture( Interface *engineInterface );
void registerATCNativeTexture( Interface *engineInterface );
void registerPVRNativeTexture( Interface *engineInterface );
void registerMobileUNCNativeTexture( Interface *engineInterface );

void initializeNativeTextureEnvironment( Interface *engineInterface )
{
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Register the native texture stream plugin.
        engineInterface->RegisterSerialization( CHUNK_TEXTURENATIVE, engineInterface->textureTypeInfo, nativeTexEnv, RWSERIALIZE_INHERIT );
    }

    // Register sub environments.
    registerD3DNativeTexture( engineInterface );
    registerXBOXNativeTexture( engineInterface );
    registerPS2NativeTexture( engineInterface );
    registerMobileDXTNativeTexture( engineInterface );
    registerATCNativeTexture( engineInterface );
    registerPVRNativeTexture( engineInterface );
    registerMobileUNCNativeTexture( engineInterface );
}

void shutdownNativeTextureEnvironment( Interface *engineInterface )
{
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Unregister us again.
        engineInterface->UnregisterSerialization( CHUNK_TEXTURENATIVE, engineInterface->textureTypeInfo, nativeTexEnv );
    }
}

bool RegisterNativeTextureType( Interface *engineInterface, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize )
{
    bool success = false;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        success = nativeTexEnv->RegisterNativeTextureType( engineInterface, nativeName, typeProvider, memSize );
    }

    return success;
}

bool UnregisterNativeTextureType( Interface *engineInterface, const char *nativeName )
{
    bool success = false;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        success = nativeTexEnv->UnregisterNativeTextureType( engineInterface, nativeName );
    }

    return success;
}

texNativeTypeProvider* GetNativeTextureTypeProvider( Interface *engineInterface, void *objMem )
{
    texNativeTypeProvider *platformData = NULL;

    // We first need the native texture type environment.
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Attempt to get the type info of the native data.
        GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( objMem );

        if ( rtObj )
        {
            RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

            // Check that we indeed are a native texture type.
            // This is guarranteed if we inherit from the native texture type info.
            if ( engineInterface->typeSystem.IsTypeInheritingFrom( nativeTexEnv->platformTexType, typeInfo ) )
            {
                // We assume that the type provider is of our native type.
                // For safety, do a dynamic cast.
                nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeTypeInterface =
                    dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( typeInfo->tInterface );

                if ( nativeTypeInterface )
                {
                    // Return the type provider.
                    platformData = nativeTypeInterface->texTypeProvider;
                }
            }
        }
    }

    return platformData;
}

void pixelDataTraversal::FreePixels( Interface *engineInterface )
{
    if ( this->isNewlyAllocated )
    {
        uint32 mipmapCount = this->mipmaps.size();

        for ( uint32 n = 0; n < mipmapCount; n++ )
        {
            mipmapResource& thisLayer = this->mipmaps[ n ];

            if ( void *texels = thisLayer.texels )
            {
                engineInterface->PixelFree( texels );

                thisLayer.texels = NULL;
            }
        }

        this->mipmaps.clear();

        if ( void *paletteData = this->paletteData )
        {
            engineInterface->PixelFree( paletteData );

            this->paletteData = NULL;
        }

        this->isNewlyAllocated = false;
    }
}

void pixelDataTraversal::CloneFrom( Interface *engineInterface, const pixelDataTraversal& right )
{
    // Free any previous data.
    this->FreePixels( engineInterface );

    // Clone parameters.
    eRasterFormat rasterFormat = right.rasterFormat;

    this->isNewlyAllocated = true;  // since we clone
    this->rasterFormat = rasterFormat;
    this->depth = right.depth;
    this->colorOrder = right.colorOrder;

    // Clone palette.
    this->paletteType = right.paletteType;
    
    void *srcPaletteData = right.paletteData;
    void *dstPaletteData = NULL;

    uint32 dstPaletteSize = 0;

    if ( srcPaletteData )
    {
        uint32 srcPaletteSize = right.paletteSize;  
        
        // Copy the palette texels.
        uint32 palRasterDepth = Bitmap::getRasterFormatDepth( rasterFormat );

        uint32 palDataSize = getRasterDataSize( srcPaletteSize, palRasterDepth );

        dstPaletteData = engineInterface->PixelAllocate( palDataSize );

        memcpy( dstPaletteData, srcPaletteData, palDataSize );

        dstPaletteSize = srcPaletteSize;
    }

    this->paletteData = dstPaletteData;
    this->paletteSize = dstPaletteSize;

    // Clone mipmaps.
    uint32 mipmapCount = right.mipmaps.size();

    this->mipmaps.resize( mipmapCount );

    for ( uint32 n = 0; n < mipmapCount; n++ )
    {
        const mipmapResource& srcLayer = right.mipmaps[ n ];

        mipmapResource newLayer;

        newLayer.width = srcLayer.width;
        newLayer.height = srcLayer.height;

        newLayer.mipWidth = srcLayer.mipWidth;
        newLayer.mipHeight = srcLayer.mipHeight;

        // Copy the mipmap layer texels.
        uint32 mipDataSize = srcLayer.dataSize;

        const void *srcTexels = srcLayer.texels;

        void *newtexels = engineInterface->PixelAllocate( mipDataSize );

        memcpy( newtexels, srcTexels, mipDataSize );

        newLayer.texels = newtexels;
        newLayer.dataSize = mipDataSize;

        // Store this layer.
        this->mipmaps[ n ] = newLayer;
    }

    // Clone non-trivial parameters.
    this->compressionType = right.compressionType;
    this->hasAlpha = right.hasAlpha;
    this->autoMipmaps = right.autoMipmaps;
    this->cubeTexture = right.cubeTexture;
    this->rasterType = right.rasterType;
}

void pixelDataTraversal::mipmapResource::Free( Interface *engineInterface )
{
    // Free the data here, since we have the Interface struct defined.
    if ( void *ourTexels = this->texels )
    {
        engineInterface->PixelFree( ourTexels );

        // We have no more texels.
        this->texels = NULL;
    }
}

void CompatibilityTransformPixelData( Interface *engineInterface, pixelDataTraversal& pixelData, const pixelCapabilities& pixelCaps )
{
    // Make sure the pixelData does not violate the capabilities struct.
    // This is done by "downcasting". It preserves maximum image quality, but increases memory requirements.

    // First get the original parameters onto stack.
    eRasterFormat srcRasterFormat = pixelData.rasterFormat;
    uint32 srcDepth = pixelData.depth;
    eColorOrdering srcColorOrder = pixelData.colorOrder;
    ePaletteType srcPaletteType = pixelData.paletteType;
    void *srcPaletteData = pixelData.paletteData;
    uint32 srcPaletteSize = pixelData.paletteSize;
    eCompressionType srcCompressionType = pixelData.compressionType;

    uint32 srcMipmapCount = pixelData.mipmaps.size();

    // Now decide the target format depending on the capabilities.
    eRasterFormat dstRasterFormat = srcRasterFormat;
    uint32 dstDepth = srcDepth;
    eColorOrdering dstColorOrder = srcColorOrder;
    ePaletteType dstPaletteType = srcPaletteType;
    void *dstPaletteData = srcPaletteData;
    uint32 dstPaletteSize = srcPaletteSize;
    eCompressionType dstCompressionType = srcCompressionType;

    bool hasBeenModded = false;

    if ( dstCompressionType == RWCOMPRESS_DXT1 && pixelCaps.supportsDXT1 == false ||
         dstCompressionType == RWCOMPRESS_DXT2 && pixelCaps.supportsDXT2 == false ||
         dstCompressionType == RWCOMPRESS_DXT3 && pixelCaps.supportsDXT3 == false ||
         dstCompressionType == RWCOMPRESS_DXT4 && pixelCaps.supportsDXT4 == false ||
         dstCompressionType == RWCOMPRESS_DXT5 && pixelCaps.supportsDXT5 == false )
    {
        // Set proper decompression parameters.
        uint32 dxtType;

        bool isDXT = IsDXTCompressionType( dstCompressionType, dxtType );

        assert( isDXT == true );

        eRasterFormat targetRasterFormat = getDXTDecompressionRasterFormat( engineInterface, dxtType, pixelData.hasAlpha );

        dstRasterFormat = targetRasterFormat;
        dstDepth = Bitmap::getRasterFormatDepth( targetRasterFormat );
        dstColorOrder = COLOR_BGRA;
        dstPaletteType = PALETTE_NONE;
        dstPaletteData = NULL;
        dstPaletteSize = 0;

        // We decompress stuff.
        dstCompressionType = RWCOMPRESS_NONE;

        hasBeenModded = true;
    }

    if ( hasBeenModded == false )
    {
        if ( dstPaletteType != PALETTE_NONE && pixelCaps.supportsPalette == false )
        {
            // We want to do things without a palette.
            dstPaletteType = PALETTE_NONE;
            dstPaletteSize = 0;
            dstPaletteData = NULL;

            dstDepth = Bitmap::getRasterFormatDepth(dstRasterFormat);

            hasBeenModded = true;
        }
    }

    // Check whether we even want an update.
    bool wantsUpdate = false;

    if ( srcRasterFormat != dstRasterFormat || dstDepth != srcDepth || dstColorOrder != srcColorOrder ||
         dstPaletteType != srcPaletteType || dstPaletteData != srcPaletteData || dstPaletteSize != srcPaletteSize ||
         dstCompressionType != srcCompressionType )
    {
        wantsUpdate = true;
    }

    if ( wantsUpdate )
    {
        // Convert the pixels now.
        bool hasUpdated;
        {
            // Create a destination format struct.
            pixelFormat dstPixelFormat;

            dstPixelFormat.rasterFormat = dstRasterFormat;
            dstPixelFormat.depth = dstDepth;
            dstPixelFormat.colorOrder = dstColorOrder;
            dstPixelFormat.paletteType = dstPaletteType;
            dstPixelFormat.compressionType = dstCompressionType;

            hasUpdated = ConvertPixelData( engineInterface, pixelData, dstPixelFormat );
        }

        // If we want an update, we should get an update.
        // Otherwise, ConvertPixelData is to blame.
        assert( hasUpdated == true );

        // If we have updated at all, apply changes.
        if ( hasUpdated )
        {
            // We must have the correct parameters.
            // Here we verify problematic parameters only.
            // Params like rasterFormat are expected to be handled properly no matter what.
            assert( pixelData.compressionType == dstCompressionType );
        }
    }
}

inline RwTypeSystem::typeInfoBase* GetNativeTextureType( Interface *engineInterface, const char *typeName )
{
    RwTypeSystem::typeInfoBase *typeInfo = NULL;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        if ( RwTypeSystem::typeInfoBase *nativeTexType = nativeTexEnv->platformTexType )
        {
            typeInfo = engineInterface->typeSystem.FindTypeInfo( typeName, nativeTexType );
        }
    }

    return typeInfo;
}

bool ConvertRasterTo( Raster *theRaster, const char *nativeName )
{
    bool conversionSuccess = false;

    Interface *engineInterface = theRaster->engineInterface;

    // First get the native texture environment.
    // This is used to browse for convertible types.
    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        // Only convert if the raster has image data.
        if ( PlatformTexture *nativeTex = theRaster->platformData )
        {
            // Get the type information of the original platform data.
            GenericRTTI *origNativeRtObj = RwTypeSystem::GetTypeStructFromObject( nativeTex );

            RwTypeSystem::typeInfoBase *origTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( origNativeRtObj );

            // Get the type information of the destination format.
            RwTypeSystem::typeInfoBase *dstTypeInfo = GetNativeTextureType( engineInterface, nativeName );

            if ( origTypeInfo != NULL && dstTypeInfo != NULL )
            {
                // If the destination type and the source type match, we are finished.
                if ( engineInterface->typeSystem.IsSameType( origTypeInfo, dstTypeInfo ) )
                {
                    conversionSuccess = true;
                }
                else
                {
                    // Attempt to get the native texture type interface for both types.
                    nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *origTypeInterface = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( origTypeInfo->tInterface );
                    nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *dstTypeInterface = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( dstTypeInfo->tInterface );

                    // Only proceed if both could resolve.
                    if ( origTypeInterface != NULL && dstTypeInterface != NULL )
                    {
                        // Use the original type provider to grab pixel data from the texture.
                        // Then get the pixel capabilities of both formats and convert the pixel data into a compatible format for the destination format.
                        // Finally, apply the pixels to the destination format texture.
                        // * PERFORMANCE: at best, it can fetch pixel data (without allocation), free the original texture, allocate the new texture and put the pixels to it.
                        // * this would be a simple move operation. the actual operation depends on the complexity of both formats.
                        texNativeTypeProvider *origTypeProvider = origTypeInterface->texTypeProvider;
                        texNativeTypeProvider *dstTypeProvider = dstTypeInterface->texTypeProvider;

                        // In case of an exception, we have to deal with the pixel information, so we do not leak memory.
                        pixelDataTraversal pixelStore;

                        // 1. Fetch the pixel data.
                        origTypeProvider->GetPixelDataFromTexture( engineInterface, nativeTex, pixelStore );

                        try
                        {
                            // 2. detach the pixel data from the texture and free it.
                            //    free the pixels if we got a private copy.
                            origTypeProvider->UnsetPixelDataFromTexture( engineInterface, nativeTex, ( pixelStore.isNewlyAllocated == true ) );

                            // Since we are the only owners of pixelData now, inform it.
                            pixelStore.SetStandalone();

                            // 3. Allocate a new texture.
                            PlatformTexture *newNativeTex = CreateNativeTexture( engineInterface, dstTypeInfo );

                            if ( newNativeTex )
                            {
                                // 4. Get the pixel capabilities of the target resource and put the texels we fetched in
                                //    the highest quality format possible.
                                pixelCapabilities dstSurfaceCaps;

                                dstTypeProvider->GetPixelCapabilities( dstSurfaceCaps );

                                // TODO: make pixels compatible for the target format.
                                // * First decide what pixel format we have to deduce from the capabilities
                                //   and then call the "ConvertPixelData" function to do the job.
                                CompatibilityTransformPixelData( engineInterface, pixelStore, dstSurfaceCaps );

                                // 5. Put the texels into our texture.
                                //    Throwing an exception here means that the texture did not apply any of the pixel
                                //    information. We can safely free pixelStore.
                                texNativeTypeProvider::acquireFeedback_t acquireFeedback;

                                dstTypeProvider->SetPixelDataToTexture( engineInterface, newNativeTex, pixelStore, acquireFeedback );

                                if ( acquireFeedback.hasDirectlyAcquired == false )
                                {
                                    // We need to release the pixels from the storage.
                                    pixelStore.FreePixels( engineInterface );
                                }
                                else
                                {
                                    // Since the texture now owns the pixels, we just detach.
                                    pixelStore.DetachPixels();
                                }

                                // 6. Link the new native texture!
                                //    Also delete the old one.
                                DeleteNativeTexture( engineInterface, nativeTex );

                                theRaster->platformData = newNativeTex;

                                // We are successful!
                                conversionSuccess = true;
                            }
                            else
                            {
                                conversionSuccess = false;
                            }
                        }
                        catch( ... )
                        {
                            conversionSuccess = false;
                        }

                        if ( conversionSuccess == false )
                        {
                            // We failed at doing our task.
                            // Terminate any resource that allocated.
                            pixelStore.FreePixels( engineInterface );
                        }
                    }
                }
            }
        }
    }

    return conversionSuccess;
}

platformTypeNameList_t GetAvailableNativeTextureTypes( Interface *engineInterface )
{
    platformTypeNameList_t registeredTypes;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( (EngineInterface*)engineInterface );

    if ( nativeTexEnv )
    {
        if ( RwTypeSystem::typeInfoBase *nativeTexType = nativeTexEnv->platformTexType )
        {
            // We need to iterate through all types and return the ones that directly inherit from our native tex type.
            for ( RwTypeSystem::type_iterator iter = engineInterface->typeSystem.GetTypeIterator(); iter.IsEnd() == false; iter.Increment() )
            {
                RwTypeSystem::typeInfoBase *theType = iter.Resolve();

                if ( theType->inheritsFrom == nativeTexType )
                {
                    registeredTypes.push_back( theType->name );
                }
            }
        }
    }
    
    return registeredTypes;
}

/*
 * Raster
 */

Raster* CreateRaster( Interface *engineInterface )
{
    RwTypeSystem::typeInfoBase *rasterTypeInfo = engineInterface->rasterTypeInfo;

    if ( rasterTypeInfo )
    {
        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, rasterTypeInfo, NULL );

        if ( rtObj )
        {
            Raster *theRaster = (Raster*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            return theRaster;
        }
    }
    else
    {
        engineInterface->PushWarning( "no raster type info present in CreateRaster" );
    }

    return NULL;
}

Raster* AcquireRaster( Raster *theRaster )
{
    // Attempts to get a handle to this raster by referencing it.
    // This function could fail if the resource has reached its maximum refcount.

    Raster *returnObj = NULL;

    if ( theRaster )
    {
        // TODO: implement ref count overflow security check.

        theRaster->refCount++;

        returnObj = theRaster;
    }

    return returnObj;
}

void DeleteRaster( Raster *theRaster )
{
    Interface *engineInterface = theRaster->engineInterface;

    // We use reference counting on rasters.
    theRaster->refCount--;

    if ( theRaster->refCount == 0 )
    {
        // Just delete it.
        GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromAbstractObject( theRaster );

        if ( rtObj )
        {
            engineInterface->typeSystem.Destroy( engineInterface, rtObj );
        }
        else
        {
            engineInterface->PushWarning( "invalid raster object pushed to DeleteRaster" );
        }
    }
}

Raster::Raster( const Raster& right )
{
    // Copy raster specifics.
    this->engineInterface = right.engineInterface;

    // Copy native platform data.
    PlatformTexture *platformTex = NULL;

    if ( right.platformData )
    {
        platformTex = CloneNativeTexture( this->engineInterface, right.platformData );
    }

    this->platformData = platformTex;

    this->refCount = 1;
}

Raster::~Raster( void )
{
    // Delete the platform data, if available.
    this->clearNativeData();

    assert( this->refCount == 0 );
}

void Raster::SetEngineVersion( LibraryVersion version )
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    texProvider->SetTextureVersion( engineInterface, platformTex, version );
}

LibraryVersion Raster::GetEngineVersion( void ) const
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    return texProvider->GetTextureVersion( platformTex );
}

void Raster::getFormatString( char *buf, size_t bufSize, size_t& lengthOut ) const
{
    // Ask the native platform texture to deliver us a format string.
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    const texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    texProvider->GetTextureFormatString( engineInterface, platformTex, buf, bufSize, lengthOut );
}

void Raster::convertToFormat(eRasterFormat newFormat)
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no platform data" );
    }

    Interface *engineInterface = this->engineInterface;

    // Get the type provider of the native data.
    texNativeTypeProvider *typeProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !typeProvider )
    {
        throw RwException( "invalid native data" );
    }

    // Fetch the entire pixel data from this texture and convert it.
    pixelDataTraversal pixelData;

    typeProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    bool werePixelsAllocated = pixelData.isNewlyAllocated;

    // Delete any pixels that the texture had previously.
    typeProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, werePixelsAllocated == true );

    pixelData.SetStandalone();

    try
    {
        eRasterFormat rasterFormat = pixelData.rasterFormat;
        ePaletteType paletteType = pixelData.paletteType;
        eColorOrdering colorOrder = pixelData.colorOrder;

        bool isPaletteRaster = ( paletteType != PALETTE_NONE );

        if ( isPaletteRaster || newFormat != rasterFormat )
        {
            pixelFormat dstFormat;
            dstFormat.rasterFormat = newFormat;
            dstFormat.depth = Bitmap::getRasterFormatDepth( newFormat );
            dstFormat.colorOrder = colorOrder;
            dstFormat.paletteType = PALETTE_NONE;
            dstFormat.compressionType = RWCOMPRESS_NONE;

            // Convert the pixels.
            bool hasUpdated = ConvertPixelData( engineInterface, pixelData, dstFormat );

            if ( hasUpdated )
            {
                // Put the texels back into the texture.
                texNativeTypeProvider::acquireFeedback_t acquireFeedback;

                typeProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

                // Free pixel data if required.
                if ( acquireFeedback.hasDirectlyAcquired == false )
                {
                    pixelData.FreePixels( engineInterface );
                }
                else
                {
                    pixelData.DetachPixels();
                }
            }
        }
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }
}

Bitmap Raster::getBitmap(void) const
{
    Interface *engineInterface = this->engineInterface;

    Bitmap resultBitmap;
    {
        PlatformTexture *platformTex = this->platformData;

        // If it has any mipmap at all.
        texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

        uint32 mipmapCount = GetNativeTextureMipmapCount( engineInterface, platformTex, texProvider );

        if ( mipmapCount > 0 )
        {
            uint32 width;
            uint32 height;
            uint32 depth;
            eRasterFormat theFormat;
            eColorOrdering theOrder;
            
            // Get the color data.
            bool hasAllocatedNewPixelData = false;
            void *pixelData = NULL;

            uint32 texDataSize;

            {
                rawBitmapFetchResult rawBitmap;

                bool gotPixelData = GetNativeTextureRawBitmapData( engineInterface, platformTex, texProvider, 0, false, rawBitmap );

                if ( gotPixelData )
                {
                    width = rawBitmap.width;
                    height = rawBitmap.height;
                    texDataSize = rawBitmap.dataSize;
                    depth = rawBitmap.depth;
                    theFormat = rawBitmap.rasterFormat;
                    theOrder = rawBitmap.colorOrder;
                    hasAllocatedNewPixelData = rawBitmap.isNewlyAllocated;
                    pixelData = rawBitmap.texelData;
                }
            }

            if ( pixelData != NULL )
            {
                // Set the data into the bitmap.
                resultBitmap.setImageData(
                    pixelData, theFormat, theOrder, depth, width, height, texDataSize
                );

                if ( hasAllocatedNewPixelData )
                {
                    engineInterface->PixelFree( pixelData );
                }
            }
        }
    }

    return resultBitmap;
}

void Raster::setImageData(const Bitmap& srcImage)
{
    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == NULL )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    // Delete old image data.
    texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, true );

    if ( srcImage.getDataSize() == 0 )
    {
        // Empty shit.
        return;
    }

    // Create a copy of the bitmap pixel data and put it into a traversal struct.
    pixelDataTraversal::mipmapResource newLayer;

    void *dstTexels = srcImage.copyPixelData();
    uint32 dstDataSize = srcImage.getDataSize();

    // Set the new texel data.
    uint32 newWidth, newHeight;
    uint32 newDepth = srcImage.getDepth();
    eRasterFormat newFormat = srcImage.getFormat();
    eColorOrdering newColorOrdering = srcImage.getColorOrder();

    srcImage.getSize( newWidth, newHeight );

    newLayer.width = newWidth;
    newLayer.height = newHeight;

    // Since it is raw image data, layer dimm is same as regular dimm.
    newLayer.mipWidth = newWidth;
    newLayer.mipHeight = newHeight;

    newLayer.texels = dstTexels;
    newLayer.dataSize = dstDataSize;

    pixelDataTraversal pixelData;

    pixelData.rasterFormat = newFormat;
    pixelData.depth = newDepth;
    pixelData.colorOrder = newColorOrdering;

    pixelData.paletteType = PALETTE_NONE;
    pixelData.paletteData = NULL;
    pixelData.paletteSize = 0;

    pixelData.compressionType = RWCOMPRESS_NONE;

    pixelData.mipmaps.push_back( newLayer );

    pixelData.isNewlyAllocated = true;
    pixelData.hasAlpha = calculateHasAlpha( pixelData );
    pixelData.rasterType = 4;   // Bitmap.
    pixelData.autoMipmaps = false;
    pixelData.cubeTexture = false;

    // Push the data to the texture.
    texNativeTypeProvider::acquireFeedback_t acquireFeedback;

    texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

    if ( acquireFeedback.hasDirectlyAcquired == false )
    {
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void Raster::resize(uint32 width, uint32 height)
{
    Bitmap mainBitmap = this->getBitmap();

    Bitmap targetBitmap( mainBitmap.getDepth(), mainBitmap.getFormat(), mainBitmap.getColorOrder() );

    targetBitmap.setSize(width, height);

    targetBitmap.drawBitmap(
        mainBitmap, 0, 0, width, height,
        Bitmap::SHADE_ZERO, Bitmap::SHADE_ONE, Bitmap::BLEND_ADDITIVE
    );

    this->setImageData( targetBitmap );
}

void Raster::getSize(uint32& width, uint32& height) const
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    GetNativeTextureBaseDimensions( engineInterface, platformTex, texProvider, width, height );
}

void TextureBase::improveFiltering(void)
{
    // Handle stuff depending on our current settings.
    eRasterStageFilterMode currentFilterMode = this->filterMode;

    eRasterStageFilterMode newFilterMode = currentFilterMode;

    if ( currentFilterMode == RWFILTER_POINT )
    {
        newFilterMode = RWFILTER_LINEAR;
    }
    else if ( currentFilterMode == RWFILTER_POINT_POINT )
    {
        newFilterMode = RWFILTER_POINT_LINEAR;
    }

    // Update our texture fields.
    if ( currentFilterMode != newFilterMode )
    {
        this->filterMode = newFilterMode;
    }
}

void Raster::newNativeData( const char *typeName )
{
    if ( this->platformData != NULL )
        return;

    Interface *engineInterface = this->engineInterface;

    RwTypeSystem::typeInfoBase *nativeTypeInfo = GetNativeTextureType( engineInterface, typeName );

    if ( nativeTypeInfo )
    {
        // Create a new native data.
        PlatformTexture *nativeTex = CreateNativeTexture( engineInterface, nativeTypeInfo );
    
        // Store stuff.
        this->platformData = nativeTex;
    }
}

void Raster::clearNativeData( void )
{
    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == NULL )
        return;

    Interface *engineInterface = this->engineInterface;

    DeleteNativeTexture( engineInterface, platformTex );

    // We have no more native data.
    this->platformData = NULL;
}

bool Raster::hasNativeDataOfType( const char *typeName ) const
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
        return false;

    Interface *engineInterface = this->engineInterface;

    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( platformTex );

    RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

    return ( strcmp( typeInfo->name, typeName ) == 0 );
}

const char* Raster::getNativeDataTypeName( void ) const
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
        return false;

    Interface *engineInterface = this->engineInterface;

    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( platformTex );

    RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

    return ( typeInfo->name );
}

void* Raster::getNativeInterface( void )
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
        return NULL;

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
        return NULL;

    return texProvider->GetNativeInterface( platformTex );
}

void Raster::optimizeForLowEnd(float quality)
{
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    // To make good decisions, we need the storage capabilities of the texture.
    storageCapabilities storeCaps;

    texProvider->GetStorageCapabilities( storeCaps );

    // Textures that should run on low end hardware should not be too HD.
    // This routine takes the PlayStation 2 as reference hardware.

    const uint32 maxTexWidth = 256;
    const uint32 maxTexHeight = 256;

    while ( true )
    {
        // Get properties of this texture first.
        uint32 texWidth, texHeight;

        nativeTextureBatchedInfo nativeInfo;

        texProvider->GetTextureInfo( engineInterface, platformTex, nativeInfo );

        texWidth = nativeInfo.baseWidth;
        texHeight = nativeInfo.baseHeight;

        // Optimize this texture.
        bool hasTakenStep = false;

        if ( !hasTakenStep && quality < 1.0f )
        {
            // We first must make sure that the texture is not unnecessaringly huge.
            if ( texWidth > maxTexWidth || texHeight > maxTexHeight )
            {
                // Half the texture size until we are at a suitable size.
                uint32 targetWidth = texWidth;
                uint32 targetHeight = texHeight;

                do
                {
                    targetWidth /= 2;
                    targetHeight /= 2;
                }
                while ( targetWidth > maxTexWidth || targetHeight > maxTexHeight );

                // The texture dimensions are too wide.
                // We half the texture in size.
                this->resize( targetWidth, targetHeight );

                hasTakenStep = true;
            }
        }
        
        if ( !hasTakenStep )
        {
            // Can we store palette data?
            if ( storeCaps.pixelCaps.supportsPalette == true )
            {
                // We should decrease the texture size by palettization.
                ePaletteType currentPaletteType = texProvider->GetTexturePaletteType( platformTex );

                if (currentPaletteType == PALETTE_NONE)
                {
                    ePaletteType targetPalette = ( quality > 0.0f ) ? ( PALETTE_8BIT ) : ( PALETTE_4BIT );

                    // TODO: per mipmap alpha checking.

                    if (texProvider->DoesTextureHaveAlpha( platformTex ) == false)
                    {
                        // 4bit palette is only feasible for non-alpha textures (at higher quality settings).
                        // Otherwise counting the colors is too complicated.
                        
                        // TODO: still allow 8bit textures.
                        targetPalette = PALETTE_4BIT;
                    }

                    // The texture should be palettized for good measure.
                    this->convertToPalette( targetPalette );

                    // TODO: decide whether 4bit or 8bit palette.

                    hasTakenStep = true;
                }
            }
        }

        if ( !hasTakenStep )
        {
            // The texture dimension is important for renderer performance. That is why we need to scale the texture according
            // to quality settings aswell.

        }
    
        if ( !hasTakenStep )
        {
            break;
        }
    }
}

void Raster::compress( float quality )
{
    // A pretty complicated algorithm that can be used to optimally compress rasters.
    // Currently this only supports DXT.

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RwException( "no native data" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native data" );
    }

    // The target raster may already be compressed or the target architecture does its own compression.
    // Decide about these situations.
    bool isAlreadyCompressed = texProvider->IsTextureCompressed( platformTex );

    if ( isAlreadyCompressed )
        return; // do not recompress textures.

    storageCapabilities storeCaps;

    texProvider->GetStorageCapabilities( storeCaps );

    if ( storeCaps.isCompressedFormat )
        return; // no point in compressing if the architecture will do it already.

    // We want to check what kind of compression the target architecture supports.
    // If we have any compression support, we proceed to next stage.
    bool supportsDXT1 = false;
    bool supportsDXT2 = false;
    bool supportsDXT3 = false;
    bool supportsDXT4 = false;
    bool supportsDXT5 = false;

    pixelCapabilities inputTransferCaps;

    texProvider->GetPixelCapabilities( inputTransferCaps );

    // Now decide upon things.
    if ( inputTransferCaps.supportsDXT1 && storeCaps.pixelCaps.supportsDXT1 )
    {
        supportsDXT1 = true;
    }

    if ( inputTransferCaps.supportsDXT2 && storeCaps.pixelCaps.supportsDXT2 )
    {
        supportsDXT2 = true;
    }

    if ( inputTransferCaps.supportsDXT3 && storeCaps.pixelCaps.supportsDXT3 )
    {
        supportsDXT3 = true;
    }

    if ( inputTransferCaps.supportsDXT4 && storeCaps.pixelCaps.supportsDXT4 )
    {
        supportsDXT4 = true;
    }

    if ( inputTransferCaps.supportsDXT5 && storeCaps.pixelCaps.supportsDXT5 )
    {
        supportsDXT5 = true;
    }

    if ( supportsDXT1 == false &&
         supportsDXT2 == false &&
         supportsDXT3 == false &&
         supportsDXT4 == false &&
         supportsDXT5 == false )
    {
        throw RwException( "attempted to compress a raster that does not support compression" );
    }

    // We need to know about the alpha status of the texture to make a good decision.
    bool texHasAlpha = texProvider->DoesTextureHaveAlpha( platformTex );

    // Decide now what compression we want.
    eCompressionType targetCompressionType = RWCOMPRESS_NONE;

    bool couldDecide =
        DecideBestDXTCompressionFormat(
            engineInterface,
            texHasAlpha,
            supportsDXT1, supportsDXT2, supportsDXT3, supportsDXT4, supportsDXT5,
            quality,
            targetCompressionType
        );

    if ( !couldDecide )
    {
        throw RwException( "could not decide on an optimal DXT compression type" );
    }

    // Since we now know about everything, we can take the pixels and perform the compression.
    pixelDataTraversal pixelData;

    texProvider->GetPixelDataFromTexture( engineInterface, platformTex, pixelData );

    bool hasDirectlyAcquired = false;

    try
    {
        // Unset the pixels from the texture and make them standalone.
        texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, pixelData.isNewlyAllocated == true );

        pixelData.SetStandalone();

        // Compress things.
        pixelFormat targetPixelFormat;
        targetPixelFormat.rasterFormat = pixelData.rasterFormat;
        targetPixelFormat.depth = pixelData.depth;
        targetPixelFormat.colorOrder = pixelData.colorOrder;
        targetPixelFormat.paletteType = PALETTE_NONE;
        targetPixelFormat.compressionType = targetCompressionType;

        bool hasCompressed = ConvertPixelData( engineInterface, pixelData, targetPixelFormat );

        if ( !hasCompressed )
        {
            throw RwException( "failed to compress raster" );
        }

        // Put the new pixels into the texture.
        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );

        hasDirectlyAcquired = acquireFeedback.hasDirectlyAcquired;
    }
    catch( ... )
    {
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( hasDirectlyAcquired == false )
    {
        // Should never happen.
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void Raster::writeTGA(const char *path, bool optimized)
{
    // If optimized == true, then this routine will output files in a commonly unsupported format
    // that is much smaller than in regular mode.
    // Problem with TGA is that it is poorly supported by most of the applications out there.

    Interface *engineInterface = this->engineInterface;

    streamConstructionFileParam_t fileParam( path );

    Stream *tgaStream = engineInterface->CreateStream( RWSTREAMTYPE_FILE, RWSTREAMMODE_READONLY, &fileParam );

	if ( !tgaStream )
    {
        engineInterface->PushWarning( "not writing file: " + std::string( path ) );
		return;
	}

    try
    {
        writeTGAStream(tgaStream, optimized);
    }
    catch( ... )
    {
        engineInterface->DeleteStream( tgaStream );

        throw;
    }

	engineInterface->DeleteStream( tgaStream );
}

void Raster::writeTGAStream(Stream *tgaStream, bool optimized)
{
    Interface *engineInterface = this->engineInterface;

    PlatformTexture *platformTex = this->platformData;

    if ( platformTex == NULL )
    {
        throw RwException( "no native data" );
    }

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RwException( "invalid native texture" );
    }

    // Get the mipmap from layer 0.
    rawMipmapLayer rawLayer;

    bool gotLayer = texProvider->GetMipmapLayer( engineInterface, platformTex, 0, rawLayer );

    if ( !gotLayer )
    {
        throw RwException( "failed to get mipmap layer zero data in TGA writing" );
    }

    // Push the mipmap to the imaging plugin.
    bool successfullyStored = SerializeMipmapLayer( tgaStream, "TGA", rawLayer );

    // TODO: what to do with the bool?

    // Free raw bitmap resources.
    if ( rawLayer.isNewlyAllocated )
    {
        engineInterface->PixelFree( rawLayer.mipData.texels );

        rawLayer.mipData.texels = NULL;
    }
}

inline bool isValidFilterMode( uint32 binaryFilterMode )
{
    if ( binaryFilterMode == RWFILTER_POINT ||
         binaryFilterMode == RWFILTER_LINEAR ||
         binaryFilterMode == RWFILTER_POINT_POINT ||
         binaryFilterMode == RWFILTER_LINEAR_POINT ||
         binaryFilterMode == RWFILTER_POINT_LINEAR ||
         binaryFilterMode == RWFILTER_LINEAR_LINEAR )
    {
        return true;
    }

    return false;
}

inline bool isValidTexAddressingMode( uint32 binary_addressing )
{
    if ( binary_addressing == RWTEXADDRESS_WRAP ||
         binary_addressing == RWTEXADDRESS_MIRROR ||
         binary_addressing == RWTEXADDRESS_CLAMP ||
         binary_addressing == RWTEXADDRESS_BORDER )
    {
        return true;
    }

    return false;
}

inline void SetFormatInfoToTexture(
    TextureBase& outTex,
    uint32 binaryFilterMode, uint32 binary_uAddressing, uint32 binary_vAddressing
)
{
    eRasterStageFilterMode rwFilterMode = RWFILTER_LINEAR;

    eRasterStageAddressMode rw_uAddressing = RWTEXADDRESS_WRAP;
    eRasterStageAddressMode rw_vAddressing = RWTEXADDRESS_WRAP;

    // Make sure they are valid.
    if ( isValidFilterMode( binaryFilterMode ) )
    {
        rwFilterMode = (eRasterStageFilterMode)binaryFilterMode;
    }
    else
    {
        outTex.engineInterface->PushWarning( "texture " + outTex.GetName() + " has an invalid filter mode; defaulting to linear" );
    }

    if ( isValidTexAddressingMode( binary_uAddressing ) )
    {
        rw_uAddressing = (eRasterStageAddressMode)binary_uAddressing;
    }
    else
    {
        outTex.engineInterface->PushWarning( "texture " + outTex.GetName() + " has an invalid uAddressing mode; defaulting to wrap" );
    }

    if ( isValidTexAddressingMode( binary_vAddressing ) )
    {
        rw_vAddressing = (eRasterStageAddressMode)binary_vAddressing;
    }
    else
    {
        outTex.engineInterface->PushWarning( "texture " + outTex.GetName() + " has an invalid vAddressing mode; defaulting to wrap" );
    }

    // Put the fields.
    outTex.SetFilterMode( rwFilterMode );
    outTex.SetUAddressing( rw_uAddressing );
    outTex.SetVAddressing( rw_vAddressing );
}

void texFormatInfo::parse(TextureBase& outTex) const
{
    // Read our fields, which are from a binary stream.
    uint32 binaryFilterMode = this->filterMode;

    uint32 binary_uAddressing = this->uAddressing;
    uint32 binary_vAddressing = this->vAddressing;

    SetFormatInfoToTexture(
        outTex,
        binaryFilterMode, binary_uAddressing, binary_vAddressing
    );
}

void texFormatInfo::set(const TextureBase& inTex)
{
    this->filterMode = (uint32)inTex.GetFilterMode();
    this->uAddressing = (uint32)inTex.GetUAddressing();
    this->vAddressing = (uint32)inTex.GetVAddressing();
    this->pad1 = 0;
}

void wardrumFormatInfo::parse(TextureBase& outTex) const
{
    // Read our fields, which are from a binary stream.
    uint32 binaryFilterMode = this->filterMode;

    uint32 binary_uAddressing = this->uAddressing;
    uint32 binary_vAddressing = this->vAddressing;

    SetFormatInfoToTexture(
        outTex,
        binaryFilterMode, binary_uAddressing, binary_vAddressing
    );
}

void wardrumFormatInfo::set(const TextureBase& inTex)
{
    this->filterMode = (uint32)inTex.GetFilterMode();
    this->uAddressing = (uint32)inTex.GetUAddressing();
    this->vAddressing = (uint32)inTex.GetVAddressing();
}

// Initializator for TXD plugins, as it cannot be done statically.
extern void registerATCNativePlugin( void );
extern void registerD3DNativePlugin( void );
extern void registerMobileDXTNativePlugin( void );
extern void registerPS2NativePlugin( void );
extern void registerPVRNativePlugin( void );
extern void registerMobileUNCNativePlugin( void );
extern void registerXBOXNativePlugin( void );

void registerTXDPlugins( void )
{
    texDictionaryStreamStore.RegisterPlugin( engineFactory );
    nativeTextureStreamStore.RegisterPlugin( engineFactory );

    // Now register sub module plugins.
    registerATCNativePlugin();
    registerD3DNativePlugin();
    registerMobileDXTNativePlugin();
    registerPS2NativePlugin();
    registerPVRNativePlugin();
    registerMobileUNCNativePlugin();
    registerXBOXNativePlugin();
}

}
