#include "mainwindow.h"

#include <d3d9.h>
#include <cwchar>
#include <locale>

#include "texformathelper.hxx"

#ifndef _DEBUG
#if defined(_M_AMD64)
#define MAGF_FORMAT_DIR     L"formats_x64"
#else
#define MAGF_FORMAT_DIR     L"formats"
#endif
#else
#if defined(_M_AMD64)
#define MAGF_FORMAT_DIR     L"formats_d_x64"
#else
#define MAGF_FORMAT_DIR     L"formats_d"
#endif
#endif

typedef void (__cdecl* LPFNSETINTERFACE)( const MagicFormatPluginInterface *intf );
typedef MagicFormat* (__cdecl* LPFNDLLFUNC1)(unsigned int&);

struct MagicFormat_Ver1handler : public rw::d3dpublic::nativeTextureFormatHandler
{
    inline MagicFormat_Ver1handler( MagicFormat *handler )
    {
        this->libHandler = handler;
    }

    const char*     GetFormatName( void ) const override
    {
        return libHandler->GetFormatName();
    }

    size_t GetFormatTextureDataSize( unsigned int width, unsigned int height ) const override
    {
        return libHandler->GetFormatTextureDataSize( width, height );
    }

    void GetTextureRWFormat( rw::eRasterFormat& rasterFormatOut, unsigned int& depthOut, rw::eColorOrdering& colorOrderOut ) const
    {
        MAGIC_RASTER_FORMAT mrasterformat;
        unsigned int mdepth;
        MAGIC_COLOR_ORDERING mcolororder;

        libHandler->GetTextureRWFormat( mrasterformat, mdepth, mcolororder );

        MagicMapToInternalRasterFormat( mrasterformat, rasterFormatOut );

        depthOut = mdepth;

        MagicMapToInternalColorOrdering( mcolororder, colorOrderOut );
    }

    virtual void ConvertToRW(
        const void *texData, unsigned int texMipWidth, unsigned int texMipHeight, size_t dstRowStride, size_t texDataSize,
        void *texOut
    ) const override
    {
        libHandler->ConvertToRW(
            texData, texMipWidth, texMipHeight, dstRowStride, texDataSize,
            texOut
        );
    }

    virtual void ConvertFromRW(
        unsigned int texMipWidth, unsigned int texMipHeight, size_t srcRowStride,
        const void *texelSource, rw::eRasterFormat rasterFormat, unsigned int depth, rw::eColorOrdering colorOrder, rw::ePaletteType paletteType, const void *paletteData, unsigned int paletteSize,
        void *texOut
    ) const override
    {
        MAGIC_RASTER_FORMAT mrasterformat;
        MAGIC_COLOR_ORDERING mcolororder;
        MAGIC_PALETTE_TYPE mpalettetype;

        MagicMapToVirtualRasterFormat( rasterFormat, mrasterformat );
        MagicMapToVirtualColorOrdering( colorOrder, mcolororder );
        MagicMapToVirtualPaletteType( paletteType, mpalettetype );

        libHandler->ConvertFromRW(
            texMipWidth, texMipHeight, srcRowStride,
            texelSource, mrasterformat, depth, mcolororder, mpalettetype, paletteData, paletteSize,
            texOut
        );
    }

private:
    MagicFormat *libHandler;
};

static MagicFormatPluginExports _funcExportIntf;

void MainWindow::initializeNativeFormats( void )
{
    // Register a basic format that we want to test things on.
    // We only can do that if the engine has the Direct3D9 native texture loaded.
    rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( this->rwEngine, "Direct3D9" );

    if ( driverIntf )
    {
		WIN32_FIND_DATA FindFileData;
		memset(&FindFileData, 0, sizeof(WIN32_FIND_DATA));
        std::wstring magfpath = this->m_appPath.toStdWString();
        std::wstring path = magfpath + L'\\' + MAGF_FORMAT_DIR + L'\\' + L"*.magf";
		HANDLE hFind = FindFirstFileW(path.c_str(), &FindFileData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
                    std::wstring wPluginName = FindFileData.cFileName;
                    std::wstring filename = magfpath + L'\\' + MAGF_FORMAT_DIR + L'\\' + FindFileData.cFileName;
					HMODULE hDLL = LoadLibraryW(filename.c_str());
					if (hDLL != NULL)
					{
                        bool success = false;

						LPFNDLLFUNC1 func = (LPFNDLLFUNC1)GetProcAddress(hDLL, "GetFormatInstance");
                        LPFNSETINTERFACE intfFunc = (LPFNSETINTERFACE)GetProcAddress( hDLL, "SetInterface" );
						if (func && intfFunc)
						{
                            unsigned int magf_version = 0;

							MagicFormat *handler = func( magf_version );

                            // We must have correct ABI version to load.
                            if ( magf_version == MagicFormatAPIVersion() )
                            {
                                // Give it our module interface.
                                intfFunc( &_funcExportIntf );

                                MagicFormat_Ver1handler *vhandler = new MagicFormat_Ver1handler( handler );

							    bool hasRegistered = driverIntf->RegisterFormatHandler(handler->GetD3DFormat(), vhandler);

                                if ( hasRegistered )
                                {
                                    magf_extension reg_entry;
                                    reg_entry.d3dformat = handler->GetD3DFormat();
                                    reg_entry.loadedLibrary = hDLL;
                                    reg_entry.handler = vhandler;

                                    this->magf_formats.push_back( reg_entry );

                                    success = true;
                                    
                                    QString message =
                                        QString( "Loaded plugin " ) + QString::fromStdWString( wPluginName ) +
                                        QString( " (" ) + handler->GetFormatName() + QString( ")" );

							        this->txdLog->addLogMessage(message, LOGMSG_INFO);
                                }
                                else
                                {
                                    delete vhandler;
                                }
                            }
                            else
                            {
                                QString message =
                                    QString( "Texture format plugin (" ) + QString::fromStdWString( wPluginName ) + QString( ") is incorrect version" );

							    this->txdLog->showError(message);
                            }
						}
						else
						{
                            QString message =
                                QString( "Texture format plugin (" ) + QString::fromStdWString( wPluginName ) + QString( ") is corrupted" );

							this->txdLog->showError(message);
						}

                        if ( success == false )
                        {
                            FreeLibrary( hDLL );
                        }
					}
					else
					{
                        DWORD lastError = GetLastError();

                        QString message =
                            QString( "Failed to load texture format plugin (" ) + QString::fromStdWString( wPluginName ) + QString( ", " ) + QString::fromStdString( std::to_string( lastError ) ) + QString( ")" );

						this->txdLog->showError(message);
					}
				}
			} while (FindNextFile(hFind, &FindFileData));
			FindClose(hFind);
		}
    }
}

void MainWindow::shutdownNativeFormats( void )
{
    rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( this->rwEngine, "Direct3D9" );

    if ( driverIntf )
    {
        // Unload all formats.
        for ( magf_formats_t::const_iterator iter = this->magf_formats.begin(); iter != this->magf_formats.end(); iter++ )
        {
            const magf_extension& ext = *iter;

            // Unregister the plugin from the engine.
            driverIntf->UnregisterFormatHandler( ext.d3dformat );

            // Delete the virtual interface.
            {
                MagicFormat_Ver1handler *vhandler = (MagicFormat_Ver1handler*)ext.handler;

                delete vhandler;
            }

            // Unload the library.
            FreeLibrary( ext.loadedLibrary );
        }

        // Clear the list of resident formats.
        this->magf_formats.clear();
    }
}