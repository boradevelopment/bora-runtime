// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "x11LinuxWindow.h"
#include "nWindow/bnWindow.h"
#include "nWindow/windowUtilities.h"

#ifdef BORA_HAS_X11
void x11LinuxWindow::SendMessage(SysHandle handle, unsigned int msg, long long wParam, long long lParam)
{
    if (!handle) return;

    if (msg == WindowMessages::MSG_CLOSE) {
        Display* dpy = handle->x11.display;
        Window win = handle->x11.window;

        Atom wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
        Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

        XEvent event{};
        event.type = ClientMessage;
        event.xclient.window = win;
        event.xclient.message_type = wmProtocols;
        event.xclient.format = 32;
        event.xclient.data.l[0] = wmDelete;
        event.xclient.data.l[1] = CurrentTime;

        // Post to server and flush connection buffer
        XSendEvent(dpy, win, False, NoEventMask, &event);
        XFlush(dpy);
    }
}

void x11LinuxWindow::ReceiveMessages(SysHandle handle)
{
    processX11Events(handle->x11.display);
}

long x11LinuxWindow::getScreenWidth(ConstSysHandle handle)
{
    if (handle == nullptr) return 0;
    if (handle->x11.display) {
        int screen = DefaultScreen(handle->x11.display);
        return DisplayWidth(handle->x11.display, screen);
    } else {
        return 0;
    }
}

long x11LinuxWindow::getScreenHeight(ConstSysHandle handle)
{
    if (handle == nullptr) return 0;
    if (handle->x11.display) {
        int screen = DefaultScreen(handle->x11.display);
        return DisplayHeight(handle->x11.display, screen);
    } else {
        return 0;
    }
}

WindowRect x11LinuxWindow::getWindowRect(ConstSysHandle handle)
{
    if (handle == nullptr) return {0, 0,0,0};

    if (auto* windowObj = static_cast<bnWindow*>(handle->object)) {
        uint32_t width  = windowObj->getWindowWidth();
        uint32_t height = windowObj->getWindowHeight();
        XWindowAttributes gwa;
        XGetWindowAttributes(handle->x11.display, handle->x11.window, &gwa);
        ::Window childWindow;
        int screenX = 0, screenY = 0;

        XTranslateCoordinates(
            handle->x11.display,
            handle->x11.window,
            gwa.root,
            0, 0,
            &screenX, &screenY,
            &childWindow
        );

        return { .left = screenX, .top = screenY, .right = width, .bottom = height };
    }


    return {0, 0, 0,0};
}

WindowPoint x11LinuxWindow::getCursorPositionFromWindowSpace(ConstSysHandle handle)
{
    WindowPoint pt{};
    if (!handle->x11.display || !handle->x11.window) return pt;

    Window rootReturn, childReturn;
    int rootX, rootY;
    int winX, winY;
    unsigned int maskReturn;

    Bool result = XQueryPointer(
        static_cast<Display*>(handle->x11.display),
        static_cast<Window>(handle->x11.window),
        &rootReturn, &childReturn,
        &rootX, &rootY,
        &winX, &winY,
        &maskReturn
    );

    if (result) {
        pt.x = winX;
        pt.y = winY;
    }

    return pt;
}

WindowPoint x11LinuxWindow::getCursorPositionFromScreenSpace(ConstSysHandle handle)
{
    WindowPoint pt{0, 0};
    if (!handle->x11.display) return pt;

    auto* display = static_cast<Display*>(handle->x11.display);
    Window rootWindow = DefaultRootWindow(display);

    Window rootReturn, childReturn;
    int rootX = 0, rootY = 0;
    int winX = 0, winY = 0;
    unsigned int maskReturn = 0;

    // Queries position relative to the root window (Screen Space)
    Bool success = XQueryPointer(
        display,
        rootWindow,
        &rootReturn, &childReturn,
        &rootX, &rootY,
        &winX, &winY,
        &maskReturn
    );

    if (success) {
        pt.x = rootX;
        pt.y = rootY;
    }

    return pt;
}

u8 x11LinuxWindow::getDPIOfWindow(ConstSysHandle handle)
{
    if (!handle || !handle->x11.display || !handle->x11.window) {
        return 96; // Fallback default DPI
    }

    auto* display = static_cast<Display*>(handle->x11.display);
    Window window = static_cast<Window>(handle->x11.window);

    // 1. Get window attributes to find its current desktop position/geometry
    XWindowAttributes windowAttrs{};
    if (!XGetWindowAttributes(display, window, &windowAttrs)) {
        return 96;
    }

    // Translate window-relative coordinates (0,0) to root/screen-space coordinates
    int windowX = 0, windowY = 0;
    Window childWindow;
    XTranslateCoordinates(display, window, windowAttrs.root, 0, 0, &windowX, &windowY, &childWindow);

    int winCenterX = windowX + (windowAttrs.width / 2);
    int winCenterY = windowY + (windowAttrs.height / 2);

    // 2. Try querying per-monitor physical dimensions using Xrandr
    int xrandrEventBase = 0, xrandrErrorBase = 0;
    if (XRRQueryExtension(display, &xrandrEventBase, &xrandrErrorBase)) {
        int monitorCount = 0;
        XRRMonitorInfo* monitors = XRRGetMonitors(display, windowAttrs.root, True, &monitorCount);

        if (monitors && monitorCount > 0) {
            XRRMonitorInfo* targetMonitor = nullptr;

            // Find the monitor that contains the center of the window
            for (int i = 0; i < monitorCount; ++i) {
                const auto& mon = monitors[i];
                if (winCenterX >= mon.x && winCenterX < (mon.x + mon.width) &&
                    winCenterY >= mon.y && winCenterY < (mon.y + mon.height)) {
                    targetMonitor = &monitors[i];
                    break;
                }
            }

            // Fallback to primary or first monitor if window center isn't inside any monitor bounds
            if (!targetMonitor) {
                targetMonitor = &monitors[0];
            }

            // Calculate DPI from monitor physical dimensions (mm -> inches)
            if (targetMonitor->mwidth > 0 && targetMonitor->mheight > 0) {
                float dpiX = (static_cast<float>(targetMonitor->width)  / static_cast<float>(targetMonitor->mwidth))  * 25.4f;
                float dpiY = (static_cast<float>(targetMonitor->height) / static_cast<float>(targetMonitor->mheight)) * 25.4f;

                float calculatedDPI = (dpiX + dpiY) * 0.5f;

                XRRFreeMonitors(monitors);
                return static_cast<u8>(std::round(calculatedDPI));
            }

            XRRFreeMonitors(monitors);
        }
    }

    // 3. Fallback: Global X11 Screen DPI (used if Xrandr is unavailable)
    int screenNumber = XScreenNumberOfScreen(windowAttrs.screen);
    float displayWidthPx = static_cast<float>(DisplayWidth(display, screenNumber));
    float displayWidthMM = static_cast<float>(DisplayWidthMM(display, screenNumber));

    if (displayWidthMM > 0.0f) {
        float globalDPI = (displayWidthPx / displayWidthMM) * 25.4f;
        return static_cast<u8>(std::round(globalDPI));
    }

    return 96; // Standard baseline fallback
}

void x11LinuxWindow::processX11Events(Display* display)
{
    bnWindow* window = nullptr;
    if (!display) return;

    // Process all pending events in the X queue without blocking
    while (XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);

        // Retrieve object pointer associated with event's window
        XPointer ptr = nullptr;
        if (XFindContext(event.xany.display, event.xany.window, getXContext(), &ptr) == 0 && ptr) {
            window = reinterpret_cast<bnWindow*>(ptr);
            if (window != nullptr)
            {
                SysHandle handle = window->handle;
                WindowEvent winEvent{};
                winEvent.originalMessage = static_cast<u8>(event.type);

                switch (event.type)
                {
                    // --- Window Destroy ---
                case ClientMessage: {
                    // Check if this client message is WM_DELETE_WINDOW
                    if (static_cast<Atom>(event.xclient.data.l[0]) == handle->x11.wmDeleteMessage) {
                        winEvent.type = WindowEventType::Destroy;
                        window->windowCallback(&winEvent);
                    }
                    break;
                }

                // --- Window Resize / Move ---
                case ConfigureNotify: {
                    int newWidth = event.xconfigure.width;
                    int newHeight = event.xconfigure.height;

                    if (newWidth != window->getWindowWidth() || newHeight != window->getWindowHeight()) {
                        winEvent.type = WindowEventType::Resize;
                        winEvent.width = newWidth;
                        winEvent.height = newHeight;
                        window->windowCallback(&winEvent);
                    }
                    break;
                }

                case FocusIn: {
                    winEvent.type = WindowEventType::Activate;
                    winEvent.wordParameter = 1; // Focus gained
                    window->windowCallback(&winEvent);
                    break;
                }
                case FocusOut: {
                    winEvent.type = WindowEventType::Activate;
                    winEvent.wordParameter = 0; // Focus lost
                    window->windowCallback(&winEvent);
                    break;
                }

                // --- Mouse Motion ---
            case MotionNotify: {
                int mouseX = event.xmotion.x;
                int mouseY = event.xmotion.y;

                // Check if cursor falls within custom non-client frame/titlebar region
                if (window->isPointInNonClientArea({mouseX, mouseY})) {
                    winEvent.type = WindowEventType::NonClientMouseMove;
                } else {
                    winEvent.type = WindowEventType::MouseMove;
                }

                winEvent.x = mouseX;
                winEvent.y = mouseY;
                window->windowCallback(&winEvent);
                break;
            }

            // --- Mouse Buttons & Scroll Wheel ---
            case ButtonPress:
            case ButtonRelease: {
                bool isPress = (event.type == ButtonPress);
                unsigned int button = event.xbutton.button;

                // X11 encodes scroll wheel as buttons 4, 5, 6, and 7
                if (button == Button4 || button == Button5) {
                    if (isPress) { // Only handle on press to prevent double triggers
                        winEvent.type = WindowEventType::MouseWheel;
                        winEvent.delta = (button == Button4) ? 120 : -120; // 120 = Standard notch
                        winEvent.x = event.xbutton.x;
                        winEvent.y = event.xbutton.y;
                        window->windowCallback(&winEvent);
                    }
                    break;
                }

                int mouseX = event.xbutton.x;
                int mouseY = event.xbutton.y;
                bool isNonClient = window->isPointInNonClientArea({mouseX, mouseY});

                if (isPress) {
                    winEvent.type = isNonClient ? WindowEventType::NonClientMouseButtonDown
                                                : WindowEventType::MouseButtonDown;
                } else {
                    winEvent.type = isNonClient ? WindowEventType::NonClientMouseButtonUp
                                                : WindowEventType::MouseButtonUp;
                }

                winEvent.button = button; // Button1 = Left, Button2 = Middle, Button3 = Right
                winEvent.x = mouseX;
                winEvent.y = mouseY;
                window->windowCallback(&winEvent);
                break;
            }

                case KeyPress:
                case KeyRelease: {
                    bool isPress = (event.type == KeyPress);
                    winEvent.type = isPress ? WindowEventType::KeyDown : WindowEventType::KeyUp;
                    winEvent.scancode = event.xkey.keycode;

                    // Map keycode to ASCII/Keycode character
                    char keyBuffer[32] = {0};
                    KeySym keySym;
                    XLookupString(&event.xkey, keyBuffer, sizeof(keyBuffer), &keySym, nullptr);

                    winEvent.key = static_cast<int>(keySym);
                    winEvent.wordParameter = static_cast<u64>(keyBuffer[0]); // Character code
                    window->windowCallback(&winEvent);
                    break;
                }

                // --- Non-Client / Custom Title Bar Hit Test ---
                case EnterNotify: {
                    // Optional hit-test evaluation when mouse enters surface
                    winEvent.type = WindowEventType::NonClientHitTest;
                    winEvent.x = event.xcrossing.x;
                    winEvent.y = event.xcrossing.y;
                    window->windowCallback(&winEvent);
                    break;
                }
                }
            }
        }
    }
}

bool x11LinuxWindow::isMaximized(ConstSysHandle handle)
{
    Display* display = handle->x11.display;
    Window window = handle->x11.window;

    Atom atom_net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom atom_maximized_vert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
    Atom atom_maximized_horz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);

    if (atom_net_wm_state == 0) return false;

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;

    // Fetch the properties from the X server
    if (XGetWindowProperty(display, window, atom_net_wm_state, 0, 1024, False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success) {

        bool is_vert = false;
        bool is_horz = false;

        if (prop && actual_type == XA_ATOM) {
            Atom* atoms = reinterpret_cast<Atom*>(prop);
            for (unsigned long i = 0; i < nitems; ++i) {
                if (atoms[i] == atom_maximized_vert) is_vert = true;
                if (atoms[i] == atom_maximized_horz) is_horz = true;
            }
        }
        if (prop) XFree(prop);
        return is_vert && is_horz;
                           }

    return false;
}

bool x11LinuxWindow::isIconic(ConstSysHandle handle)
{

    Atom actual_type;
    int actual_format;
    unsigned long nitems = 0;
    unsigned long bytes_after = 0;
    unsigned char* prop = nullptr;

    Atom atom_net_wm_state = XInternAtom(handle->x11.display, "_NET_WM_STATE", True);
    Atom atom_hidden = XInternAtom(handle->x11.display, "_NET_WM_STATE_HIDDEN", True);

    if (atom_net_wm_state == 0 || atom_hidden == 0) {
        return false;
    }

    // Fetch the _NET_WM_STATE property array from the window
    int status = XGetWindowProperty(handle->x11.display, handle->x11.window, atom_net_wm_state, 0, 1024, False,
                                    XA_ATOM, &actual_type, &actual_format,
                                    &nitems, &bytes_after, &prop);

    bool is_iconic = false;
    if (status == Success && prop) {
        Atom* atoms = reinterpret_cast<Atom*>(prop);
        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == atom_hidden) {
                is_iconic = true;
                break;
            }
        }
        XFree(prop);
    }

    return is_iconic;
}

void x11LinuxWindow::setTitle(ConstSysHandle handle, const wchar_t* c_str)
{
    if (!handle) return;

    auto title = StringUtils::wcharToUtf8(c_str);
    auto* display = static_cast<Display*>(handle->x11.display);
    auto window = static_cast<Window>(handle->x11.window);

    if (!display || !window) return;
    Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8String = XInternAtom(display, "UTF8_STRING", False);
    XChangeProperty(
        display, window, netWmName, utf8String, 8,
        PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.c_str()),
        static_cast<int>(title.length())
    );
    XStoreName(display, window, title.c_str());

    XFlush(display);
}

void x11LinuxWindow::setCursor(SysHandle handle, const WindowCursor& cursor)
{
    if (!handle) return;

    auto* display = handle->x11.display;
    Window window = handle->x11.window;

    if (cursor.nativeHandle.x11.x11Cursor != 0) {
        XDefineCursor(display, window, cursor.nativeHandle.x11.x11Cursor);
    } else {
        XUndefineCursor(display, window); // Reset to default
    }
    XFlush(display);
}

WindowCursor x11LinuxWindow::createCursor(SysHandle handle, SystemCursorShape shape)
{
    if (!handle) return {};

    WindowCursor cursor{};

    auto* display = handle->x11.display;
    unsigned int shapeId = XC_left_ptr;

    switch (shape) {
    case SystemCursorShape::IBeam:    shapeId = XC_xterm; break;
    case SystemCursorShape::Hand:     shapeId = XC_hand2; break;
    case SystemCursorShape::ResizeNS: shapeId = XC_sb_v_double_arrow; break;
    case SystemCursorShape::ResizeEW: shapeId = XC_sb_h_double_arrow; break;
    default:                          shapeId = XC_left_ptr; break;
    }

    cursor.nativeHandle.x11.x11Cursor = XCreateFontCursor(display, shapeId);
    return cursor;
}

void x11LinuxWindow::maximizeWindow(ConstSysHandle handle)
{
    Display* display = handle->x11.display;
    ::Window window  = handle->x11.window;

    if (!display || !window) return;

    XEvent xev{};
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    xev.xclient.data.l[1] = static_cast<long>(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False));
    xev.xclient.data.l[2] = static_cast<long>(XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False));
    xev.xclient.data.l[3] = 1; // Normal source indication

    XSendEvent(
        display, DefaultRootWindow(display), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &xev
    );
    XFlush(display);
}

void x11LinuxWindow::minimizeWindow(ConstSysHandle handle)
{
    Display* display = handle->x11.display;
    Window window = handle->x11.window;

    int screen = DefaultScreen(display);
    XIconifyWindow(display, window, screen);
}

void x11LinuxWindow::showWindow(ConstSysHandle handle)
{
    if (handle->x11.display && handle->x11.window) {
        XMapWindow(handle->x11.display, handle->x11.window);
        XFlush(handle->x11.display);
    }
}

void x11LinuxWindow::hideWindow(ConstSysHandle handle)
{
    if (handle->x11.display && handle->x11.window) {
        XUnmapWindow(handle->x11.display, handle->x11.window);
        XFlush(handle->x11.display);
    }
}

void x11LinuxWindow::setIcon(ConstSysHandle handle, const u8* data, size_t size)
{
    if (handle->x11.display && handle->x11.window)
    {
        constexpr long imageSize = 256;
        Atom netWmIcon = XInternAtom(handle->x11.display, "_NET_WM_ICON", False);
        Atom cardinal = XInternAtom(handle->x11.display, "CARDINAL", False);

        // _NET_WM_ICON expects width, height, followed by ARGB pixel data formatted as 'unsigned long'
        size_t numPixels = imageSize^2;
        std::vector<unsigned long> iconBuffer(2 + numPixels);

        iconBuffer[0] = imageSize;
        iconBuffer[1] = imageSize;

        for (size_t i = 0; i < numPixels; ++i) {
            uint8_t r = data[i * 4 + 0];
            uint8_t g = data[i * 4 + 1];
            uint8_t b = data[i * 4 + 2];
            uint8_t a = data[i * 4 + 3];
            iconBuffer[2 + i] = (static_cast<unsigned long>(a) << 24) |
                                (static_cast<unsigned long>(r) << 16) |
                                (static_cast<unsigned long>(g) << 8)  |
                                (static_cast<unsigned long>(b));
        }

        XChangeProperty(
            handle->x11.display,
            handle->x11.window,
            netWmIcon,
            cardinal,
            32, // 32-bit format (unsigned long)
            PropModeReplace,
            reinterpret_cast<const unsigned char*>(iconBuffer.data()),
            static_cast<int>(iconBuffer.size())
        );

        XFlush(handle->x11.display);
    }
}

void x11LinuxWindow::setMoveResize(SysHandle handle, WindowEvent* event)
{
    Display* display = handle->x11.display;
    Window window = handle->x11.window;

    int direction = -1;

    if (event->wordParameter == static_cast<i64>(HitTestResult::Caption)) {
        direction = 8; // _NET_WM_MOVERESIZE_MOVE (Move via titlebar/caption)
    } else {
        switch (event->wordParameter) {
            case (u64)HitTestResult::Top:          direction = 0; break; // _NET_WM_MOVERESIZE_TOP
            case (u64)HitTestResult::Bottom:       direction = 1; break; // _NET_WM_MOVERESIZE_BOTTOM
            case (u64)HitTestResult::Left:         direction = 2; break; // _NET_WM_MOVERESIZE_LEFT
            case (u64)HitTestResult::Right:        direction = 3; break; // _NET_WM_MOVERESIZE_RIGHT
            case (u64)HitTestResult::TopLeft:      direction = 4; break; // _NET_WM_MOVERESIZE_TOP_LEFT
            case (u64)HitTestResult::TopRight:     direction = 5; break; // _NET_WM_MOVERESIZE_TOP_RIGHT
            case (u64)HitTestResult::BottomLeft:   direction = 6; break; // _NET_WM_MOVERESIZE_BOTTOM_LEFT
            case (u64)HitTestResult::BottomRight:  direction = 7; break; // _NET_WM_MOVERESIZE_BOTTOM_RIGHT
            default: return;
        }
    }


    XUngrabPointer(display, CurrentTime);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.xclient.type = ClientMessage;
    xev.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    xev.xclient.display = display;
    xev.xclient.window = window;
    xev.xclient.format = 32;

    xev.xclient.data.l[0] = event->x;
    xev.xclient.data.l[1] = event->y;
    xev.xclient.data.l[2] = direction;
    xev.xclient.data.l[3] = event->button;
    xev.xclient.data.l[4] = 1;

    // Send the message to the root window
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(display);
}

void x11LinuxWindow::setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info)
{
    Display* display = handle->x11.display;
    Window window = handle->x11.window;
    XSizeHints* hints = XAllocSizeHints();
    if (!hints) return;

    long provided = 0;
    XGetWMNormalHints(display, window, hints, &provided);

    hints->flags |= PMinSize;
    hints->min_width  = static_cast<int>(info.minWidth);
    hints->min_height = static_cast<int>(info.minHeight);
    hints->flags |= PMaxSize;
    hints->max_width  = static_cast<int>(info.maxWidth);
    hints->max_height = static_cast<int>(info.maxHeight);


    XSetWMNormalHints(display, window, hints);
    XFree(hints);
    XFlush(display);
}

void x11LinuxWindow::resizeWindow(ConstSysHandle handle, long width, long height)
{
    XResizeWindow(handle->x11.display, handle->x11.window, width, height);
    XFlush(handle->x11.display);
}
#endif
