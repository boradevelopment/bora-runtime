// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: linuxWindow.h
 * Purpose: Does the heavy lifting for native linux based window code
*/
#pragma once
// check if theres wayland/x11 support in build
#include <cstdlib>
#include "nWindow/bnWindowAbstracts.h"
#include "wayland/waylandLinuxWindow.h"
#include "x11/x11LinuxWindow.h"
#include "linuxAbstracts.h"

class linuxWindow
{
public:
 template <typename T>
    static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false) {
     auto logger = LogManager::instance().getLogger("bora.graphics.window");
        #ifndef BORA_LINUX_WINDOW_SUPPORT
        LOG_ERROR(logger) << "BORA was not built for window support!";
        return nullptr;
        #endif
        SysHandle handle{};
        if (waylandLinuxWindow::supported())
        {
            handle = waylandLinuxWindow::createWindow(object, configuration, isChild);
        }

        if (handle == nullptr)
        { // try x11
            if (!x11LinuxWindow::supported()) return nullptr;
            return x11LinuxWindow::createWindow(object, configuration, isChild);
        }


        handles.push_back(handle);
        return handle;
    }

    static void paintBasedOnConfig(const bnWindowConstructorStruct* configuration, SysHandle handle);

    // Gets
    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static long getScreenWidth(ConstSysHandle handle);
    static long getScreenHeight(ConstSysHandle handle);

    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);
    static void maximizeWindow(ConstSysHandle handle);
    static void showWindow(ConstSysHandle handle);
    static void hideWindow(SysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
    static void SetMoveResize(SysHandle handle, WindowEvent* event);
    static void setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info);

 // Conversions
    // static WindowPoint convertFromWin32Point(const POINT point);
    // static POINT convertToWin32Point(const WindowPoint point);
    // static WindowRect convertFromWin32Rect(const RECT point);
    // static RECT convertToWin32Rect(const WindowRect point);
    // static WindowSize convertFromWin32Size(const SIZE point);
    // static SIZE convertToWin32Size(const SIZE point);

    static bool isPointInRect(const WindowRect* rect, const WindowPoint point);

};

