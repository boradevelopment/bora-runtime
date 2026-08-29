// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "waylandLinuxWindow.h"

#ifdef BORA_HAS_WAYLAND
void waylandLinuxWindow::processWaylandEvents(struct wl_display* display)
{
    if (!display) return;
    while (wl_display_prepare_read(display) != 0) {
        wl_display_dispatch_pending(display);
    }
    wl_display_flush(display);
    wl_display_read_events(display);
    wl_display_dispatch_pending(display);
}

WindowRect waylandLinuxWindow::getWindowRect(ConstSysHandle handle)
{
    if (auto* windowObj = static_cast<bnWindow*>(handle->object)) {
        uint32_t width  = windowObj->getWindowWidth();
        uint32_t height = windowObj->getWindowHeight();

        return { 0, 0, width, height };
    }

    return {0, 0,0,0};
}

WindowPoint waylandLinuxWindow::getCursorPositionFromWindowSpace(ConstSysHandle handle)
{
    WindowPoint pt{};
    if (!handle) return pt;

    pt.x = handle->wayland.cachedCursorX;
    pt.y = handle->wayland.cachedCursorY;

    return pt;
}

WindowPoint waylandLinuxWindow::getCursorPositionFromScreenSpace(ConstSysHandle handle)
{
    // todo: unsupported
    return getCursorPositionFromScreenSpace(handle);
}

u8 waylandLinuxWindow::getDPIOfWindow(ConstSysHandle handle)
{

    if (!handle || handle->wayland.monitorOutputs.empty()) {
        return 96; // Fallback baseline DPI
    }

    int maxScale = 1;

    // 1. Iterate over all wl_output handles the window surface currently spans
    for (auto &monitorInfo : handle->wayland.monitorOutputs) {
        if (!monitorInfo) continue;

        if (monitorInfo->scale > maxScale) {
            maxScale = monitorInfo->scale;
        }
    }

    constexpr u8 BASELINE_DPI = 96;
    return static_cast<u8>(maxScale * BASELINE_DPI);
}

long waylandLinuxWindow::getScreenWidth(ConstSysHandle handle)
{
    if (!handle) return 0;

    return handle->wayland.screenOutputWidth;
}

long waylandLinuxWindow::getScreenHeight(ConstSysHandle handle)
{
    if (!handle) return 0;

    return handle->wayland.screenOutputHeight;
}

void waylandLinuxWindow::setCursor(SysHandle handle, const WindowCursor& cursor)
{
    if (!handle) return;

    auto* pointer = handle->wayland.wlPointer;
    auto* cursorSurface = handle->wayland.cursorSurface;
    uint32_t enterSerial = handle->wayland.lastPointerEnterSerial;

    if (!pointer || !cursorSurface || !cursor.nativeHandle.wayland.wlBuffer) return;
    
    wl_surface_attach(cursorSurface, cursor.nativeHandle.wayland.wlBuffer, 0, 0);
    wl_surface_damage(cursorSurface, 0, 0,
                      cursor.nativeHandle.wayland.wlCursor->images[0]->width,
                      cursor.nativeHandle.wayland.wlCursor->images[0]->height);

    wl_surface_commit(cursorSurface);
    wl_pointer_set_cursor(
        pointer,
        enterSerial,
        cursorSurface,
        cursor.nativeHandle.wayland.hotspotX,
        cursor.nativeHandle.wayland.hotspotY
    );

}

WindowCursor waylandLinuxWindow::createCursor(SysHandle handle, SystemCursorShape shape)
{
    WindowCursor cursor{};

    auto* shm = static_cast<wl_shm*>(handle->wayland.shm);

    // Load standard Wayland cursor theme (default size 24)
    cursor.nativeHandle.wayland.wlTheme = wl_cursor_theme_load(nullptr, 24, shm);
    const char* cursorName = "left_ptr";

    switch (shape) {
    case SystemCursorShape::IBeam:    cursorName = "xterm"; break;
    case SystemCursorShape::Hand:     cursorName = "hand2"; break;
    case SystemCursorShape::ResizeNS: cursorName = "v_double_arrow"; break;
    case SystemCursorShape::ResizeEW: cursorName = "h_double_arrow"; break;
    default:                          cursorName = "left_ptr"; break;
    }

    cursor.nativeHandle.wayland.wlCursor = wl_cursor_theme_get_cursor(cursor.nativeHandle.wayland.wlTheme, cursorName);
    if (cursor.nativeHandle.wayland.wlCursor && cursor.nativeHandle.wayland.wlCursor->image_count > 0) {
        auto* image = cursor.nativeHandle.wayland.wlCursor->images[0];
        cursor.nativeHandle.wayland.wlBuffer = wl_cursor_image_get_buffer(image);
        cursor.nativeHandle.wayland.hotspotX = image->hotspot_x;
        cursor.nativeHandle.wayland.hotspotY = image->hotspot_y;
    }

    return cursor;
}

void waylandLinuxWindow::setTitle(ConstSysHandle handle, const wchar_t* c_str)
{
    if (!handle) return;

    auto* xdgToplevel = static_cast<struct xdg_toplevel*>(handle->wayland.xdg_toplevel);

    if (!xdgToplevel) return;

    // Set title on the xdg_toplevel role
    xdg_toplevel_set_title(xdgToplevel, wcharToUtf8(c_str).c_str());

    // Commit surface changes to apply the updated title state
    if (handle->wayland.surface) {
        wl_surface_commit(static_cast<struct wl_surface*>(handle->wayland.surface));
    }
}

void waylandLinuxWindow::maximizeWindow(ConstSysHandle handle)
{
    if (handle->wayland.xdg_toplevel) {
        xdg_toplevel_set_maximized(handle->wayland.xdg_toplevel);
    }
}

void waylandLinuxWindow::showWindow(ConstSysHandle handle)
{
    if (handle->wayland.surface && handle->wayland.display)
    {
        wl_surface_commit(handle->wayland.surface);
    }
}

void waylandLinuxWindow::hideWindow(SysHandle handle)
{
    if (handle->wayland.xdg_toplevel) {
        xdg_toplevel_destroy(handle->wayland.xdg_toplevel);
        handle->wayland.xdg_toplevel = nullptr;
    }
    if (handle->wayland.xdg_surface) {
        xdg_surface_destroy(handle->wayland.xdg_surface);
        handle->wayland.xdg_surface = nullptr;
    }

    if (handle->wayland.surface && handle->wayland.display) {
        wl_surface_attach(handle->wayland.surface, nullptr, 0, 0);
        wl_surface_commit(handle->wayland.surface);
        wl_display_flush(handle->wayland.display);
    }
}

void waylandLinuxWindow::setIcon(ConstSysHandle handle, const u8* data, size_t size)
{
    constexpr long imageSize = 256;
    std::string appId = "dev.bora.";
    appId.append(handle->id);

    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    fs::path baseDir;

    if (xdgDataHome && xdgDataHome[0] != '\0') {
        baseDir = xdgDataHome;
    } else {
        const char* home = std::getenv("HOME");
        if (!home) return;
        baseDir = fs::path(home) / ".local" / "share";
    }

    fs::path iconDir = baseDir / "icons" / "hicolor" / (std::to_string(imageSize) + "x" + std::to_string(imageSize)) / "apps";
    fs::path iconPath = iconDir / (appId + ".png");
    fs::path desktopDir = baseDir / "applications";
    fs::path desktopPath = desktopDir / (appId + ".desktop");

    try {
        if (!fs::exists(iconPath)) {
            fs::create_directories(iconDir);
            std::ofstream iconFile(iconPath, std::ios::binary);
            if (iconFile.is_open()) {
                iconFile.write(reinterpret_cast<const char*>(data), size);
            }
        }

        // 3. Write .desktop entry if it doesn't already exist
        if (!fs::exists(desktopPath)) {
            fs::create_directories(desktopDir);
            std::ofstream desktopFile(desktopPath);
            if (desktopFile.is_open()) {
                desktopFile << "[Desktop Entry]\n"
                            << "Type=Application\n"
                            << "Name=" << handle->id << "\n"
                            << "Icon=" << appId << "\n"
                            << "Terminal=false\n"
                            << "NoDisplay=true\n"; // Hides app launcher entry if desired while retaining taskbar icons
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to write Wayland desktop entry: " << e.what() << '\n';
    }
}

void waylandLinuxWindow::setMoveResize(SysHandle handle, WindowEvent* event)
{
    if (event->wordParameter == static_cast<i64>(HitTestResult::Caption)) {
        xdg_toplevel_move(handle->wayland.xdg_toplevel, handle->wayland.wl_seat, handle->wayland.serial);
        return;
    }

    uint32_t edges = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
    switch (event->wordParameter) {
    case HitTestResult::Top:         edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
    case HitTestResult::Bottom:      edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
    case HitTestResult::Left:        edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
    case HitTestResult::Right:       edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
    case HitTestResult::TopLeft:     edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
    case HitTestResult::TopRight:    edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
    case HitTestResult::BottomLeft:  edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; break;
    case HitTestResult::BottomRight: edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; break;
    default: return;
    }

    xdg_toplevel_resize(handle->wayland.xdg_toplevel, handle->wayland.wl_seat, handle->wayland.serial, edges);
}

void waylandLinuxWindow::setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info)
{
    if (handle->wayland.surface && handle->wayland.display)
    {
        if (!handle->wayland.xdg_toplevel) return;

        // 0 in Wayland protocol means "no limit"
        xdg_toplevel_set_min_size(handle->wayland.xdg_toplevel, info.minWidth, info.maxHeight);
        xdg_toplevel_set_max_size(handle->wayland.xdg_toplevel, info.maxWidth, info.maxHeight);
    }
}
#endif
