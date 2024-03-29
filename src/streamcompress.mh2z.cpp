#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

#define MAGIC_NUM       'MH2Z'

// Meh. I made this just for fun. Manhunt 2 does not use RW TXD formats anyway.

struct mh2zCompressionEnv : public compressionManager
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterStreamCompressionManager( mainWnd, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterStreamCompressionManager( mainWnd, this );
    }

    struct mh2zHeader
    {
        endian::little_endian <std::uint32_t> magic_num;
        endian::little_endian <std::uint32_t> decomp_size;
    };

    bool IsStreamCompressed( CFile *input ) const override
    {
        mh2zHeader header;

        if ( !input->ReadStruct( header ) )
        {
            return false;
        }

        if ( header.magic_num != MAGIC_NUM )
        {
            return false;
        }

        // We trust that it is compressed.
        return true;
    }

    struct mh2zCompressionProvider : public compressionProvider
    {
        bool Compress( CFile *input, CFile *output ) override
        {
            // Write the MH2Z header.
            mh2zHeader header;
            header.magic_num = MAGIC_NUM;
            header.decomp_size = (size_t)( input->GetSizeNative() - input->TellNative() );

            output->WriteStruct( header );

            // Now the compression data.
            fileSystem->CompressZLIBStream( input, output, true );
            return true;
        }

        bool Decompress( CFile *input, CFile *output ) override
        {
            mh2zHeader header;

            if ( !input->ReadStruct( header ) )
            {
                return false;
            }

            if ( header.magic_num != MAGIC_NUM )
            {
                return false;
            }

            // Decompress the remainder into the output.
            size_t dataSize = (size_t)( input->GetSizeNative() - input->TellNative() );

            fileSystem->DecompressZLIBStream( input, output, dataSize, true );
            return true;
        }
    };

    compressionProvider* CreateProvider( void ) override
    {
        mh2zCompressionProvider *prov = new mh2zCompressionProvider();

        return prov;
    }

    void DestroyProvider( compressionProvider *prov ) override
    {
        mh2zCompressionProvider *dprov = (mh2zCompressionProvider*)prov;

        delete dprov;
    }
};

static PluginDependantStructRegister <mh2zCompressionEnv, mainWindowFactory_t> mh2zComprRegister;

void InitializeMH2ZCompressionEnv( void )
{
    mh2zComprRegister.RegisterPlugin( mainWindowFactory );
}