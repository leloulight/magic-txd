#include "StdInc.h"

#ifdef RWLIB_INCLUDE_NATIVETEX_D3D9

#include "txdread.d3d9.hxx"

namespace rw
{

bool d3d9NativeTextureTypeProvider::RegisterFormatHandler( DWORD formatNumber, d3dpublic::nativeTextureFormatHandler *handler )
{
    D3DFORMAT format = (D3DFORMAT)formatNumber;

    bool success = false;

    // There can be only one D3DFORMAT linked to any handler.
    // Also, a handler is expected to be only linked to one D3DFORMAT.
    {
        // Check whether this format is already handled.
        // If it is, then ignore shit.
        d3dpublic::nativeTextureFormatHandler *alreadyHandled = this->GetFormatHandler( format );

        if ( alreadyHandled == NULL )
        {
            // Go ahead. Register it.
            nativeFormatExtension ext;
            ext.theFormat = format;
            ext.handler = handler;

            this->formatExtensions.push_back( ext );

            // Yay!
            success = true;
        }
    }

    return success;
}

bool d3d9NativeTextureTypeProvider::UnregisterFormatHandler( DWORD formatNumber )
{
    D3DFORMAT format = (D3DFORMAT)formatNumber;

    bool success = false;

    // We go through the list of registered formats and remove the ugly one.
    {
        for ( nativeFormatExtensions_t::const_iterator iter = this->formatExtensions.cbegin(); iter != this->formatExtensions.cend(); iter++ )
        {
            const nativeFormatExtension& ext = *iter;

            if ( ext.theFormat == format )
            {
                // Remove this.
                this->formatExtensions.erase( iter );

                success = true;
                break;
            }
        }
    }

    return success;
}

};

#endif //RWLIB_INCLUDE_NATIVETEX_D3D9