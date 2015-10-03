#include "StdInc.h"
#include <atomic>

#include <shlobj.h>                 // For SHChangeNotify

// {A16ACA16-0F66-480B-9F5C-B42E29761A9B}
const CLSID CLSID_RenderWareThumbnailProvider =
{ 0xa16aca16, 0xf66, 0x480b, { 0x9f, 0x5c, 0xb4, 0x2e, 0x29, 0x76, 0x1a, 0x9b } };


HMODULE module_instance = NULL;
std::atomic <unsigned long> module_refCount = 0;

rw::Interface *rwEngine = NULL;

// Sub modules.
extern void InitializeRwWin32StreamEnv( void );

extern void ShutdownRwWin32StreamEnv( void );

__declspec( dllexport ) BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        module_instance = hModule;

        DisableThreadLibraryCalls( hModule );

        // Initialize the module-wide RenderWare environment.
        {
            rw::LibraryVersion libVer;
            libVer.rwLibMajor = 3;
            libVer.rwLibMinor = 7;
            libVer.rwRevMajor = 0;
            libVer.rwRevMinor = 0;

            rwEngine = rw::CreateEngine( std::move( libVer ) );
        }

        InitializeRwWin32StreamEnv();
        break;
    case DLL_PROCESS_DETACH:
        ShutdownRwWin32StreamEnv();

        // Destroy the RenderWare environment.
        rw::DeleteEngine( rwEngine );

        module_instance = NULL;
        break;
    }

    return TRUE;
}

// Implementation of the factory object.
struct thumbNailClassFactory : public IClassFactory
{
    // IUnknown
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv )
    {
        if ( ppv == NULL )
            return E_POINTER;

        if ( riid == __uuidof(IClassFactory) )
        {
            this->refCount++;

            IClassFactory *comObj = this;

            *ppv = comObj;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef( void )
    {
        return ++refCount;
    }

    IFACEMETHODIMP_(ULONG) Release( void )
    {
        unsigned long newRefCount = --refCount;

        if ( newRefCount == 0 )
        {
            // We can clean up after ourselves.
            delete this;
        }

        return newRefCount;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        if ( pUnkOuter != NULL )
        {
            return CLASS_E_NOAGGREGATION;
        }

        if ( riid == __uuidof(IThumbnailProvider) )
        {
            IThumbnailProvider *prov = new (std::nothrow) RenderWareThumbnailProvider();

            if ( !prov )
            {
                return E_OUTOFMEMORY;
            }

            *ppv = prov;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if ( fLock )
        {
            module_refCount++;
        }
        else
        {
            module_refCount--;
        }

        return S_OK;
    }

    thumbNailClassFactory( void )
    {
        module_refCount++;
    }

protected:
    ~thumbNailClassFactory( void )
    {
        module_refCount--;
    }

private:
    std::atomic <unsigned long> refCount;
};

// Stubs.
namespace rw
{
    LibraryVersion app_version( void )
    {
        return LibraryVersion();
    }

    int32 rwmain( rw::Interface *rwEngine )
    {
        return -1;
    }
};

STDAPI DllCanUnloadNow( void )
{
    return ( module_refCount == 0 ? S_OK : S_FALSE );
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void **ppOut )
{
    if ( IsEqualCLSID( rclsid, CLSID_RenderWareThumbnailProvider ) && riid == __uuidof(IClassFactory) )
    {
        thumbNailClassFactory *thumbFactory = new (std::nothrow) thumbNailClassFactory();

        if ( thumbFactory )
        {
            IClassFactory *comObj = thumbFactory;

            *ppOut = comObj;
            return S_OK;
        }

        return E_OUTOFMEMORY;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

//
//   FUNCTION: DllRegisterServer
//
//   PURPOSE: Register the COM server and the thumbnail handler.
// 
STDAPI DllRegisterServer(void)
{
    HRESULT hr;

    wchar_t szModule[MAX_PATH];

    if (GetModuleFileNameW(module_instance, szModule, ARRAYSIZE(szModule)) == 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Register the component.
    hr = RegisterInprocServer(szModule, CLSID_RenderWareThumbnailProvider, 
        L"rwthumb.RenderWareThumbnailProvider Class", 
        L"Free");

    if ( SUCCEEDED(hr) )
    {
        // Register the thumbnail handler. The thumbnail handler is associated
        // with the .recipe file class.
        hr = RegisterShellExtThumbnailHandler(L".txd", CLSID_RenderWareThumbnailProvider);

        if ( SUCCEEDED(hr) )
        {
            // This tells the shell to invalidate the thumbnail cache. It is 
            // important because any .recipe files viewed before registering 
            // this handler would otherwise show cached blank thumbnails.
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        }
    }

    return hr;
}


//
//   FUNCTION: DllUnregisterServer
//
//   PURPOSE: Unregister the COM server and the thumbnail handler.
// 
STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;

    wchar_t szModule[MAX_PATH];

    if (GetModuleFileNameW( module_instance, szModule, ARRAYSIZE(szModule) ) == 0)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Unregister the component.
    hr = UnregisterInprocServer( CLSID_RenderWareThumbnailProvider );

    if ( SUCCEEDED(hr) )
    {
        // Unregister the thumbnail handler.
        hr = UnregisterShellExtThumbnailHandler(L".txd");
    }

    return hr;
}