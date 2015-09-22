#include "StdInc.h"

#include <cstring>
#include <assert.h>
#include <bitset>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>
#include <algorithm>
#include <cmath>

#include "pixelformat.hxx"

#include "pixelutil.hxx"

#include "txdread.d3d.hxx"

#include "pluginutil.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.dxt.hxx"

#include "rwserialize.hxx"

#include "txdread.natcompat.hxx"

#include "txdread.rasterplg.hxx"

namespace rw
{

/*
 * Texture Dictionary
 */

TexDictionary* texDictionaryStreamPlugin::CreateTexDictionary( EngineInterface *engineInterface ) const
{
    GenericRTTI *rttiObj = engineInterface->typeSystem.Construct( engineInterface, this->txdTypeInfo, NULL );

    if ( rttiObj == NULL )
    {
        return NULL;
    }
    
    TexDictionary *txdObj = (TexDictionary*)RwTypeSystem::GetObjectFromTypeStruct( rttiObj );

    return txdObj;
}

inline bool isRwObjectInheritingFrom( EngineInterface *engineInterface, RwObject *rwObj, RwTypeSystem::typeInfoBase *baseType )
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

TexDictionary* texDictionaryStreamPlugin::ToTexDictionary( EngineInterface *engineInterface, RwObject *rwObj )
{
    if ( isRwObjectInheritingFrom( engineInterface, rwObj, this->txdTypeInfo ) )
    {
        return (TexDictionary*)rwObj;
    }

    return NULL;
}

void texDictionaryStreamPlugin::Deserialize( Interface *intf, BlockProvider& inputProvider, RwObject *objectToDeserialize ) const
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    // Cast our object.
    TexDictionary *txdObj = (TexDictionary*)objectToDeserialize;

    // Read the textures.
    {
        BlockProvider texDictMetaStructBlock( &inputProvider );

        uint32 textureBlockCount = 0;
        bool requiresRecommendedPlatform = true;

        texDictMetaStructBlock.EnterContext();

        try
        {
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
        }
        catch( ... )
        {
            texDictMetaStructBlock.LeaveContext();
            
            throw;
        }

        // We finished reading meta data.
        texDictMetaStructBlock.LeaveContext();

        txdObj->hasRecommendedPlatform = requiresRecommendedPlatform;

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

TexDictionary* CreateTexDictionary( Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    TexDictionary *texDictOut = NULL;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( engineInterface );

    if ( txdStream )
    {
        texDictOut = txdStream->CreateTexDictionary( engineInterface );
    }

    return texDictOut;
}

TexDictionary* ToTexDictionary( Interface *intf, RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    TexDictionary *texDictOut = NULL;

    texDictionaryStreamPlugin *txdStream = texDictionaryStreamStore.GetPluginStruct( engineInterface );

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

TextureBase* CreateTexture( Interface *intf, Raster *texRaster )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

TextureBase* ToTexture( Interface *intf, RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( isRwObjectInheritingFrom( engineInterface, rwObj, engineInterface->textureTypeInfo ) )
    {
        return (TextureBase*)rwObj;
    }

    return NULL;
}

/*
 * Native Texture
 */

static PlatformTexture* CreateNativeTexture( Interface *intf, RwTypeSystem::typeInfoBase *nativeTexType )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

static PlatformTexture* CloneNativeTexture( Interface *intf, const PlatformTexture *srcNativeTex )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

static void DeleteNativeTexture( Interface *intf, PlatformTexture *nativeTexture )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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
        uint32 rowAlignment = mipData.rowAlignment;
        eColorOrdering colorOrder = mipData.colorOrder;
        ePaletteType paletteType = mipData.paletteType;
        void *paletteData = mipData.paletteData;
        uint32 paletteSize = mipData.paletteSize;
        
        eCompressionType compressionType = mipData.compressionType;

        bool isNewlyAllocated = mipData.isNewlyAllocated;

        try
        {
            // If we are not raw compressed yet, then we have to make us raw compressed.
            if ( compressionType != RWCOMPRESS_NONE )
            {
                // Compresion types do not support a valid source raster format.
                // We should determine one.
                eRasterFormat targetRasterFormat;
                uint32 targetDepth;
                uint32 targetRowAlignment = 4;  // good measure.

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
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        targetRasterFormat, targetDepth, targetRowAlignment, colorOrder, paletteType, paletteData, paletteSize, RWCOMPRESS_NONE,
                        false,
                        uncWidth, uncHeight,
                        dstTexels, dstDataSize
                    );

                if ( doConvert == false )
                {
                    // We prefer C++ exceptions over assertions.
                    // This way, we actually handle mistakes that can always happen.
                    throw RwException( "failed to convert fetched mipmap layer to uncompressed format" );
                }

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
                rowAlignment = targetRowAlignment;

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
                        rasterFormat, depth, rowAlignment, colorOrder, paletteType, paletteData, paletteSize, compressionType,
                        rasterFormat, dstDepth, rowAlignment, colorOrder, PALETTE_NONE, NULL, 0, compressionType,
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
        }
        catch( ... )
        {
            // If there was any error during conversion, we must clean up.
            if ( isNewlyAllocated )
            {
                engineInterface->PixelFree( srcTexels );

                if ( paletteData != NULL )
                {
                    engineInterface->PixelFree( paletteData );
                }
            }

            throw;
        }

        // Put data into the output.
        rawBitmapOut.texelData = srcTexels;
        rawBitmapOut.dataSize = dataSize;
        
        rawBitmapOut.width = layerWidth;
        rawBitmapOut.height = layerHeight;

        rawBitmapOut.isNewlyAllocated = isNewlyAllocated;

        rawBitmapOut.depth = depth;
        rawBitmapOut.rowAlignment = rowAlignment;
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
    inline void Initialize( EngineInterface *engineInterface )
    {
        this->platformTexType = engineInterface->typeSystem.RegisterAbstractType <PlatformTexture> ( "native_texture" );

        // Initialize the list that will keep all native texture types.
        LIST_CLEAR( this->texNativeTypes.root );

        // Register us in the serialization manager.
        RegisterSerialization( engineInterface, CHUNK_TEXTURENATIVE, engineInterface->textureTypeInfo, this, RWSERIALIZE_INHERIT );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        // Unregister us again.
        UnregisterSerialization( engineInterface, CHUNK_TEXTURENATIVE, engineInterface->textureTypeInfo, this );

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

    void Serialize( Interface *intf, BlockProvider& outputProvider, RwObject *objectToStore ) const
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

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

    void Deserialize( Interface *intf, BlockProvider& inputProvider, RwObject *objectToDeserialize ) const
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

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
        void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
        {
            this->texTypeProvider->ConstructTexture( this->engineInterface, mem, this->actualObjSize );
        }

        void CopyConstruct( void *mem, const void *srcMem ) const override
        {
            this->texTypeProvider->CopyConstructTexture( this->engineInterface, mem, srcMem, this->actualObjSize );
        }

        void Destruct( void *mem ) const override
        {
            this->texTypeProvider->DestroyTexture( this->engineInterface, mem, this->actualObjSize );
        }

        size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override
        {
            return this->actualObjSize;
        }

        size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *objMem ) const override
        {
            return this->actualObjSize;
        }

        Interface *engineInterface;
        texNativeTypeProvider *texTypeProvider;
        size_t actualObjSize;
    };

    bool RegisterNativeTextureType( Interface *intf, const char *nativeName, texNativeTypeProvider *typeProvider, size_t memSize )
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

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

    bool UnregisterNativeTextureType( Interface *intf, const char *nativeName )
    {
        EngineInterface *engineInterface = (EngineInterface*)intf;

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

texNativeTypeProvider* GetNativeTextureTypeProvider( Interface *intf, void *objMem )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

inline RwTypeSystem::typeInfoBase* GetNativeTextureType( Interface *intf, const char *typeName )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    RwTypeSystem::typeInfoBase *typeInfo = NULL;

    nativeTextureStreamPlugin *nativeTexEnv = nativeTextureStreamStore.GetPluginStruct( engineInterface );

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

    EngineInterface *engineInterface = (EngineInterface*)theRaster->engineInterface;

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
                                try
                                {
                                    // Transfer the version of the raster.
                                    {
                                        LibraryVersion srcVersion = origTypeProvider->GetTextureVersion( nativeTex );

                                        dstTypeProvider->SetTextureVersion( engineInterface, newNativeTex, srcVersion );
                                    }

                                    // 4. make pixels compatible for the target format.
                                    // *  First decide what pixel format we have to deduce from the capabilities
                                    //    and then call the "ConvertPixelData" function to do the job.
                                    CompatibilityTransformPixelData( engineInterface, pixelStore, dstTypeProvider );

                                    // The texels have to obey size rules of the destination native texture.
                                    // So let us check what size rules we need, right?
                                    AdjustPixelDataDimensionsByFormat( engineInterface, dstTypeProvider, pixelStore );

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
                                }
                                catch( ... )
                                {
                                    // If any exception happened, we must clean up things.
                                    DeleteNativeTexture( engineInterface, newNativeTex );

                                    throw;
                                }

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
                            // We do not pass on exceptions.
                            // Do not rely on this tho.
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

void* GetNativeTextureDriverInterface( Interface *engineInterface, const char *typeName )
{
    void *intf = NULL;

    // Get the type that is associated with the given typeName.
    RwTypeSystem::typeInfoBase *theType = GetNativeTextureType( engineInterface, typeName );

    if ( theType )
    {
        // Ensure that we are a native texture type and get its manager.
        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeIntf = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( theType->tInterface );

        if ( nativeIntf )
        {
            // Get the interface pointer.
            intf = nativeIntf->texTypeProvider->GetDriverNativeInterface();
        }
    }

    return intf;
}

const char* GetNativeTextureImageFormatExtension( Interface *engineInterface, const char *typeName )
{
    const char *ext = NULL;

    // Get the type that is associated with the given typeName.
    RwTypeSystem::typeInfoBase *theType = GetNativeTextureType( engineInterface, typeName );

    if ( theType )
    {
        // Ensure that we are a native texture type and get its manager.
        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeIntf = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( theType->tInterface );

        if ( nativeIntf )
        {
            // Get the interface pointer.
            ext = nativeIntf->texTypeProvider->GetNativeImageFormatExtension();
        }
    }

    return ext;
}

platformTypeNameList_t GetAvailableNativeTextureTypes( Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

bool GetNativeTextureFormatInfo( Interface *intf, const char *nativeName, nativeRasterFormatInfo& infoOut )
{
    bool gotInfo = false;

    EngineInterface *engineInterface = (EngineInterface*)intf;

    // Get the type that is associated with the given typeName.
    RwTypeSystem::typeInfoBase *theType = GetNativeTextureType( engineInterface, nativeName );

    if ( theType )
    {
        // Ensure that we are a native texture type and get its manager.
        nativeTextureStreamPlugin::nativeTextureCustomTypeInterface *nativeIntf = dynamic_cast <nativeTextureStreamPlugin::nativeTextureCustomTypeInterface*> ( theType->tInterface );

        if ( nativeIntf )
        {
            // Alright, let us return info about it.
            texNativeTypeProvider *texProvider = nativeIntf->texTypeProvider;

            storageCapabilities formatStoreCaps;
            texProvider->GetStorageCapabilities( formatStoreCaps );

            infoOut.isCompressedFormat = formatStoreCaps.isCompressedFormat;
            infoOut.supportsDXT1 = formatStoreCaps.pixelCaps.supportsDXT1;
            infoOut.supportsDXT2 = formatStoreCaps.pixelCaps.supportsDXT2;
            infoOut.supportsDXT3 = formatStoreCaps.pixelCaps.supportsDXT3;
            infoOut.supportsDXT4 = formatStoreCaps.pixelCaps.supportsDXT4;
            infoOut.supportsDXT5 = formatStoreCaps.pixelCaps.supportsDXT5;
            infoOut.supportsPalette = formatStoreCaps.pixelCaps.supportsPalette;

            gotInfo = true;
        }
    }

    return gotInfo;
}

/*
    Raster helper API.
    So we have got standardized names in third-party programs.
*/

const char* GetRasterFormatStandardName( eRasterFormat theFormat )
{
    const char *name = "unknown";

    if ( theFormat == RASTER_DEFAULT )
    {
        name = "unspecified";
    }
    else if ( theFormat == RASTER_1555 )
    {
        name = "RASTER_1555";
    }
    else if ( theFormat == RASTER_565 )
    {
        name = "RASTER_565";
    }
    else if ( theFormat == RASTER_4444 )
    {
        name = "RASTER_4444";
    }
    else if ( theFormat == RASTER_LUM )
    {
        name = "RASTER_LUM";
    }
    else if ( theFormat == RASTER_8888 )
    {
        name = "RASTER_8888";
    }
    else if ( theFormat == RASTER_888 )
    {
        name = "RASTER_888";
    }
    else if ( theFormat == RASTER_16 )
    {
        name = "RASTER_16";
    }
    else if ( theFormat == RASTER_24 )
    {
        name = "RASTER_24";
    }
    else if ( theFormat == RASTER_32 )
    {
        name = "RASTER_32";
    }
    else if ( theFormat == RASTER_555 )
    {
        name = "RASTER_555";
    }
    // NEW FORMATS.
    else if ( theFormat == RASTER_LUM_ALPHA )
    {
        name = "RASTER_LUM_ALPHA";
    }

    return name;
}

eRasterFormat FindRasterFormatByName( const char *theName )
{
    // TODO: make this as human-friendly as possible!

    if ( stricmp( theName, "RASTER_1555" ) == 0 ||
         stricmp( theName, "1555" ) == 0 )
    {
        return RASTER_1555;
    }
    else if ( stricmp( theName, "RASTER_565" ) == 0 ||
              stricmp( theName, "565" ) == 0 )
    {
        return RASTER_565;
    }
    else if ( stricmp( theName, "RASTER_4444" ) == 0 ||
              stricmp( theName, "4444" ) == 0 )
    {
        return RASTER_4444;
    }
    else if ( stricmp( theName, "RASTER_LUM" ) == 0 ||
              stricmp( theName, "LUM" ) == 0 )
    {
        return RASTER_LUM;
    }
    else if ( stricmp( theName, "RASTER_8888" ) == 0 ||
              stricmp( theName, "8888" ) == 0 )
    {
        return RASTER_8888;
    }
    else if ( stricmp( theName, "RASTER_888" ) == 0 ||
              stricmp( theName, "888" ) == 0 )
    {
        return RASTER_888;
    }
    else if ( stricmp( theName, "RASTER_16" ) == 0 )
    {
        return RASTER_16;
    }
    else if ( stricmp( theName, "RASTER_24" ) == 0 )
    {
        return RASTER_24;
    }
    else if ( stricmp( theName, "RASTER_32" ) == 0 )
    {
        return RASTER_32;
    }
    else if ( stricmp( theName, "RASTER_555" ) == 0 ||
              stricmp( theName, "555" ) == 0 )
    {
        return RASTER_555;
    }

    // No idea.
    return RASTER_DEFAULT;
}

/*
 * Raster
 */

Raster* CreateRaster( Interface *intf )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

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

Raster* CloneRaster( const Raster *rasterToClone )
{
    EngineInterface *engineInterface = (EngineInterface*)rasterToClone->engineInterface;

    // We can clone generically.
    rw::Raster *newRaster = NULL;

    const GenericRTTI *srcRtObj = RwTypeSystem::GetTypeStructFromConstObject( rasterToClone );

    if ( srcRtObj )
    {
        GenericRTTI *clonedRtObj = engineInterface->typeSystem.Clone( engineInterface, srcRtObj );

        if ( clonedRtObj )
        {
            newRaster = (rw::Raster*)RwTypeSystem::GetObjectFromTypeStruct( clonedRtObj );
        }
    }

    return newRaster;
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
    EngineInterface *engineInterface = (EngineInterface*)theRaster->engineInterface;

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
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( &right ) );

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

// Most important raster plugin, the threading consistency.
rasterConsistencyRegister_t rasterConsistencyRegister;

void registerRasterConsistency( void )
{
    rasterConsistencyRegister.RegisterPlugin( engineFactory );
}

void Raster::SetEngineVersion( LibraryVersion version )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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

void Raster::getSizeRules( rasterSizeRules& rulesOut ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    // We need to fetch that from the native texture.
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

    nativeTextureSizeRules nativeSizeRules;
    texProvider->GetTextureSizeRules( platformTex, nativeSizeRules );

    // Give opaque data to the runtime.
    rulesOut.powerOfTwo = nativeSizeRules.powerOfTwo;
    rulesOut.squared = nativeSizeRules.squared;
    rulesOut.multipleOf = nativeSizeRules.multipleOf;
    rulesOut.multipleOfValue = nativeSizeRules.multipleOfValue;
    rulesOut.maximum = nativeSizeRules.maximum;
    rulesOut.maxVal = nativeSizeRules.maxVal;
}

bool rasterSizeRules::verifyDimensions( uint32 width, uint32 height ) const
{
    nativeTextureSizeRules nativeSizeRules;
    nativeSizeRules.powerOfTwo = this->powerOfTwo;
    nativeSizeRules.squared = this->squared;
    nativeSizeRules.multipleOf = this->multipleOf;
    nativeSizeRules.multipleOfValue = this->multipleOfValue;
    nativeSizeRules.maximum = this->maximum;
    nativeSizeRules.maxVal = this->maxVal;

    return nativeSizeRules.IsMipmapSizeValid( width, height );
}

void rasterSizeRules::adjustDimensions( uint32 width, uint32 height, uint32& newWidth, uint32& newHeight ) const
{
    nativeTextureSizeRules nativeSizeRules;
    nativeSizeRules.powerOfTwo = this->powerOfTwo;
    nativeSizeRules.squared = this->squared;
    nativeSizeRules.multipleOf = this->multipleOf;
    nativeSizeRules.multipleOfValue = this->multipleOfValue;
    nativeSizeRules.maximum = this->maximum;
    nativeSizeRules.maxVal = this->maxVal;

    nativeSizeRules.CalculateValidLayerDimensions( width, height, newWidth, newHeight );
}

void Raster::getFormatString( char *buf, size_t bufSize, size_t& lengthOut ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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

    // First check whether replacing is even worth it.
    {
        eRasterFormat rasterFormat = pixelData.rasterFormat;
        ePaletteType paletteType = pixelData.paletteType;
        eColorOrdering colorOrder = pixelData.colorOrder;

        bool isPaletteRaster = ( paletteType != PALETTE_NONE );
        bool isCompressed = ( pixelData.compressionType != RWCOMPRESS_NONE );

        if ( isCompressed || isPaletteRaster || newFormat != rasterFormat )
        {
            // We have now decided to replace the pixel data.
            // Delete any pixels that the texture had previously.
            typeProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, werePixelsAllocated == true );

            pixelData.SetStandalone();

            try
            {
                pixelFormat dstFormat;
                dstFormat.rasterFormat = newFormat;
                dstFormat.depth = Bitmap::getRasterFormatDepth( newFormat );
                dstFormat.rowAlignment = ( isCompressed ? 4 : pixelData.rowAlignment );
                dstFormat.colorOrder = colorOrder;
                dstFormat.paletteType = PALETTE_NONE;
                dstFormat.compressionType = RWCOMPRESS_NONE;

                // Convert the pixels.
                bool hasUpdated = ConvertPixelData( engineInterface, pixelData, dstFormat );

                if ( hasUpdated )
                {
                    // Make sure dimensions are correct.
                    AdjustPixelDataDimensionsByFormat( engineInterface, typeProvider, pixelData );

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
            catch( ... )
            {
                pixelData.FreePixels( engineInterface );

                throw;
            }
        }
    }
}

eRasterFormat Raster::getRasterFormat( void ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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

    return texProvider->GetTextureRasterFormat( platformTex );
}

Bitmap Raster::getBitmap(void) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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
            uint32 rowAlignment;
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
                    rowAlignment = rawBitmap.rowAlignment;
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
                    pixelData, theFormat, theOrder, depth, rowAlignment, width, height, texDataSize,
                    ( hasAllocatedNewPixelData == true )
                );
                
                // We do not have to free the raw bitmap, because we assign it if it was stand-alone.
            }
        }
    }

    return resultBitmap;
}

void Raster::setImageData(const Bitmap& srcImage)
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    void *dstTexels = srcImage.copyPixelData();
    uint32 dstDataSize = srcImage.getDataSize();

    if ( dstTexels == NULL )
    {
        throw RwException( "failed to allocate copy of Bitmap texel data for Raster acquisition" );
    }

    // Only valid if the block below has not thrown an exception.
    texNativeTypeProvider::acquireFeedback_t acquireFeedback;

    pixelDataTraversal pixelData;

    try
    {
        pixelDataTraversal::mipmapResource newLayer;

        // Set the new texel data.
        uint32 newWidth, newHeight;
        uint32 newDepth = srcImage.getDepth();
        uint32 newRowAlignment = srcImage.getRowAlignment();
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

        pixelData.rasterFormat = newFormat;
        pixelData.depth = newDepth;
        pixelData.rowAlignment = newRowAlignment;
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

        // We do not have to transform for capabilities, since the input is a raw mipmap layer.
        // Raw texture data must be accepted by every native texture format.

        // Sure the dimensions are correct.
        AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

        // Push the data to the texture.
        texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
    }
    catch( ... )
    {
        // If the acquisition has failed, we have to bail.
        pixelData.FreePixels( engineInterface );

        throw;
    }

    if ( acquireFeedback.hasDirectlyAcquired == false )
    {
        pixelData.FreePixels( engineInterface );
    }
    else
    {
        pixelData.DetachPixels();
    }
}

void TextureBase::improveFiltering(void)
{
    // This routine scaled up the filtering settings of this texture.
    // When rendered, this texture will appear smoother.

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

void TextureBase::fixFiltering(void)
{
    // Only do things if we have a raster.
    if ( Raster *texRaster = this->texRaster )
    {
        // Adjust filtering mode.
        eRasterStageFilterMode currentFilterMode = this->GetFilterMode();

        uint32 actualNewMipmapCount = texRaster->getMipmapCount();

        // We need to represent a correct filter state, depending on the mipmap count
        // of the native texture. This is required to enable mipmap rendering, when required!
        if ( actualNewMipmapCount > 1 )
        {
            if ( currentFilterMode == RWFILTER_POINT )
            {
                this->SetFilterMode( RWFILTER_POINT_POINT );
            }
            else if ( currentFilterMode == RWFILTER_LINEAR )
            {
                this->SetFilterMode( RWFILTER_LINEAR_LINEAR );
            }
        }
        else
        {
            if ( currentFilterMode == RWFILTER_POINT_POINT ||
                    currentFilterMode == RWFILTER_POINT_LINEAR )
            {
                this->SetFilterMode( RWFILTER_POINT );
            }
            else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                        currentFilterMode == RWFILTER_LINEAR_LINEAR )
            {
                this->SetFilterMode( RWFILTER_LINEAR );
            }
        }

        // TODO: if there is a filtering mode that does not make sense at all,
        // how should we handle it?
    }
}

void Raster::newNativeData( const char *typeName )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
        return NULL;

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
        return NULL;

    return texProvider->GetNativeInterface( platformTex );
}

void* Raster::getDriverNativeInterface( void )
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
        return NULL;

    Interface *engineInterface = this->engineInterface;

    const texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
        return NULL;

    return texProvider->GetDriverNativeInterface();
}

void Raster::writeImage(Stream *outputStream, const char *method)
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

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

    bool hasSerialized = false;

    // First we try to serialize in the native texture implementation format.
    const char *nativeImageImplExt = texProvider->GetNativeImageFormatExtension();

    if ( nativeImageImplExt != NULL && stricmp( nativeImageImplExt, method ) == 0 )
    {
        // We want to serialize using this.
        texProvider->SerializeNativeImage( engineInterface, outputStream, platformTex );

        hasSerialized = true;
    }

    // Last resort.
    if ( !hasSerialized )
    {
        // Get the mipmap from layer 0.
        rawMipmapLayer rawLayer;

        bool gotLayer = texProvider->GetMipmapLayer( engineInterface, platformTex, 0, rawLayer );

        if ( !gotLayer )
        {
            throw RwException( "failed to get mipmap layer zero data in image writing" );
        }

        try
        {
            // Push the mipmap to the imaging plugin.
            bool successfullyStored = SerializeMipmapLayer( outputStream, method, rawLayer );

            if ( successfullyStored == false )
            {
                throw RwException( "failed to serialize mipmap layer" );
            }
        }
        catch( ... )
        {
            if ( rawLayer.isNewlyAllocated )
            {
                engineInterface->PixelFree( rawLayer.mipData.texels );

                rawLayer.mipData.texels = NULL;
            }

            throw;
        }

        // Free raw bitmap resources.
        if ( rawLayer.isNewlyAllocated )
        {
            engineInterface->PixelFree( rawLayer.mipData.texels );

            rawLayer.mipData.texels = NULL;
        }

        hasSerialized = true;
    }
}

void Raster::readImage( rw::Stream *inputStream )
{
    scoped_rwlock_writer <rwlock> rasterConsistency( GetRasterLock( this ) );

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

    bool hasDeserialized = false;

    // We could be a native image format.
    const char *nativeImplExt = texProvider->GetNativeImageFormatExtension();

    if ( nativeImplExt != NULL )
    {
        int64 curStreamPtr = inputStream->tell();

        bool isOfFormat = false;

        try
        {
            isOfFormat = texProvider->IsNativeImageFormat( engineInterface, inputStream );
        }
        catch( RwException& )
        {
            isOfFormat = false;
        }

        // Restore seek pointer.
        inputStream->seek( curStreamPtr, RWSEEK_BEG );

        if ( isOfFormat )
        {
            // Alright, we want to deserialize then.
            // First clear the image data that might exist in our texture.
            texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, true );

            texProvider->DeserializeNativeImage( engineInterface, inputStream, platformTex );

            hasDeserialized = true;
        }
    }

    // Last resort.
    if ( !hasDeserialized )
    {
        // Attempt to get a mipmap layer from the stream.
        rawMipmapLayer rawImagingLayer;

        bool deserializeSuccess = DeserializeMipmapLayer( inputStream, rawImagingLayer );

        if ( !deserializeSuccess )
        {
            throw RwException( "unknown image format" );
        }

        try
        {
            // Delete image data that was previously at the texture.
            texProvider->UnsetPixelDataFromTexture( engineInterface, platformTex, true );
        }
        catch( ... )
        {
            // Free the raw mipmap layer.
            engineInterface->PixelFree( rawImagingLayer.mipData.texels );

            throw;
        }

        // Put the imaging layer into the pixel traversal struct.
        pixelDataTraversal pixelData;

        pixelData.mipmaps.resize( 1 );

        pixelDataTraversal::mipmapResource& mipLayer = pixelData.mipmaps[ 0 ];

        mipLayer.texels = rawImagingLayer.mipData.texels;
        mipLayer.width = rawImagingLayer.mipData.width;
        mipLayer.height = rawImagingLayer.mipData.height;
        mipLayer.mipWidth = rawImagingLayer.mipData.mipWidth;
        mipLayer.mipHeight = rawImagingLayer.mipData.mipHeight;
        mipLayer.dataSize = rawImagingLayer.mipData.dataSize;

        pixelData.rasterFormat = rawImagingLayer.rasterFormat;
        pixelData.depth = rawImagingLayer.depth;
        pixelData.rowAlignment = rawImagingLayer.rowAlignment;
        pixelData.colorOrder = rawImagingLayer.colorOrder;
        pixelData.paletteType = rawImagingLayer.paletteType;
        pixelData.paletteData = rawImagingLayer.paletteData;
        pixelData.paletteSize = rawImagingLayer.paletteSize;
        pixelData.compressionType = rawImagingLayer.compressionType;

        pixelData.hasAlpha = calculateHasAlpha( pixelData );
        pixelData.autoMipmaps = false;
        pixelData.cubeTexture = false;
        pixelData.rasterType = 4;   // bitmap raster.

        pixelData.isNewlyAllocated = true;

        texNativeTypeProvider::acquireFeedback_t acquireFeedback;

        try
        {
            // Make sure the pixel data is compatible.
            CompatibilityTransformPixelData( engineInterface, pixelData, texProvider );

            AdjustPixelDataDimensionsByFormat( engineInterface, texProvider, pixelData );

            // Set this to the texture now.
            texProvider->SetPixelDataToTexture( engineInterface, platformTex, pixelData, acquireFeedback );
        }
        catch( ... )
        {
            // We just free our shit.
            pixelData.FreePixels( engineInterface );

            throw;
        }

        if ( acquireFeedback.hasDirectlyAcquired == false )
        {
            // We need to release our pixels.
            pixelData.FreePixels( engineInterface );
        }
        else
        {
            pixelData.DetachPixels();
        }

        hasDeserialized = true;
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

void texFormatInfo::writeToBlock(BlockProvider& outputProvider) const
{
    texFormatInfo_serialized <endian::little_endian <uint32>> serStruct;
    serStruct.info = *(uint32*)this;

    outputProvider.writeStruct( serStruct );
}

void texFormatInfo::readFromBlock(BlockProvider& inputProvider)
{
    texFormatInfo_serialized <endian::little_endian <uint32>> serStruct;
    
    inputProvider.readStruct( serStruct );

    *(uint32*)this = serStruct.info;
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
#ifdef RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE
extern void registerATCNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
extern void registerD3D8NativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
extern void registerD3D9NativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9
#ifdef RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE
extern void registerMobileDXTNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
extern void registerPS2NativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
extern void registerPVRNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE
extern void registerMobileUNCNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX
extern void registerXBOXNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_XBOX
#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE
extern void registerGCNativePlugin( void );
#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE

// Sub modules.
void registerResizeFilteringEnvironment( void );

void registerTXDPlugins( void )
{
    // First register the main serialization plugins.
    // Those are responsible for detecting texture dictionaries and native textures in RW streams.
    // The sub module plugins depend on those.
    texDictionaryStreamStore.RegisterPlugin( engineFactory );
    nativeTextureStreamStore.RegisterPlugin( engineFactory );

    // Now register sub module plugins.
#ifdef RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE
    registerATCNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_ATC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D8
    registerD3D8NativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_D3D8
#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9
    registerD3D9NativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_D3D9
#ifdef RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE
    registerMobileDXTNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_S3TC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
    registerPS2NativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_PLAYSTATION2
#ifdef RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
    registerPVRNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_POWERVR_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE
    registerMobileUNCNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_UNC_MOBILE
#ifdef RWLIB_INCLUDE_NATIVETEX_XBOX
    registerXBOXNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_XBOX
#ifdef RWLIB_INCLUDE_NATIVETEX_GAMECUBE
    registerGCNativePlugin();
#endif //RWLIB_INCLUDE_NATIVETEX_GAMECUBE

    // Register pure sub modules.
    registerResizeFilteringEnvironment();
}

}
