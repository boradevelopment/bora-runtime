// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: waylandLinuxWindow.h
 * Purpose: Helper for the linuxWindow class for Wayland stuff
*/
#pragma once
#ifdef BORA_HAS_WAYLAND
#include <cstring>
#include "Utilities.h"
#include "waylandEvents.h"
#include "nWindow/bnWindowAbstracts.h"
#include "xdg-shell-client-protocol.h"
#include "nWindow/linuxAbstracts.h"

class bnWindow;

class waylandLinuxWindow
{
public:
    [[nodiscard]] static constexpr bool supported() noexcept
    {
#if BORA_HAS_WAYLAND
        return true;
#else
        return false;
#endif
    }
    template <typename T>
   static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false)
    {
        std::string titleUtf8 = wstringToUtf8(configuration->title);
        std::string idUtf8 = wstringToUtf8(configuration->id);
        // todo: get the developer name from somewhere.
        std::string finalID = "dev.bora." + idUtf8;

        long width = configuration->width;
        long height = configuration->height;

        if (const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY"))
        {
            auto* handle = new LinuxWindowHandle();
            handle->wayland.display = wl_display_connect(waylandDisplay);
            handle->object = object;
            if (handle->wayland.display)
            {
                handle->wayland.registry = wl_display_get_registry(handle->wayland.display);
                wl_registry_add_listener(handle->wayland.registry, &registry_listener, handle);
                wl_display_roundtrip(handle->wayland.display);
                wl_display_roundtrip(handle->wayland.display);

                if (handle->wayland.compositor && handle->wayland.xdg_wm_base) {
                    handle->type = DisplayServerType::Wayland;
                    handle->wayland.surface = wl_compositor_create_surface(handle->wayland.compositor);
                    handle->wayland.cursorSurface = wl_compositor_create_surface(handle->wayland.compositor);
                    handle->wayland.xdg_surface = xdg_wm_base_get_xdg_surface(
                        handle->wayland.xdg_wm_base, handle->wayland.surface);
                    xdg_surface_add_listener(handle->wayland.xdg_surface, &g_xdg_surface_listener, object);
                    handle->wayland.xdg_toplevel = xdg_surface_get_toplevel(handle->wayland.xdg_surface);

                    xdg_toplevel_set_app_id(handle->wayland.xdg_toplevel, finalID.c_str());
                    xdg_toplevel_set_title(handle->wayland.xdg_toplevel, titleUtf8.c_str());
                    xdg_toplevel_add_listener(handle->wayland.xdg_toplevel, &g_XdgToplevelListener, object);

                    wl_surface_commit(handle->wayland.surface);
                    wl_display_roundtrip(handle->wayland.display);
                    xdg_surface_set_window_geometry(
                        handle->wayland.xdg_surface,
                        0, 0,
                        static_cast<int32_t>(width),
                        static_cast<int32_t>(height)
                    );

                    handles.push_back(handle);
                    return handle;
                }
            }
        }

        return nullptr;
    }

    static void processWaylandEvents(struct wl_display* display);
    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static long getScreenWidth(ConstSysHandle handle);
    static long getScreenHeight(ConstSysHandle handle);

    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);
    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
    static void maximizeWindow(ConstSysHandle handle);
    static void showWindow(ConstSysHandle handle);
    static void hideWindow(SysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
    static void setMoveResize(SysHandle handle, WindowEvent* event);
    void setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info);
};
#endif