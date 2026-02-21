// This header is apart of the BORA Source
// Check LICENSE.md for more information regarding the BORA license.

/* 
 * FileName: win32Window.h
 * Purpose: Does the heavy lifting for native windows code
 */
#pragma once
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <propvarutil.h>
#include <shobjidl_core.h>  // For IPropertyStore
#include <propsys.h>        // For property keys
#include <propkey.h>
#include "nWindow/bnWindowAbstracts.h"
#include <Windowsx.h>


inline void SetWindowAppUserModelID(HWND hwnd, PCWSTR appUserModelID) {
    // Get the property store of the window
    IPropertyStore* pPropStore = nullptr;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pPropStore));
    if (SUCCEEDED(hr)) {
        // Create a PROPVARIANT for the string value
        PROPVARIANT propvar;
        hr = InitPropVariantFromString(appUserModelID, &propvar);
        if (SUCCEEDED(hr)) {
            // Set the AppUserModelID property key
            hr = pPropStore->SetValue(PKEY_AppUserModel_ID, propvar);
            if (SUCCEEDED(hr)) {
                pPropStore->Commit();
            }
            PropVariantClear(&propvar);
        }
        pPropStore->Release();
    }
}

using namespace Gdiplus;
class win32Window {
public:
    template <typename T>
    static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false) {
        const wchar_t* window_class_name = configuration->id.c_str();
        WNDCLASSEXW window_class = { 0 };
        window_class.cbSize = sizeof(window_class);
        window_class.lpszClassName = window_class_name;
        window_class.lpfnWndProc = winCallStatic;
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        RegisterClassExW(&window_class);

        SysHandle handle = CreateWindowExW(
            WS_EX_APPWINDOW,
            window_class_name,
            configuration->title.c_str(),
            WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            configuration->width,
            configuration->height,
            isChild && handles.size() >= 1 ? handles[handles.size()-1] : 0,
             0,
            configuration->hInstance ? configuration->hInstance : 0,
            object
        );


        handles.push_back(handle);

        SetWindowAppUserModelID(handle, configuration->title.c_str());

        return handle;
    }

    static LRESULT winCallStatic(HWND handle, UINT message, WPARAM w_param, LPARAM l_param);
    static void paintBasedOnConfig(const bnWindowConstructorStruct* configuration, SysHandle handle);

    // Gets
    static WindowRect getWindowRect(const SysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(const SysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace();
    static WindowSize getThemePartSize(const SysHandle handle, const wchar_t* theme, int partId, int stateId);
    static u8 getDPIOfWindow(SysHandle handle);

    // Conversions
    static WindowPoint convertFromWin32Point(const POINT point);
    static POINT convertToWin32Point(const WindowPoint point);
    static WindowRect convertFromWin32Rect(const RECT point);
    static RECT convertToWin32Rect(const WindowRect point);
    static WindowSize convertFromWin32Size(const SIZE point);
    static SIZE convertToWin32Size(const SIZE point);

    static bool isPointInRect(const WindowRect* rect, const WindowPoint point);
};
#endif