#include "StdInc.h"

#include "txdread.gc.hxx"

#include "pluginutil.hxx"

namespace rw
{

void gamecubeNativeTextureTypeProvider::SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const
{

}

void gamecubeNativeTextureTypeProvider::GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut )
{

}

void gamecubeNativeTextureTypeProvider::SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, texNativeTypeProvider::acquireFeedback_t& acquireFeedback )
{

}

void gamecubeNativeTextureTypeProvider::UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate )
{

}

static PluginDependantStructRegister <gamecubeNativeTextureTypeProvider, RwInterfaceFactory_t> gcNativeTypeRegister;

void registerGCNativePlugin( void )
{
    gcNativeTypeRegister.RegisterPlugin( engineFactory );
}

};