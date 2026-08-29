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
#include <wayland-cursor.h>
#include <wayland-client.h>
#endif

#ifdef BORA_HAS_WAYLAND
struct MonitorInfo {
    uint32_t globalId = 0;
    wl_output* output = nullptr;

    // Physical & Output Metadata
    const char* make = "";
    const char* model = "";
    int32_t x = 0;
    int32_t y = 0;
    int32_t physicalWidthMm = 0;
    int32_t physicalHeightMm = 0;
    int32_t subpixel = 0;
    int32_t transform = 0;

    // Current Mode Specs
    int32_t widthPixels = 0;
    int32_t heightPixels = 0;
    int32_t refreshRateMHz = 0; // e.g. 60000 = 60Hz

    // Scale
    int32_t scale = 1;
};
#endif

struct LinuxWindowHandle {
    LinuxWindowHandle();
    void* object = nullptr;
    const char* id{};
    DisplayServerType type = DisplayServerType::Unknown;
    union {
#ifdef BORA_HAS_WAYLAND
        struct {
            wl_display* display = nullptr;
            wl_surface* surface = nullptr;
            wl_registry* registry = nullptr;
            wl_compositor* compositor = nullptr;
            struct xdg_wm_base* xdg_wm_base = nullptr;
            struct xdg_surface* xdg_surface = nullptr;
            struct xdg_toplevel* xdg_toplevel = nullptr;
            wl_seat* wl_seat = nullptr;
            wl_shm* shm = nullptr;
            wl_pointer* wlPointer = nullptr;
            wl_surface* cursorSurface = nullptr;
            long screenOutputWidth, screenOutputHeight;
            long cachedCursorX, cachedCursorY;
            bool isCursorInside;
            u32 serial;
            u32 lastPointerEnterSerial;
            std::vector<std::unique_ptr<MonitorInfo>> monitorOutputs;
        } wayland;
#endif
#ifdef BORA_HAS_X11
        struct {
            Cursor x11Cursor = 0;
            Display* display;
            ::Window window;
            Atom wmDeleteMessage;
        } x11{};
#endif
    };
};

struct LinuxCursorHandle {
#ifdef BORA_HAS_X11
    struct
    {
        // X11 cursor resource
        Cursor x11Cursor = 0;
    } x11;
#endif

#ifdef BORA_HAS_WAYLAND
    struct
    {
        // Wayland cursor representation
        wl_cursor_theme* wlTheme = nullptr;
        wl_cursor* wlCursor = nullptr;
        wl_buffer* wlBuffer = nullptr;
        u32 hotspotX = 0;
        u32 hotspotY = 0;
    } wayland;
#endif
};

