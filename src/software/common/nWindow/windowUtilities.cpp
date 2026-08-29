#include "windowUtilities.h"

#include "nWindow/linuxWindow.h"
#include "nWindow/gio/gioWindowUtilities.h"
#include "nWindow/gtk/gtkWindowUtilities.h"

#if defined(_WIN32)
#include <windows.h>

UINT getNativeIconFlag(MessageBoxIcon icon) {
    switch (icon) {
    case MessageBoxIcon::Info:     return MB_ICONINFORMATION;
    case MessageBoxIcon::Warning:  return MB_ICONWARNING;
    case MessageBoxIcon::Error:    return MB_ICONERROR;
    case MessageBoxIcon::Question: return MB_ICONQUESTION;
    default:                       return 0;
    }
}
#endif

void windowUtilities::createSystemMessageBox(SysHandle handle, const char* title, const char* message,
    MessageBoxIcon icons)
{
#ifdef __linux__
    bool state = false;
    if constexpr (gioWindowUtilities::supported())
    {
        state = gioWindowUtilities::createMessageBox(handle, title, message, icons);
    }
    if (!state) // try GTK
    {
        state = gtkWindowUtilities::createMessageBox(handle, title, message, icons);
#endif
    }
}

void windowUtilities::createSystemMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message,
    MessageBoxIcon icons)
{
#ifdef __linux__
    bool state = false;
    if constexpr (gioWindowUtilities::supported())
    {
        state = gioWindowUtilities::createMessageBox(handle, title, message, icons);
    }
    if (!state) // try GTK
    {
        state = gtkWindowUtilities::createMessageBox(handle, title, message, icons);
    }
#endif
}

WindowRect windowUtilities::getWindowRect(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::getWindowRect(handle);
#elif defined(__linux__)
    return linuxWindow::getWindowRect(handle);
#endif
}

WindowPoint windowUtilities::getCursorPositionFromWindowSpace(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::getCursorPositionFromWindowSpace(handle);
#elif defined(__linux__)
    return linuxWindow::getCursorPositionFromWindowSpace(handle);
#endif
}

WindowPoint windowUtilities::getCursorPositionFromScreenSpace(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::getCursorPositionFromScreenSpace(handle);
#elif defined(__linux__)
    return linuxWindow::getCursorPositionFromScreenSpace(handle);
#endif
}

u8 windowUtilities::getDPIOfWindow(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::getDPIOfWindow(handle);
#elif defined(__linux__)
    return linuxWindow::getDPIOfWindow(handle);
#endif
}

long windowUtilities::getScreenWidth(SysHandle handle)
{
#ifdef WIN32
    return win32Window::getScreenWidth(handle);
#elif defined(__linux__)
    return linuxWindow::getScreenWidth(handle);
#endif
}

long windowUtilities::getScreenHeight(SysHandle handle)
{
#ifdef WIN32
    return win32Window::getScreenHeight(handle);
#elif defined(__linux__)
    return linuxWindow::getScreenHeight(handle);
#endif
}

void windowUtilities::setCursor(SysHandle handle, const WindowCursor& cursor)
{
#ifdef WIN32
    return win32Window::setCursor(handle, cursor);
#elif defined(__linux__)
    return linuxWindow::setCursor(handle, cursor);
#endif
}

WindowCursor windowUtilities::createCursor(SysHandle handle, SystemCursorShape shape)
{
#ifdef WIN32
    return win32Window::createCursor(handle, shape);
#elif defined(__linux__)
    return linuxWindow::createCursor(handle, shape);
#endif
}

void windowUtilities::maximizeWindow(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::maximizeWindow(handle);
#elif defined(__linux__)
    return linuxWindow::maximizeWindow(handle);
#endif
}

void windowUtilities::showWindow(ConstSysHandle handle)
{
#ifdef WIN32
    return win32Window::showWindow(handle);
#elif defined(__linux__)
    return linuxWindow::showWindow(handle);
#endif
}

void windowUtilities::hideWindow(SysHandle handle)
{
#ifdef WIN32
    return win32Window::hideWindow(handle);
#elif defined(__linux__)
    return linuxWindow::hideWindow(handle);
#endif
}

void windowUtilities::setIcon(ConstSysHandle handle, const u8* data, size_t size)
{
#ifdef WIN32
    return win32Window::setIcon(handle, data, size);
#elif defined(__linux__)
    return linuxWindow::setIcon(handle, data, size);
#endif
}

bool windowUtilities::isPointInRect(WindowRect* window_rect, WindowPoint pt)
{
    if (!window_rect) {
        return false;
    }

    return (pt.x >= window_rect->left) &&
           (pt.x <  window_rect->right) &&
           (pt.y >= window_rect->top) &&
           (pt.y <  window_rect->bottom);
}

void windowUtilities::setTitle(ConstSysHandle handle, const wchar_t* c_str)
{
#ifdef WIN32
    return win32Window::setTitle(handle, c_str);
#elif defined(__linux__)
    return linuxWindow::setTitle(handle, c_str);
#endif
}
