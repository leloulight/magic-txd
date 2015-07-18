#include "mainwindow.h"

#include <d3d9.h>

typedef rw::d3dpublic::nativeTextureFormatHandler *(__cdecl* LPFNDLLFUNC1)();

void MainWindow::initializeNativeFormats( void )
{
    // Register a basic format that we want to test things on.
    // We only can do that if the engine has the Direct3D9 native texture loaded.
    rw::d3dpublic::d3dNativeTextureDriverInterface *driverIntf = (rw::d3dpublic::d3dNativeTextureDriverInterface*)rw::GetNativeTextureDriverInterface( this->rwEngine, "Direct3D9" );

    if ( driverIntf )
    {
		WIN32_FIND_DATA FindFileData;
		memset(&FindFileData, 0, sizeof(WIN32_FIND_DATA));
		HANDLE hFind = FindFirstFile(L"formats\\*.magf", &FindFileData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					wchar_t filename[MAX_PATH];
					wcscpy(filename, L"formats\\");
					wcscat(filename, FindFileData.cFileName);
					char message[512];
					char pluginName[MAX_PATH];
					wcstombs(pluginName, FindFileData.cFileName, MAX_PATH);
					HMODULE hDLL = LoadLibrary(filename);
					if (hDLL != NULL)
					{
						LPFNDLLFUNC1 func = (LPFNDLLFUNC1)GetProcAddress(hDLL, "GetFormatInstance");
						if (func)
						{
							rw::d3dpublic::nativeTextureFormatHandler *handler = func();
							driverIntf->RegisterFormatHandler(handler->GetD3DFormat(), handler);
							sprintf(message, "Loaded plugin %s (%s)", pluginName, handler->GetFormatName());
							this->txdLog->addLogMessage(message, LOGMSG_INFO);
						}
						else
						{
							sprintf(message, "Texture format plugin (%s) is corrupted", pluginName);
							this->txdLog->showError(message);
						}
					}
					else
					{
						sprintf(message, "Failed to load texture format plugin (%s)", pluginName);
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

}