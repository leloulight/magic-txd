#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

struct lzoStreamCompressionManager : public compressionManager
{
    inline void Initialize( MainWindow *mainWnd )
    {
        this->mainWnd = mainWnd;

        RegisterStreamCompressionManager( mainWnd, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterStreamCompressionManager( mainWnd, this );
    }

    bool IsStreamCompressed( CFile *stream ) const override
    {
        return mainWnd->fileSystem->IsStreamLZOCompressed( stream );
    }

    struct fsysProviderWrap : public compressionProvider
    {
        inline fsysProviderWrap( CIMGArchiveCompressionHandler *handler )
        {
            this->prov = handler;
        }

        bool Compress( CFile *input, CFile *output ) override
        {
            return prov->Compress( input, output );
        }

        bool Decompress( CFile *input, CFile *output ) override
        {
            return prov->Decompress( input, output );
        }

        CIMGArchiveCompressionHandler *prov;
    };

    compressionProvider* CreateProvider( void ) override
    {
        CIMGArchiveCompressionHandler *lzo = mainWnd->fileSystem->CreateLZOCompressor();

        if ( lzo )
        {
            return new fsysProviderWrap( lzo );
        }

        return NULL;
    }

    void DestroyProvider( compressionProvider *prov ) override
    {
        fsysProviderWrap *wrap = (fsysProviderWrap*)prov;

        CIMGArchiveCompressionHandler *fsysHandler = wrap->prov;

        delete wrap;

        mainWnd->fileSystem->DestroyLZOCompressor( fsysHandler );
    }

    MainWindow *mainWnd;
};

static PluginDependantStructRegister <lzoStreamCompressionManager, mainWindowFactory_t> lzoStreamCompressionRegister;

void InitializeLZOStreamCompression( void )
{
    lzoStreamCompressionRegister.RegisterPlugin( mainWindowFactory );
}