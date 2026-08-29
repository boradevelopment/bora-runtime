// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "waylandEvents.h"


void surface_handle_enter(void* data, struct wl_surface* surface, struct wl_output* output)
{
    auto* window = static_cast<bnWindow*>(data);
    if (!window) return;

}

void output_geometry(void* data, struct wl_output* wl_output, int32_t x, int32_t y, int32_t physical_width,
    int32_t physical_height, int32_t subpixel, const char* make, const char* model, int32_t transform)
{
    auto* mon = static_cast<MonitorInfo*>(data);
    mon->x = x;
    mon->y = y;
    mon->physicalWidthMm = physical_width;
    mon->physicalHeightMm = physical_height;
    mon->subpixel = subpixel;
    mon->make = make ? make : "";
    mon->model = model ? model : "";
    mon->transform = transform;
}

void output_mode(void* data, struct wl_output* wl_output, uint32_t flags, int32_t width, int32_t height,
                 int32_t refresh)
{
    auto* mon = static_cast<MonitorInfo*>(data);

    // WL_OUTPUT_MODE_CURRENT indicates the active display mode
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        mon->widthPixels = width;
        mon->heightPixels = height;
        mon->refreshRateMHz = refresh;
    }
}

void output_done(void* data, struct wl_output* wl_output)
{
    auto* mon = static_cast<MonitorInfo*>(data);
    auto logger = LogManager::instance().getLogger("bora.nWindow.output");
    LOG_INFO_VERBOSE(logger, 2) << "[Output] Monitor updated: " << mon->make << " " << mon->model
        << " (" << mon->widthPixels << "x" << mon->heightPixels << " @ "
        << (mon->refreshRateMHz / 1000.0f) << "Hz, Scale: " << mon->scale << "x)\n";
}

void output_scale(void* data, struct wl_output* wl_output, int32_t factor)
{
    auto* mon = static_cast<MonitorInfo*>(data);
    mon->scale = factor;
}

void xdg_toplevel_handle_configure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height,
                                   wl_array* states)
{
    if (width > 0 && height > 0) {
        auto* window = static_cast<bnWindow*>(data);
        // WindowEvent event;

        // bool active = false;
        // if (states && states->size > 0) {
        //     auto* statePtr = static_cast<const uint32_t*>(states->data);
        //     size_t count = states->size / sizeof(uint32_t);
        //
        //     for (size_t i = 0; i < count; ++i) {
        //         if (statePtr[i] == XDG_TOPLEVEL_STATE_ACTIVATED) {
        //             active = true;
        //             break;
        //         }
        //     }
        // }


        // event.type = WindowEventType::Activate;
        // window->windowCallback(&event);
        //todo - forward!

        if (width != window->getWindowWidth() || height != window->getWindowHeight()) {
            WindowEvent event{};
            event.type = WindowEventType::Resize;
            event.width = width;
            event.height = height;
            window->windowCallback(&event);
        }
    }
}

void xdg_toplevel_handle_close(void* data, struct xdg_toplevel* xdg_toplevel)
{
    auto* window = static_cast<bnWindow*>(data);
    if (!window) return;

    WindowEvent event{};
    event.type = WindowEventType::Destroy;
    window->windowCallback(&event);
}

void pointer_handle_enter(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface,
    wl_fixed_t sx, wl_fixed_t sy)
{
    auto* window = static_cast<bnWindow*>(data);
    if (!window) return;
    
    window->handle->wayland.lastPointerEnterSerial = serial;
    WindowEvent event{};
    event.type = WindowEventType::SetCursor;
    event.x = wl_fixed_to_int(sx);
    event.y = wl_fixed_to_int(sy);
    window->handle->wayland.isCursorInside = true;
    window->handle->wayland.cachedCursorX = event.x;
    window->handle->wayland.cachedCursorY = event.y;

    window->handle->wayland.serial = serial;
    window->windowCallback(&event);
}

void pointer_handle_leave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface)
{
    auto* window = static_cast<bnWindow*>(data);

    WindowEvent event{};
    window->handle->wayland.isCursorInside = false;
    window->handle->wayland.serial = serial;
    event.type = WindowEventType::NonClientMouseLeave;
    window->windowCallback(&event);
}

void pointer_handle_motion(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    auto* window = static_cast<bnWindow*>(data);
    if (!window) return;

    WindowEvent hitEvent{};
    hitEvent.type = WindowEventType::NonClientHitTest;
    hitEvent.x = wl_fixed_to_int(sx);
    hitEvent.y = wl_fixed_to_int(sy);
    hitEvent.width = window->getWindowWidth();
    hitEvent.height = window->getWindowHeight();
    window->windowCallback(&hitEvent);

    WindowEvent moveEvent{};
    moveEvent.type = (hitEvent.customResult != static_cast<i64>(HitTestResult::Client))
                         ? WindowEventType::NonClientMouseMove
                         : WindowEventType::MouseMove;

    moveEvent.x = wl_fixed_to_int(sx);
    moveEvent.y = wl_fixed_to_int(sy);
    window->handle->wayland.cachedCursorX =  moveEvent.x;
    window->handle->wayland.cachedCursorY = moveEvent.y;

    moveEvent.wordParameter = static_cast<u64>(hitEvent.customResult); // Pass hit region
    window->windowCallback(&moveEvent);
}

void pointer_handle_button(void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button,
    uint32_t state)
{
    auto* window = static_cast<bnWindow*>(data);

    // Execute Hit-Test to classify whether click belongs to Client or Non-Client Frame
    WindowEvent hitEvent{};
    hitEvent.type = WindowEventType::NonClientHitTest;
    window->windowCallback(&hitEvent);

    bool isPressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    WindowEvent event{};
    if (isPressed) {
        event.type = (hitEvent.customResult != static_cast<i64>(HitTestResult::Client))
                         ? WindowEventType::NonClientMouseButtonDown
                         : WindowEventType::MouseButtonDown;
    } else {
        event.type = (hitEvent.customResult != static_cast<i64>(HitTestResult::Client))
                         ? WindowEventType::NonClientMouseButtonUp
                         : WindowEventType::MouseButtonUp;
    }

    // Convert Linux Input event code to engine index (BTN_LEFT=0x110, BTN_RIGHT=0x111, BTN_MIDDLE=0x112)
    event.button = (button == BTN_LEFT) ? 0 : (button == BTN_RIGHT ? 1 : 2);
    event.wordParameter = static_cast<u64>(hitEvent.customResult);
    event.longParameter = serial;
    window->handle->wayland.serial = serial;
    window->windowCallback(&event);
}

void pointer_handle_axis(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    auto* window = static_cast<bnWindow*>(data);

    // Axis 0 = Vertical Scroll, Axis 1 = Horizontal Scroll
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        WindowEvent event{};
        event.type = WindowEventType::MouseWheel;
        // Wayland passes scroll offset as 24.8 fixed point. Invert sign to match Win32 wheel delta convention
        event.delta = -wl_fixed_to_int(value);
        window->windowCallback(&event);
    }
}

int TranslateEvdevToEngineKey(uint32_t evdevKey) {
    switch (evdevKey) {
        // Letters (Lowercase ASCII)
        case KEY_A: return 'a';
        case KEY_B: return 'b';
        case KEY_C: return 'c';
        case KEY_D: return 'd';
        case KEY_E: return 'e';
        case KEY_F: return 'f';
        case KEY_G: return 'g';
        case KEY_H: return 'h';
        case KEY_I: return 'i';
        case KEY_J: return 'j';
        case KEY_K: return 'k';
        case KEY_L: return 'l';
        case KEY_M: return 'm';
        case KEY_N: return 'n';
        case KEY_O: return 'o';
        case KEY_P: return 'p';
        case KEY_Q: return 'q';
        case KEY_R: return 'r';
        case KEY_S: return 's';
        case KEY_T: return 't';
        case KEY_U: return 'u';
        case KEY_V: return 'v';
        case KEY_W: return 'w';
        case KEY_X: return 'x';
        case KEY_Y: return 'y';
        case KEY_Z: return 'z';

        // Numbers
        case KEY_0: return '0';
        case KEY_1: return '1';
        case KEY_2: return '2';
        case KEY_3: return '3';
        case KEY_4: return '4';
        case KEY_5: return '5';
        case KEY_6: return '6';
        case KEY_7: return '7';
        case KEY_8: return '8';
        case KEY_9: return '9';

        // Common Whitespace & Control (Standard ASCII Control Codes)
        case KEY_SPACE:     return ' ';
        case KEY_ENTER:     return '\n';
        case KEY_TAB:       return '\t';
        case KEY_BACKSPACE: return 8;    // ASCII Backspace (\b)
        case KEY_ESC:       return 27;   // ASCII Escape

        // Punctuation
        case KEY_MINUS:        return '-';
        case KEY_EQUAL:        return '=';
        case KEY_LEFTBRACE:    return '[';
        case KEY_RIGHTBRACE:   return ']';
        case KEY_SEMICOLON:    return ';';
        case KEY_APOSTROPHE:   return '\'';
        case KEY_GRAVE:        return '`';
        case KEY_BACKSLASH:    return '\\';
        case KEY_COMMA:        return ',';
        case KEY_DOT:          return '.';
        case KEY_SLASH:        return '/';

        default: return 0; // Unmapped key
    }
}

void keyboard_handle_key(void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key,
    uint32_t state)
{
    auto* window = static_cast<bnWindow*>(data);

    WindowEvent event{};
    event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                     ? WindowEventType::KeyDown
                     : WindowEventType::KeyUp;

    event.scancode = static_cast<int>(key);
    // Transmute Linux evdev keycode to internal engine keycode index
    event.key = TranslateEvdevToEngineKey(key);

    window->windowCallback(&event);
}

void seat_handle_capabilities(void* data, struct wl_seat* seat, uint32_t caps)
{
    //?
    auto* window = static_cast<bnWindow*>(data);

    // 1. Pointer (Mouse) Registration
    if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
        window->handle->wayland.wlPointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(wl_seat_get_pointer(seat), &g_pointer_listener, data);
    }

    // 2. Keyboard Registration
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        wl_keyboard_add_listener(wl_seat_get_keyboard(seat), &g_keyboard_listener, data);
    }
}

void registry_handler(void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version)
{
    auto* state = static_cast<SysHandle>(data);

    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wayland.shm = static_cast<wl_shm*>(
            wl_registry_bind(registry, id, &wl_shm_interface, 1)
        );
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wayland.compositor = static_cast<struct wl_compositor*>(
            wl_registry_bind(registry, id, &wl_compositor_interface, 1));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wayland.xdg_wm_base = static_cast<struct xdg_wm_base*>(
            wl_registry_bind(registry, id, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(state->wayland.xdg_wm_base, &xdg_wm_base_listener, state);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->wayland.wl_seat = static_cast<wl_seat*>(
            wl_registry_bind(registry, id, &wl_seat_interface, 1)
        );

        wl_seat_add_listener(state->wayland.wl_seat, &g_seat_listener, data);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // Bind up to version 3 or 4 to support scale and done events
        uint32_t bindVersion = std::min(version, 3u);
        auto* output = static_cast<wl_output*>(
            wl_registry_bind(registry, id, &wl_output_interface, bindVersion)
        );

        auto monitor = std::make_unique<MonitorInfo>();
        monitor->globalId = id;
        monitor->output = output;

        wl_output_add_listener(output, &g_OutputListener, monitor.get());
        state->wayland.monitorOutputs.push_back(std::move(monitor));
    }
}

void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t id)
{
    auto* state = static_cast<SysHandle>(data);
    // Handle monitor disconnection
    std::erase_if(state->wayland.monitorOutputs, [id](const auto& mon) {
        if (mon->globalId == id) {
            wl_output_destroy(mon->output);
            return true;
        }
        return false;
    });
}
