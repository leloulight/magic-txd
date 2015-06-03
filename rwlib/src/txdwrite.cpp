#include <cstring>
#include <assert.h>
#include <math.h>

#include <StdInc.h>

#include "txdread.common.hxx"

namespace rw
{

void texDictionaryStreamPlugin::Serialize( Interface *engineInterface, BlockProvider& outputProvider, RwObject *objectToSerialize ) const
{
    // Make sure we got a TXD.
    GenericRTTI *rttiObj = RwTypeSystem::GetTypeStructFromObject( objectToSerialize );

    RwTypeSystem::typeInfoBase *theType = RwTypeSystem::GetTypeInfoFromTypeStruct( rttiObj );

    if ( engineInterface->typeSystem.IsSameType( theType, this->txdTypeInfo ) == false )
    {
        throw RwException( "invalid type at TXD serialization" );
    }

    const TexDictionary *txdObj = (const TexDictionary*)objectToSerialize;

    LibraryVersion version = outputProvider.getBlockVersion();

    uint32 numTextures = txdObj->numTextures;

    {
	    // Write the TXD meta info struct.
        BlockProvider txdMetaInfoBlock( &outputProvider );

        txdMetaInfoBlock.EnterContext();

        try
        {
            txdMetaInfoBlock.setBlockID( CHUNK_STRUCT );

            // Write depending on version.
            if (version.rwLibMinor <= 5)
            {
                txdMetaInfoBlock.writeUInt32(numTextures);
            }
            else
            {
                if ( numTextures > 0xFFFF )
                {
                    throw RwException( "texture dictionary has too many textures for writing" );
                }

                // Determine the recommended platform to give this TXD.
                // If we dont have any, we can write 0.
                uint16 recommendedPlatform = 0;

                if (txdObj->hasRecommendedPlatform)
                {
                    // A recommended platform can only be given if all textures are of the same platform.
                    // Otherwise it is misleading.
                    bool hasTexPlatform = false;
                    bool hasValidPlatform = true;
                    uint32 curRecommendedPlatform;

                    LIST_FOREACH_BEGIN( TextureBase, txdObj->textures.root, texDictNode )

                        TextureBase *tex = item;

                        Raster *texRaster = tex->GetRaster();

                        if ( texRaster )
                        {
                            // We can only determine the recommended platform if we have native data.
                            void *nativeObj = texRaster->platformData;

                            if ( nativeObj )
                            {
                                texNativeTypeProvider *typeProvider = GetNativeTextureTypeProvider( engineInterface, nativeObj );

                                if ( typeProvider )
                                {
                                    // Call the providers method to get the recommended driver.
                                    uint32 driverId = typeProvider->GetDriverIdentifier( nativeObj );

                                    if ( driverId != 0 )
                                    {
                                        // We want to ensure that all textures have the same recommended platform.
                                        // This will mean that a texture dictionary will be loadable for certain on one specialized architecture.
                                        if ( !hasTexPlatform )
                                        {
                                            curRecommendedPlatform = driverId;

                                            hasTexPlatform = true;
                                        }
                                        else
                                        {
                                            if ( curRecommendedPlatform != driverId )
                                            {
                                                // We found a driver conflict.
                                                // This means that we cannot recommend for any special driver.
                                                hasValidPlatform = false;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    LIST_FOREACH_END

                    // Set it.
                    if ( hasTexPlatform && hasValidPlatform )
                    {
                        // We are valid, so pass to runtime.
                        recommendedPlatform = curRecommendedPlatform;
                    }
                }

	            txdMetaInfoBlock.writeUInt16(numTextures);
	            txdMetaInfoBlock.writeUInt16(recommendedPlatform);
            }
        }
        catch( ... )
        {
            txdMetaInfoBlock.LeaveContext();

            throw;
        }

        txdMetaInfoBlock.LeaveContext();
    }

    // Serialize all textures of this TXD.
    // This is done by appending the textures after the meta block.
    LIST_FOREACH_BEGIN( TextureBase, txdObj->textures.root, texDictNode )

        TextureBase *texture = item;

        // Put it into a sub block.
        BlockProvider texNativeBlock( &outputProvider );

        engineInterface->SerializeBlock( texture, texNativeBlock );

    LIST_FOREACH_END

    // Write extensions.
    engineInterface->SerializeExtensions( txdObj, outputProvider );
}

}
