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

static rw::TexDictionary* RwTexDictionaryStreamRead( const wchar_t *fileName )
{
    rw::TexDictionary *resultObj = NULL;

    rw::streamConstructionFileParamW_t fileParam( fileName );

    rw::Stream *theStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &fileParam );

    if ( theStream )
    {
        try
        {
            rw::RwObject *rwObj = rwEngine->Deserialize( theStream );

            if ( rwObj )
            {
                try
                {
                    resultObj = rw::ToTexDictionary( rwEngine, rwObj );
                }
                catch( ... )
                {
                    rwEngine->DeleteRwObject( rwObj );

                    throw;
                }

                if ( !resultObj )
                {
                    rwEngine->DeleteRwObject( rwObj );
                }
            }
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

static void RwTexDictionaryStreamWrite( const wchar_t *dstPath, rw::TexDictionary *texDict )
{
    rw::streamConstructionFileParamW_t fileParam( dstPath );

    rw::Stream *rwStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_WRITEONLY, &fileParam );

    if ( rwStream )
    {
        try
        {
            // Just write the dict.
            rwEngine->Serialize( texDict, rwStream );
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
static void TexDict_forAllTextures( rw::TexDictionary *texDict, callbackType cb )
{
    // Do mindless exporting.
    for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
    {
        rw::TextureBase *texHandle = iter.Resolve();

        cb( texHandle );
    }
}

template <typename callbackType>
static void TexDict_forAllTextures_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::TexDictionary *texDict = RwTexDictionaryStreamRead( txdFileName );

    if ( texDict )
    {
        try
        {
            TexDict_forAllTextures( texDict, cb );
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texDict );

            throw;
        }

        // Remember to clean up resources.
        rwEngine->DeleteRwObject( texDict );
    }
}

static rw::TexDictionary* RwTexDictionaryDeepClone( rw::TexDictionary *texDict )
{
    rw::TexDictionary *dict = rw::CreateTexDictionary( rwEngine );

    if ( dict )
    {
        try
        {
            // Clone the textures with new raster objects.
            for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
            {
                rw::TextureBase *texHandle = iter.Resolve();

                rw::TextureBase *cloned = (rw::TextureBase*)rwEngine->CloneRwObject( texHandle );

                if ( cloned )
                {
                    try
                    {
                        // Make a copy of the raster.
                        rw::Raster *texRaster = cloned->GetRaster();

                        if ( texRaster == texHandle->GetRaster() )
                        {
                            rw::Raster *newRaster = rw::CloneRaster( texRaster );

                            try
                            {
                                cloned->SetRaster( newRaster );
                            }
                            catch( ... )
                            {
                                rw::DeleteRaster( newRaster );

                                throw;
                            }

                            // Release our reference.
                            rw::DeleteRaster( newRaster );
                        }

                        cloned->AddToDictionary( dict );
                    }
                    catch( ... )
                    {
                        rwEngine->DeleteRwObject( cloned );

                        throw;
                    }
                }
                else
                {
                    throw rw::RwException( "failed to clone texture" );
                }
            }
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( dict );

            dict = NULL;

            throw;
        }
    }

    return dict;
}

template <typename callbackType>
static void TexDict_transform_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::TexDictionary *texDict = RwTexDictionaryStreamRead( txdFileName );

    if ( texDict )
    {
        // Make a backup of the original in case something bad happened that causes data loss.
        rw::TexDictionary *safetyCopy = RwTexDictionaryDeepClone( texDict );

        try
        {
            // Process it.
            TexDict_forAllTextures( texDict, cb );

            // Save it.
            try
            {
                RwTexDictionaryStreamWrite( txdFileName, texDict );
            }
            catch( ... )
            {
                // Well, something fucked up our stuff.
                // We should restore to the original, because the user does not want data loss!
                // If we could not make a safety copy... too bad.
                if ( safetyCopy )
                {
                    RwTexDictionaryStreamWrite( txdFileName, safetyCopy );
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

            rwEngine->DeleteRwObject( texDict );

            throw;
        }

        // If we have a safety copy, delete it.
        if ( safetyCopy )
        {
            rwEngine->DeleteRwObject( safetyCopy );
        }

        rwEngine->DeleteRwObject( texDict );
    }
    else
    {
        throw rw::RwException( "not a (valid) TXD" );
    }
}

template <typename exportHandler>
static void TexDict_exportAs( const wchar_t *txdFileName, const wchar_t *ext, const wchar_t *targetDir, exportHandler cb )
{
    TexDict_forAllTextures_ser( txdFileName,
        [=]( rw::TextureBase *texHandle )
    {
        std::wstring targetFilePath( targetDir );

        // We want to give the extension ".rwtex".
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
                        TexDict_exportAs( this->fileName.c_str(), L"rwtex", targetPath,
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
                            TexDict_exportAs( this->fileName.c_str(), wideExtention.c_str(), targetDir,
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
        
            BOOL insertExtractItemSuccess = InsertMenuItemW( optionsMenu, 0, TRUE, &extractItemInfo );

            if ( insertExtractItemSuccess == FALSE )
            {
                throw std::exception();
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
                        TexDict_transform_ser( this->fileName.c_str(),
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
        }
        catch( ... )
        {
            DestroyMenu( convertMenu );

            throw;
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

        DestroyMenu( convertMenu );

        // TODO: detect whether Magic.TXD has been installed into a location.

        const UINT openWithMagicTXD_id = curMenuID++;

        MENUITEMINFOW openWithMagicTXDInfo;
        openWithMagicTXDInfo.cbSize = sizeof( openWithMagicTXDInfo );
        openWithMagicTXDInfo.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
        openWithMagicTXDInfo.dwTypeData = L"Open with Magic.TXD";
        openWithMagicTXDInfo.fType = MFT_STRING;
        openWithMagicTXDInfo.wID = idCmdFirst + openWithMagicTXD_id;
        openWithMagicTXDInfo.fState = MFS_ENABLED;

        BOOL insertMagicTXDItemSuccess = InsertMenuItemW( optionsMenu, 2, TRUE, &openWithMagicTXDInfo );

        if ( insertMagicTXDItemSuccess == FALSE )
        {
            throw std::exception();
        }

        this->cmdMap[ openWithMagicTXD_id ] =
            [=]( void )
        {
            __debugbreak();
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