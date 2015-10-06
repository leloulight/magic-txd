#include "StdInc.h"

#include <wrl/client.h>

#include <PathCch.h>
#pragma comment(lib, "Pathcch.lib")

using namespace Microsoft::WRL;

static LRESULT CALLBACK _menu_handler_proc( HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return DefWindowProcW( wnd, msg, wParam, lParam );
}

RenderWareContextHandlerProvider::RenderWareContextHandlerProvider( void ) : refCount( 1 )
{
    this->isInitialized = false;
    
    module_refCount++;
}

RenderWareContextHandlerProvider::~RenderWareContextHandlerProvider( void )
{
    module_refCount--;
}

IFACEMETHODIMP RenderWareContextHandlerProvider::QueryInterface( REFIID riid, void **ppOut )
{
    if ( !ppOut )
        return E_POINTER;

    if ( riid == __uuidof(IUnknown) )
    {
        this->refCount++;

        IUnknown *comObj = (IShellExtInit*)this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IShellExtInit) )
    {
        this->refCount++;

        IShellExtInit *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IContextMenu) )
    {
        this->refCount++;

        IContextMenu *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }

    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) RenderWareContextHandlerProvider::AddRef( void )
{
    return ++this->refCount;
}

IFACEMETHODIMP_(ULONG) RenderWareContextHandlerProvider::Release( void )
{
    unsigned long newRef = --this->refCount;

    if ( newRef == 0 )
    {
        delete this;
    }

    return newRef;
}

IFACEMETHODIMP RenderWareContextHandlerProvider::Initialize( LPCITEMIDLIST idList, LPDATAOBJECT dataObj, HKEY keyProgId )
{
    if ( this->isInitialized )
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    if ( dataObj == NULL )
        return E_INVALIDARG;

    HRESULT res = E_FAIL;

    try
    {
        FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stm;

        HRESULT getHandleSuccess = dataObj->GetData( &fe, &stm );

        if ( SUCCEEDED(getHandleSuccess) )
        {
            try
            {
                HDROP drop = (HDROP)GlobalLock( stm.hGlobal );

                if ( drop )
                {
                    try
                    {
                        UINT numFiles = DragQueryFileW( drop, 0xFFFFFFFF, NULL, 0 );

                        // For now we want to only support operation on one file.
                        if ( numFiles == 1 )
                        {
                            // Remember the file.
                            UINT fileNameCharCount = DragQueryFileW( drop, 0, NULL, 0 );

                            this->fileName.resize( (size_t)fileNameCharCount );

                            DragQueryFileW( drop, 0, (LPWSTR)this->fileName.data(), fileNameCharCount + 1 );

                            res = S_OK;
                        }
                    }
                    catch( ... )
                    {
                        GlobalUnlock( stm.hGlobal );

                        throw;
                    }

                    GlobalUnlock( stm.hGlobal );
                }
            }
            catch( ... )
            {
                ReleaseStgMedium( &stm );

                throw;
            }

            ReleaseStgMedium( &stm );
        }
    }
    catch( ... )
    {
        // Ignore errors.
    }

    this->isInitialized = true;

    return res;
}

template <typename resultFunc>
static void ShellGetTargetDirectory( const wchar_t *startPath, resultFunc cb )
{
    ComPtr <IFileOpenDialog> dlg;

    HRESULT getDialogSuc = CoCreateInstance( CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dlg ) );

    if ( SUCCEEDED(getDialogSuc) )
    {
        dlg->SetOptions( FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST );

        // Set some friendly attributes. :)
        dlg->SetTitle( L"Browse Folder..." );
        dlg->SetOkButtonLabel( L"Select" );
        dlg->SetFileNameLabel( L"Target:" );

        if ( startPath )
        {
            std::wstring dirPath( startPath );

            PathCchRemoveFileSpec( (wchar_t*)dirPath.data(), dirPath.size() );

            ComPtr <IShellItem> dirPathItem;

            HRESULT resParsePath = SHCreateItemFromParsingName( dirPath.c_str(), NULL, IID_PPV_ARGS( &dirPathItem ) );

            if ( SUCCEEDED(resParsePath) )
            {
                dlg->SetFolder( dirPathItem.Get() );
            }
        }

        // Alrite. Show ourselves.
        HRESULT resShow = dlg->Show( NULL );

        if ( SUCCEEDED(resShow) )
        {
            // Wait for the result.
            ComPtr <IShellItem> resultItem;

            HRESULT resFetchResult = dlg->GetResult( &resultItem );

            if ( SUCCEEDED(resFetchResult) )
            {
                // We call back the handler with the result.
                // Since we expect a fs result, we convert to a real path.
                LPWSTR strFSPath = NULL;

                HRESULT getFSPath = resultItem->GetDisplayName( SIGDN_FILESYSPATH, &strFSPath );

                if ( SUCCEEDED( getFSPath ) )
                {
                    cb( strFSPath );

                    CoTaskMemFree( strFSPath );
                }
            }
        }
    }
}

static rw::RwObject* RwObjectStreamRead( const wchar_t *fileName )
{
    rw::RwObject *resultObj = NULL;

    rw::streamConstructionFileParamW_t fileParam( fileName );

    rw::Stream *theStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &fileParam );

    if ( theStream )
    {
        try
        {
            resultObj = rwEngine->Deserialize( theStream );
        }
        catch( ... )
        {
            rwEngine->DeleteStream( theStream );

            throw;
        }

        rwEngine->DeleteStream( theStream );
    }

    return resultObj;
}

static void RwObjectStreamWrite( const wchar_t *dstPath, rw::RwObject *texObj )
{
    rw::streamConstructionFileParamW_t fileParam( dstPath );

    rw::Stream *rwStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_WRITEONLY, &fileParam );

    if ( rwStream )
    {
        try
        {
            // Just write the dict.
            rwEngine->Serialize( texObj, rwStream );
        }
        catch( ... )
        {
            rwEngine->DeleteStream( rwStream );

            throw;
        }

        rwEngine->DeleteStream( rwStream );
    }
}

template <typename callbackType>
static void RwObj_deepTraverse( rw::RwObject *rwObj, callbackType cb )
{
    if ( rw::TexDictionary *texDict = rw::ToTexDictionary( rwEngine, rwObj ) )
    {
        for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
        {
            rw::TextureBase *texHandle = iter.Resolve();

            cb( texHandle );
        }
    }

    cb( rwObj );
}

template <typename callbackType>
static void TexObj_forAllTextures( rw::RwObject *texObj, callbackType cb )
{
    RwObj_deepTraverse( texObj,
        [&]( rw::RwObject *rwObj )
    {
        if ( rw::TextureBase *texHandle = rw::ToTexture( rwEngine, rwObj ) )
        {
            cb( texHandle );
        }
    });
}

template <typename callbackType>
static void TexObj_forAllTextures_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            TexObj_forAllTextures( texObj, cb );
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        // Remember to clean up resources.
        rwEngine->DeleteRwObject( texObj );
    }
}

template <typename callbackType>
static void RwObj_deepTraverse_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            RwObj_deepTraverse( texObj, cb );
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        // Remember to clean up resources.
        rwEngine->DeleteRwObject( texObj );
    }
}

static void RwTextureMakeUnique( rw::TextureBase *texHandle )
{
    // Make a copy of the raster, if it is not unique.
    rw::Raster *texRaster = texHandle->GetRaster();

    if ( texRaster->refCount > 1 )
    {
        rw::Raster *newRaster = rw::CloneRaster( texRaster );

        try
        {
            texHandle->SetRaster( newRaster );
        }
        catch( ... )
        {
            rw::DeleteRaster( newRaster );

            throw;
        }

        // Release our reference.
        rw::DeleteRaster( newRaster );
    }
}

static rw::RwObject* RwObjectDeepClone( rw::RwObject *texObj )
{
    rw::RwObject *newObj = rwEngine->CloneRwObject( texObj );

    if ( newObj )
    {
        try
        {
            if ( rw::TexDictionary *newTexDict = rw::ToTexDictionary( rwEngine, newObj ) )
            {
                // Clone the textures with new raster objects.
                for ( rw::TexDictionary::texIter_t iter( newTexDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
                {
                    rw::TextureBase *texHandle = iter.Resolve();

                    RwTextureMakeUnique( texHandle );
                }
            }
            else if ( rw::TextureBase *newTexture = rw::ToTexture( rwEngine, newObj ) )
            {
                RwTextureMakeUnique( newTexture );
            }
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( newObj );
            throw;
        }
    }

    return newObj;
}

template <typename callbackType>
static void RwObj_transform_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            // Make a backup of the original in case something bad happened that causes data loss.
            rw::RwObject *safetyCopy = RwObjectDeepClone( texObj );

            try
            {
                // Process it.
                cb( texObj );

                // Save it.
                try
                {
                    RwObjectStreamWrite( txdFileName, texObj );
                }
                catch( ... )
                {
                    // Well, something fucked up our stuff.
                    // We should restore to the original, because the user does not want data loss!
                    // If we could not make a safety copy... too bad.
                    if ( safetyCopy )
                    {
                        RwObjectStreamWrite( txdFileName, safetyCopy );
                    }

                    // Transfer the error anyway.
                    throw;
                }
            }
            catch( ... )
            {
                if ( safetyCopy )
                {
                    rwEngine->DeleteRwObject( safetyCopy );
                }

                throw;
            }

            // If we have a safety copy, delete it.
            if ( safetyCopy )
            {
                rwEngine->DeleteRwObject( safetyCopy );
            }
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        rwEngine->DeleteRwObject( texObj );
    }
    else
    {
        throw rw::RwException( "invalid RW object" );
    }
}

template <typename callbackType>
static void TexObj_transform_ser( const wchar_t *txdFileName, callbackType cb )
{
    RwObj_transform_ser( txdFileName,
        [&]( rw::RwObject *rwObj )
    {
        TexObj_forAllTextures( rwObj, cb );
    });
}

template <typename exportHandler>
static void TexObj_exportAs( const wchar_t *txdFileName, const wchar_t *ext, const wchar_t *targetDir, exportHandler cb )
{
    TexObj_forAllTextures_ser( txdFileName,
        [=]( rw::TextureBase *texHandle )
    {
        std::wstring targetFilePath( targetDir );

        // Make a file path, with the proper extension.
        const std::string& ansiName = texHandle->GetName();

        std::wstring wideTexName( ansiName.begin(), ansiName.end() );   // NO UTF-8 SHIT.

        targetFilePath += L"\\";
        targetFilePath += wideTexName;
        targetFilePath += L".";
        targetFilePath += ext;

        // Do it!
        try
        {
            rw::streamConstructionFileParamW_t fileParam( targetFilePath.c_str() );

            rw::Stream *chunkStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_WRITEONLY, &fileParam );

            if ( chunkStream )
            {
                try
                {
                    // Alright. We are finally ready :)
                    cb( texHandle, chunkStream );
                }
                catch( ... )
                {
                    rwEngine->DeleteStream( chunkStream );

                    throw;
                }

                rwEngine->DeleteStream( chunkStream );
            }
        }
        catch( rw::RwException& )
        {
            // Whatever.
        }
    });
}

typedef std::list <std::pair <std::wstring, rw::KnownVersions::eGameVersion>> gameVerList_t;

static const gameVerList_t gameVerMap =
{
    { L"GTA 3", rw::KnownVersions::GTA3 },
    { L"GTA Vice City", rw::KnownVersions::VC_PC },
    { L"GTA San Andreas", rw::KnownVersions::SA },
    { L"Manhunt", rw::KnownVersions::MANHUNT }
};

IFACEMETHODIMP RenderWareContextHandlerProvider::QueryContextMenu(
    HMENU hMenu, UINT indexMenu,
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags
)
{
    // If uFlags include CMF_DEFAULTONLY then we should not do anything.
    if (CMF_DEFAULTONLY & uFlags)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    UINT menuIndex = indexMenu;

    // If there is no separator before us, we want to add one.
    bool shouldAddSeparator = false;
    {
        MENUITEMINFOW prevItemInfo;

        BOOL gotPrevItemInfo =
            GetMenuItemInfoW( hMenu, indexMenu - 1, TRUE, &prevItemInfo );

        if ( gotPrevItemInfo )
        {
            if ( ( prevItemInfo.fMask & MIIM_FTYPE ) == 0 || ( prevItemInfo.fType & MFT_SEPARATOR ) == 0 )
            {
                shouldAddSeparator = true;
            }
        }
    }

    if ( shouldAddSeparator )
    {
        MENUITEMINFOW sepItemInfo;
        sepItemInfo.fMask = MIIM_FTYPE;
        sepItemInfo.fType = MFT_SEPARATOR;

        BOOL sepAddSuccess =
            InsertMenuItemW( hMenu, menuIndex, TRUE, &sepItemInfo );

        if ( sepAddSuccess == TRUE )
        {
            menuIndex++;
        }
        // We do not care if it fails.
    }

    UINT curMenuID = 0;

    HMENU optionsMenu = CreateMenu();

    UINT optionsMenuIndex = 0;

    // Add some cool options.
    try
    {
        HMENU extractMenu = CreateMenu();

        try
        {
            // We add the default options that RenderWare supports.
            {
                const UINT extractTexChunksMenuID = curMenuID++;

                MENUITEMINFOW texChunkItemInfo;
                texChunkItemInfo.cbSize = sizeof( texChunkItemInfo );
                texChunkItemInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
                texChunkItemInfo.dwTypeData = L"Texture Chunks";
                texChunkItemInfo.fType = MFT_STRING;
                texChunkItemInfo.wID = idCmdFirst + extractTexChunksMenuID;
                texChunkItemInfo.fState = MFS_ENABLED;

                BOOL insertTexChunkExtract =
                    InsertMenuItemW( extractMenu, 0, TRUE, &texChunkItemInfo );

                if ( insertTexChunkExtract == FALSE )
                {
                    throw std::exception();
                }

                this->cmdMap[ extractTexChunksMenuID ] =
                    [=]( void )
                {
                    ShellGetTargetDirectory( this->fileName.c_str(),
                        [=]( const wchar_t *targetPath )
                    {
                        // We want to export the contents as raw texture chunks.
                        TexObj_exportAs( this->fileName.c_str(), L"rwtex", targetPath,
                            [=]( rw::TextureBase *texHandle, rw::Stream *outStream )
                        {
                            // We export the texture directly.
                            rwEngine->Serialize( texHandle, outStream );
                        });
                    });
                    return true;
                };
            }

            try
            {
                rw::registered_image_formats_t regFormats;
                rw::GetRegisteredImageFormats( rwEngine, regFormats );

                UINT dyn_id = 0;

                for ( const rw::registered_image_format& format : regFormats )
                {
                    UINT usedID = curMenuID++;

                    // Make a nice display string.
                    std::string displayString( format.defaultExt );
                    displayString += " (";
                    displayString += format.formatName;
                    displayString += ")";

                    MENUITEMINFOA formatItemInfo;
                    formatItemInfo.cbSize = sizeof( formatItemInfo );
                    formatItemInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
                    formatItemInfo.dwTypeData = (char*)displayString.c_str();
                    formatItemInfo.fType = MFT_STRING;
                    formatItemInfo.wID = idCmdFirst + usedID;
                    formatItemInfo.fState = MFS_ENABLED;

                    BOOL insertDynamicItem =
                        InsertMenuItemA( extractMenu, dyn_id + 1, TRUE, &formatItemInfo );

                    if ( insertDynamicItem == FALSE )
                    {
                        throw std::exception();
                    }

                    this->cmdMap[ usedID ] =
                        [=]( void )
                    {
                        // TODO: actually use a good extention instead of the default.
                        // We want to add support into RW to specify multiple image format extentions.

                        std::string extention( format.defaultExt );
                        std::wstring wideExtention( extention.begin(), extention.end() );

                        std::transform( wideExtention.begin(), wideExtention.end(), wideExtention.begin(), ::tolower );

                        ShellGetTargetDirectory( this->fileName.c_str(),
                            [&]( const wchar_t *targetDir )
                        {
                            TexObj_exportAs( this->fileName.c_str(), wideExtention.c_str(), targetDir,
                                [&]( rw::TextureBase *texHandle, rw::Stream *outStream )
                            {
                                // Only perform if we have a raster, which should be always anyway.
                                if ( rw::Raster *texRaster = texHandle->GetRaster() )
                                {
                                    texRaster->writeImage( outStream, extention.c_str() );
                                }
                            });
                        });
                        
                        return true;
                    };
                }
            }
            catch( rw::RwException& )
            {
                // If there was any RW exception, we continue anyway.
            }

            MENUITEMINFOW extractItemInfo;
            extractItemInfo.cbSize = sizeof( extractItemInfo );
            extractItemInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
            extractItemInfo.dwTypeData = L"Extract";
            extractItemInfo.fType = MFT_STRING;
            extractItemInfo.fState = MFS_ENABLED;
            extractItemInfo.hSubMenu = extractMenu;
        
            BOOL insertExtractItemSuccess = InsertMenuItemW( optionsMenu, optionsMenuIndex, TRUE, &extractItemInfo );

            if ( insertExtractItemSuccess == FALSE )
            {
                throw std::exception();
            }
            else
            {
                optionsMenuIndex++;
            }
        }
        catch( ... )
        {
            DestroyMenu( extractMenu );

            throw;
        }

        DestroyMenu( extractMenu );

        // We also might want to convert TXD files.
        HMENU convertMenu = CreateMenu();

        try
        {
            // Fill it with entries of platforms that we can target.
            rw::platformTypeNameList_t availPlatforms = rw::GetAvailableNativeTextureTypes( rwEngine );

            UINT cur_index = 0;

            for ( const std::string& name : availPlatforms )
            {
                const UINT usedID = curMenuID++;

                MENUITEMINFOA platformItemInfo;
                platformItemInfo.cbSize = sizeof( platformItemInfo );
                platformItemInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
                platformItemInfo.dwTypeData = (char*)name.c_str();
                platformItemInfo.fType = MFT_STRING;
                platformItemInfo.wID = idCmdFirst + usedID;
                platformItemInfo.fState = MFS_ENABLED;

                BOOL insertPlatformItem = InsertMenuItemA( convertMenu, cur_index, TRUE, &platformItemInfo );

                if ( insertPlatformItem == FALSE )
                {
                    throw std::exception();
                }
                else
                {
                    cur_index++;
                }

                this->cmdMap[ usedID ] =
                    [=]( void )
                {
                    try
                    {
                        TexObj_transform_ser( this->fileName.c_str(),
                            [=]( rw::TextureBase *texHandle )
                        {
                            // Convert to another platform.
                            // We only want to suceed with stuff if we actually wrote everything.
                            if ( rw::Raster *texRaster = texHandle->GetRaster() )
                            {
                                rw::ConvertRasterTo( texRaster, name.c_str() );
                            }
                        });
                    }
                    catch( rw::RwException& except )
                    {
                        // We should display a message box.
                        std::string errorMessage( "failed to convert to platform: " );

                        errorMessage += except.message;

                        MessageBoxA( NULL, errorMessage.c_str(), "Conversion Error", MB_OK );

                        // Throw further.
                        throw;
                    }

                    return true;
                };
            }

            MENUITEMINFOW convertMenuInfo;
            convertMenuInfo.cbSize = sizeof( convertMenuInfo );
            convertMenuInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
            convertMenuInfo.dwTypeData = L"Set Platform";
            convertMenuInfo.fType = MFT_STRING;
            convertMenuInfo.fState = MFS_ENABLED;
            convertMenuInfo.hSubMenu = convertMenu;

            BOOL insertSetPlatformItemSuccess = InsertMenuItemW( optionsMenu, 1, TRUE, &convertMenuInfo );

            if ( insertSetPlatformItemSuccess == FALSE )
            {
                throw std::exception();
            }
        }
        catch( ... )
        {
            DestroyMenu( convertMenu );

            throw;
        }

        DestroyMenu( convertMenu );

        // We also want the ability to change game version.
        HMENU versionMenu = CreateMenu();

        try
        {
            // Add game versions.
            UINT dyn_id = 0;

            for ( const std::pair <std::wstring, rw::KnownVersions::eGameVersion>& verPair : gameVerMap )
            {
                const UINT usedID = curMenuID++;

                MENUITEMINFOW verEntryInfo;
                verEntryInfo.cbSize = sizeof(verEntryInfo);
                verEntryInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
                verEntryInfo.dwTypeData = (wchar_t*)verPair.first.c_str();
                verEntryInfo.fType = MFT_STRING;
                verEntryInfo.wID = idCmdFirst + usedID;
                verEntryInfo.fState = MFS_ENABLED;

                BOOL succInsertVersionItem = InsertMenuItemW( versionMenu, dyn_id, TRUE, &verEntryInfo );

                if ( succInsertVersionItem == FALSE )
                {
                    throw std::exception();
                }
                else
                {
                    dyn_id++;
                }

                // Add a handler for it.
                this->cmdMap[ usedID ] =
                    [=]( void )
                {
                    try
                    {
                        // Execute us.
                        RwObj_transform_ser( this->fileName.c_str(),
                            [=]( rw::RwObject *rwObj )
                        {
                            RwObj_deepTraverse( rwObj,
                                [=]( rw::RwObject *rwObj )
                            {
                                rw::LibraryVersion newVer = rw::KnownVersions::getGameVersion( verPair.second );

                                rwObj->SetEngineVersion( newVer );
                            });
                        });
                    }
                    catch( rw::RwException& except )
                    {
                        // We want to inform about this aswell.
                        std::string errorMsg( "failed to set game version: " );

                        errorMsg += except.message;

                        MessageBoxA( NULL, errorMsg.c_str(), "Error", MB_OK );

                        // Pass on the error.
                        throw;
                    }
                    return true;
                };
            }

            MENUITEMINFOW versionMenuInfo;
            versionMenuInfo.cbSize = sizeof(versionMenuInfo);
            versionMenuInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU | MIIM_STATE;
            versionMenuInfo.dwTypeData = L"Set Version";
            versionMenuInfo.fType = MFT_STRING;
            versionMenuInfo.fState = MFS_ENABLED;
            versionMenuInfo.hSubMenu = versionMenu;

            BOOL insertVersionMenu = InsertMenuItemW( optionsMenu, 2, TRUE, &versionMenuInfo );

            if ( insertVersionMenu == FALSE )
            {
                throw std::exception();
            }
        }
        catch( ... )
        {
            DestroyMenu( versionMenu );

            throw;
        }

        DestroyMenu( versionMenu );

        // TODO: detect whether Magic.TXD has been installed into a location.

        const UINT openWithMagicTXD_id = curMenuID++;

        MENUITEMINFOW openWithMagicTXDInfo;
        openWithMagicTXDInfo.cbSize = sizeof( openWithMagicTXDInfo );
        openWithMagicTXDInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
        openWithMagicTXDInfo.dwTypeData = L"Open with Magic.TXD";
        openWithMagicTXDInfo.fType = MFT_STRING;
        openWithMagicTXDInfo.wID = idCmdFirst + openWithMagicTXD_id;
        openWithMagicTXDInfo.fState = MFS_ENABLED;

        BOOL insertMagicTXDItemSuccess = InsertMenuItemW( optionsMenu, 3, TRUE, &openWithMagicTXDInfo );

        if ( insertMagicTXDItemSuccess == FALSE )
        {
            throw std::exception();
        }

        this->cmdMap[ openWithMagicTXD_id ] =
            [=]( void )
        {
            //__debugbreak();
            return true;
        };

        MENUITEMINFOW menuInfo;
        menuInfo.cbSize = sizeof( menuInfo );
        menuInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
        menuInfo.dwTypeData = L"RenderWare Options";
        menuInfo.fType = MFT_STRING;
        menuInfo.fState = MFS_ENABLED;
        menuInfo.hSubMenu = optionsMenu;

        BOOL insertRWOptionsMenu =
            InsertMenuItemW( hMenu, menuIndex, TRUE, &menuInfo );

        if ( insertRWOptionsMenu == FALSE )
        {
            throw std::exception();
        }
        else
        {
            menuIndex++;
        }
    }
    catch( ... )
    {
        DestroyMenu( optionsMenu );

        return HRESULT_FROM_WIN32( GetLastError() );
    }

    DestroyMenu( optionsMenu );

    // Determine the highest used ID.
    UINT highestID = 0;

    if ( !this->cmdMap.empty() )
    {
        highestID = this->cmdMap.rbegin()->first;
    }

    return MAKE_HRESULT( SEVERITY_SUCCESS, 0, highestID + 1 );
}

IFACEMETHODIMP RenderWareContextHandlerProvider::InvokeCommand( LPCMINVOKECOMMANDINFO pici )
{
    // We check by context menu id.
    bool isANSIVerbMode = ( HIWORD( pici->lpVerb ) != 0 );

    HRESULT isHandled = E_FAIL;
    
    if ( !isANSIVerbMode )
    {
        WORD cmdID = LOWORD( pici->lpVerb );

        // Execute the item.
        menuCmdMap_t::const_iterator iter = this->cmdMap.find( cmdID );

        if ( iter != this->cmdMap.end() )
        {
            try
            {
                // Do it.
                bool successful = iter->second();

                isHandled = ( successful ? S_OK : S_FALSE );
            }
            catch( ... )
            {
                // If we encountered any exception, just fail the handler.
            }
        }
    }

    return isHandled;
}

IFACEMETHODIMP RenderWareContextHandlerProvider::GetCommandString(
    UINT_PTR idCommand, UINT uFlags,
    UINT *pwReserved, LPSTR pszName, UINT cchMax
)
{
    return E_FAIL;
}