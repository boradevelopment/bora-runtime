// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: linuxAbstracts.h
 * Purpose: ?
*/
#pragma once
#ifdef BORA_HAS_X11
#include <X11/Xlib.h>
#endif
#ifdef BORA_HAS_WAYLAND
#include <wayland-client.h>
#endif

struct LinuxWindowHandle {
    LinuxWindowHandle();
    void* object = nullptr;
    const char* id{};
    DisplayServerType type = DisplayServerType::Unknown;
    union {
#ifdef BORA_HAS_WAYLAND
        struct {
            struct wl_display* display = nullptr;
            struct wl_surface* surface = nullptr;
            struct wl_registry* registry = nullptr;
            struct wl_compositor* compositor = nullptr;
            struct xdg_wm_base* xdg_wm_base = nullptr;
            struct xdg_surface* xdg_surface = nullptr;
            struct xdg_toplevel* xdg_toplevel = nullptr;
            long screenOutputWidth, screenOutputHeight;
        } wayland;
#endif
#ifdef BORA_HAS_X11
        struct {
            Display* display;
            ::Window window;
            Atom wmDeleteMessage;
        } x11{};
#endif
    };
};
