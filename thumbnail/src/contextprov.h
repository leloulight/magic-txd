#pragma once

#include <map>
#include <functional>

struct RenderWareContextHandlerProvider : public IShellExtInit, public IContextMenu
{
    // IUnknown
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppOut ) override;
    IFACEMETHODIMP_(ULONG) AddRef( void ) override;
    IFACEMETHODIMP_(ULONG) Release( void ) override;

    // IShellExtInit
    IFACEMETHODIMP Initialize(
        LPCITEMIDLIST pidlFolder, LPDATAOBJECT
        pDataObj, HKEY hKeyProgID
    ) override;

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(
        HMENU hMenu, UINT indexMenu,
        UINT idCmdFirst, UINT idCmdLast, UINT uFlags
    ) override;
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    IFACEMETHODIMP GetCommandString(
        UINT_PTR idCommand, UINT uFlags,
        UINT *pwReserved, LPSTR pszName, UINT cchMax
    ) override;

    RenderWareContextHandlerProvider( void );

protected:
    ~RenderWareContextHandlerProvider( void );

    bool isInitialized;
    std::wstring fileName;

private:
    std::atomic <unsigned long> refCount;

    typedef std::map <UINT, std::function <bool ( void )>> menuCmdMap_t;

    menuCmdMap_t cmdMap;
};