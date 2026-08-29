// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "linuxWindow.h"

WindowRect linuxWindow::getWindowRect(ConstSysHandle handle)
{
    if (handle == nullptr) return {0,0,0,0};
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getWindowRect(handle);
    }
    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getWindowRect(handle);
    }

    return {0, 0,0,0};
}

WindowPoint linuxWindow::getCursorPositionFromWindowSpace(ConstSysHandle handle)
{
    if (handle == nullptr) return {0,0};
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getCursorPositionFromWindowSpace(handle);
    }
    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getCursorPositionFromWindowSpace(handle);
    }

    return {0,0};
}

WindowPoint linuxWindow::getCursorPositionFromScreenSpace(ConstSysHandle handle)
{
    if (handle == nullptr) return {0,0};
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getCursorPositionFromScreenSpace(handle);
    }
    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getCursorPositionFromScreenSpace(handle);
    }

    return {0,0};
}

u8 linuxWindow::getDPIOfWindow(ConstSysHandle handle)
{
    if (handle == nullptr) return 0;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getDPIOfWindow(handle);
    }
    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getDPIOfWindow(handle);
    }

    return 0;
}

long linuxWindow::getScreenWidth(ConstSysHandle handle)
{
    if (handle == nullptr) return 0;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getScreenWidth(handle);
    }
    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getScreenWidth(handle);
    }

    return 0;
}

long linuxWindow::getScreenHeight(ConstSysHandle handle)
{
    if (handle == nullptr) return 0;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::getScreenHeight(handle);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::getScreenHeight(handle);
    }

    return 0;
}

void linuxWindow::setTitle(ConstSysHandle handle, const wchar_t* c_str)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::setTitle(handle, c_str);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::setTitle(handle, c_str);
    }
}

void linuxWindow::setCursor(SysHandle handle, const WindowCursor& cursor)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::setCursor(handle, cursor);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::setCursor(handle, cursor);
    }
}

WindowCursor linuxWindow::createCursor(SysHandle handle, SystemCursorShape shape)
{
    if (handle == nullptr) return {};
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::createCursor(handle, shape);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::createCursor(handle, shape);
    }

    return {};
}

void linuxWindow::maximizeWindow(ConstSysHandle handle)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::maximizeWindow(handle);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::maximizeWindow(handle);
    }
}

void linuxWindow::showWindow(ConstSysHandle handle)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::showWindow(handle);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::showWindow(handle);
    }
}

void linuxWindow::hideWindow(SysHandle handle)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::hideWindow(handle);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::hideWindow(handle);
    }
}

void linuxWindow::setIcon(ConstSysHandle handle, const u8* data, size_t size)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::setIcon(handle, data, size);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::setIcon(handle, data, size);
    }
}

void linuxWindow::SetMoveResize(SysHandle handle, WindowEvent* event)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::setMoveResize(handle, event);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::setMoveResize(handle, event);
    }
}

void linuxWindow::setLPMMI(ConstSysHandle handle, const WindowMinMaxInfo& info)
{
    if (handle == nullptr) return;
    if (handle->type == DisplayServerType::Wayland)
    {
        return waylandLinuxWindow::setLPMMI(handle, info);
    }

    if (handle->type == DisplayServerType::X11)
    {
        return x11LinuxWindow::setLPMMI(handle, info);
    }
}
