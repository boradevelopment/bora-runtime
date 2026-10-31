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
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#if defined(Always)
#undef Always
#endif
#if defined(None)
#undef None
#endif
#include <cstring>
#include "nWindow/linuxAbstracts.h"
#include "nWindow/bnWindowAbstracts.h"
#include <X11/extensions/Xrandr.h>
#include "tools/StringUtils.h"
#ifdef BORA_HAS_VULKAN
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xlib.h>
#endif

class x11LinuxWindow
{
    static XContext getXContext() {
        static XContext s_context = XUniqueContext();
        return s_context;
    }
public:
    [[nodiscard]] static bool supported() noexcept
    {
#if BORA_HAS_X11
#ifdef BORA_HAS_VULKAN
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        // Helper lambda to check if a specific extension exists
        auto isSupported = [&](const char* name) {
            for (const auto& ext : availableExtensions) {
                if (strcmp(ext.extensionName, name) == 0) {
                    return true;
                }
            }
            return false;
        };

        return isSupported(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
        return true;
#else
        return false;
#endif
    }
    template <typename T>
       static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false)
    {
        std::string titleUtf8 = StringUtils::wstringToUtf8(configuration->title);
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
        // XMapWindow(display, window);
        // XFlush(display); - hidden by default
        auto* handle = new LinuxWindowHandle();
        handle->object = object;
        handle->type = DisplayServerType::X11;
        handle->x11.display = display;
        handle->x11.window = window;
        handle->x11.wmDeleteMessage = wmDelete;

        Atom hintsAtom = XInternAtom(display, "_MOTIF_WM_HINTS", True);
        if (hintsAtom != 0) {
            struct {
                unsigned long flags;
                unsigned long functions;
                unsigned long decorations;
                long input_mode;
                unsigned long status;
            } hints = {0};

            hints.flags = (1L << 0) | (1L << 1); // MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS
            hints.functions = (1L << 0) | (1L << 5); // MWM_FUNC_ALL or allow move/close (or MWM_FUNC_CLOSE)
            hints.decorations = 0; // No borders/titlebar

            XChangeProperty(display, window, hintsAtom, hintsAtom, 32, PropModeReplace, (unsigned char *)&hints, 5);
        }

        Atom windowType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        Atom normalType = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        XChangeProperty(display, window, windowType, XA_ATOM, 32, PropModeReplace, (unsigned char *)&normalType, 1);

        return handle;
    }


    static void SendMessage(SysHandle handle, unsigned int msg, long long wParam = 0, long long lParam = 0);
    static void ReceiveMessages(SysHandle handle);

    static long getScreenWidth(ConstSysHandle handle);
    static long getScreenHeight(ConstSysHandle handle);
    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static void processX11Events(Display* display);

    static bool isMaximized(ConstSysHandle handle);
    static bool isIconic(ConstSysHandle handle);

    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);
    static void maximizeWindow(ConstSysHandle handle);
    static void minimizeWindow(ConstSysHandle handle);
    static void showWindow(ConstSysHandle handle);
    static void hideWindow(ConstSysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
    static void setMoveResize(SysHandle handle, WindowEvent* event);
    static void setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info);
    static void resizeWindow(ConstSysHandle handle, long width, long height);
};

#endif
