// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: x11LinuxWindow.h
 * Purpose: Helper for the linuxWindow class for X11 stuff
*/
#pragma once
#ifdef BORA_HAS_X11
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include "nWindow/linuxAbstracts.h"
#include "Utilities.h"
#include "nWindow/bnWindowAbstracts.h"

class x11LinuxWindow
{
    static XContext getXContext() {
        static XContext s_context = XUniqueContext();
        return s_context;
    }
public:
    [[nodiscard]] static constexpr bool supported() noexcept
    {
#if BORA_HAS_X11
        return true;
#else
        return false;
#endif
    }
    template <typename T>
       static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false)
    {
        std::string titleUtf8 = wstringToUtf8(configuration->title);
        long width = configuration->width;
        long height = configuration->height;
        Display* display = XOpenDisplay(nullptr);
        if (!display) return nullptr;
        int screen = DefaultScreen(display);
        ::Window root = RootWindow(display, screen);
        ::Window window = XCreateSimpleWindow(
        display, root,
        0, 0, width, height, 1,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
         );

        XSaveContext(display, window, getXContext(), reinterpret_cast<XPointer>(object));
        XStoreName(display, window, titleUtf8.c_str());
        XClassHint classHint;
        classHint.res_name = const_cast<char*>(titleUtf8.c_str());
        classHint.res_class = const_cast<char*>(titleUtf8.c_str());
        XSetClassHint(display, window, &classHint);
        Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDelete, 1);
        long eventMask = ExposureMask | StructureNotifyMask | FocusChangeMask |
                         KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask | EnterWindowMask | LeaveWindowMask;
        XSelectInput(display, window, eventMask);

        XMapWindow(display, window);
        XFlush(display);
        auto* handle = new LinuxWindowHandle();
        handle->object = object;
        handle->type = DisplayServerType::X11;
        handle->x11.display = display;
        handle->x11.window = window;
        handle->x11.wmDeleteMessage = wmDelete;

        return handle;
    }

    static long getScreenWidth(ConstSysHandle handle);
    static long getScreenHeight(ConstSysHandle handle);
    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static void processX11Events(Display* display);

    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);
    static void maximizeWindow(ConstSysHandle handle);
    static void showWindow(ConstSysHandle handle);
    static void hideWindow(ConstSysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
};

#endif
