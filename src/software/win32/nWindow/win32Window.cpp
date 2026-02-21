// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "win32Window.h"
#include "nWindow/bnWindow.h"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "Comctl32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Propsys.lib")

LRESULT win32Window::winCallStatic(HWND handle, UINT message, WPARAM w_param, LPARAM l_param) {
    bnWindow* window = nullptr;

    if (!handle) return 0;

    if (message == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(l_param);
        window = static_cast<bnWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return DefWindowProc(handle, message, w_param, l_param);
    }

    if (message == WM_NCDESTROY) {
        SetWindowLongPtr(handle, GWLP_USERDATA, 0);
        return DefWindowProc(handle, message, w_param, l_param);
    }

    window = reinterpret_cast<bnWindow*>(GetWindowLongPtr(handle, GWLP_USERDATA));
    if (!window) return DefWindowProc(handle, message, w_param, l_param);

    window->handle = handle;
    WindowEvent event;
    event.wordParameter = w_param;
    event.longParameter = l_param;
    event.originalMessage = message;
    event.type = WindowEventType::None;
    POINT cursor_point;
    GetCursorPos(&cursor_point);
    ScreenToClient(handle, &cursor_point);
    event.x = cursor_point.x;
    event.y = cursor_point.y;
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    event.width = clientRect.right;
    event.height = clientRect.bottom;

    switch (message) {
    case WM_PAINT: {
        event.type = WindowEventType::InitalizeDraw;
        break;
    }
    case WM_NCCALCSIZE: {
        event.type = WindowEventType::NonClientAreaCalcSize;
        break;
    }
    case WM_CREATE: {
        event.type = WindowEventType::Initalize;
        break;
    }
    case WM_NCLBUTTONDBLCLK: {
        event.type = WindowEventType::NonClientButtonDoubleClick;
        break;
    }
    case WM_KEYUP: {
        event.type = WindowEventType::KeyUp;
        break;
    }
    case WM_ACTIVATE: {
        event.type = WindowEventType::Activate;
        break;
    }
    case WM_NCHITTEST: {
        event.type = WindowEventType::NonClientHitTest;
        break;
    }
    case WM_NCMOUSEMOVE: {
        event.type = WindowEventType::NonClientMouseMove;
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        event.type = WindowEventType::MouseMove;
        break;
    }
    case WM_NCLBUTTONDOWN: {
        event.type = WindowEventType::NonClientMouseButtonDown;
        event.button = 0;
        break;
    }
    case WM_NCLBUTTONUP: {
        event.type = WindowEventType::NonClientMouseButtonUp;
        event.button = 0;
        break;
    }
    case WM_NCRBUTTONUP: {
        event.type = WindowEventType::NonClientMouseButtonUp;
        event.button = 1;
        break;
    }
    case WM_SETCURSOR: {
        event.type = WindowEventType::SetCursor;
        break;
    }
    case WM_DESTROY: {
        event.type = WindowEventType::Destroy;
        break;
    }
    case WM_NCMOUSELEAVE: {
        event.type = WindowEventType::NonClientMouseLeave;
        break;
    }
    case WM_GETMINMAXINFO:
    {
        event.type = WindowEventType::GetMinMaxInfo;
        break;
    }
    case WM_CAPTURECHANGED: {
        return TRUE;
    }
    case WM_SIZE: {
       event.type = WindowEventType::Resize;
        break;
    }
    case WM_LBUTTONDOWN: {
        event.type = WindowEventType::MouseButtonDown;
        event.button = 0;
         break;
    }
    case WM_RBUTTONDOWN: {
    event.type = WindowEventType::MouseButtonDown;
    event.button = 1;
     break;
    }
    case WM_LBUTTONUP: {
    event.type = WindowEventType::MouseButtonUp;
    event.button = 0;
        break;
    }
    case WM_RBUTTONUP: {
    event.type = WindowEventType::MouseButtonUp;
    event.button = 1;
        break;
    }
    case WM_KEYDOWN: {
    event.type = WindowEventType::KeyDown;
    event.key = w_param;
        break;
    }
    default: {
        event.type = WindowEventType::None;
        break;
    }
    }

    switch (window->windowCallback(&event)) {
        case (WindowCallbackCodes::DEFAULT): {
            return DefWindowProc(handle, message, w_param, l_param);
        }
        case (WindowCallbackCodes::CUSTOM_RESULT): {
            return event.customResult;
        }
        case (WindowCallbackCodes::OKAY): {
            return 1;
        }
        case (WindowCallbackCodes::ISSUE): {
            return 0;
        }
        default:
            return 0;
    }
}

void win32Window::paintBasedOnConfig(const bnWindowConstructorStruct *configuration, SysHandle handle) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(handle, &ps);

    // Fill the window background with a solid color (light gray here)
    HBRUSH hBrush = CreateSolidBrush(RGB(configuration->clearColor.r, configuration->clearColor.g, configuration->clearColor.b));
    FillRect(hdc, &ps.rcPaint, hBrush);
    DeleteObject(hBrush);

    EndPaint(handle, &ps);
}

WindowRect win32Window::getWindowRect(const SysHandle handle) {
    RECT rect;
    GetClientRect(handle, &rect);
    return convertFromWin32Rect(rect);
}

WindowPoint win32Window::getCursorPositionFromWindowSpace(const SysHandle handle) {
    POINT ptOriginal;
    GetCursorPos(&ptOriginal);
    ScreenToClient(handle, &ptOriginal);

    return convertFromWin32Point(ptOriginal);
}

WindowPoint win32Window::getCursorPositionFromScreenSpace() {
    POINT ptOriginal;
    GetCursorPos(&ptOriginal);

    return convertFromWin32Point(ptOriginal);
}

WindowSize win32Window::getThemePartSize(const SysHandle handle, const wchar_t *themeName, int partId, int stateId) {
    SIZE sizeOriginal;
    HTHEME theme = OpenThemeData(handle, themeName);
    GetThemePartSize(theme, NULL, partId, stateId, NULL, TS_TRUE, &sizeOriginal);
    CloseThemeData(theme);

    return convertFromWin32Size(sizeOriginal);
}

u8 win32Window::getDPIOfWindow(SysHandle handle) {
    return GetDpiForWindow(handle);
}

WindowPoint win32Window::convertFromWin32Point(POINT point) {
    WindowPoint convertedPoint;
    convertedPoint.x = point.x;
    convertedPoint.y = point.y;
    return convertedPoint;
}

POINT win32Window::convertToWin32Point(WindowPoint point) {
    POINT convertedPoint;
    convertedPoint.x = point.x;
    convertedPoint.y = point.y;
    return convertedPoint;
}

WindowRect win32Window::convertFromWin32Rect(RECT point) {
    WindowRect convertedRect;
    convertedRect.bottom = point.bottom;
    convertedRect.left = point.left;
    convertedRect.right = point.right;
    convertedRect.top = point.top;
    return convertedRect;
}

RECT win32Window::convertToWin32Rect(WindowRect point) {
    RECT convertedRect;
    convertedRect.bottom = point.bottom;
    convertedRect.left = point.left;
    convertedRect.right = point.right;
    convertedRect.top = point.top;
    return convertedRect;
}

WindowSize win32Window::convertFromWin32Size(const SIZE point) {
    WindowSize convertedSize;
    convertedSize.cx = point.cx;
    convertedSize.cy = point.cy;
    return convertedSize;
}

SIZE win32Window::convertToWin32Size(const SIZE point) {
    SIZE convertedSize;
    convertedSize.cx = point.cx;
    convertedSize.cy = point.cy;
    return convertedSize;
}

bool win32Window::isPointInRect(const WindowRect *rect, const WindowPoint point) {
    return point.x >= rect->left && point.x <= rect->right && point.y >= rect->top && point.y <= rect->bottom;
}



