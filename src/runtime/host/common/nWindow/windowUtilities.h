#pragma once
#include "bnWindowAbstracts.h"

enum class MessageBoxIcon {
    NoIcon = 0,
    Info,
    Warning,
    Error,
    Question
};

class windowUtilities
{
public:
    static void createSystemMessageBox(SysHandle handle, const char* title, const char* message, MessageBoxIcon icons);
    static void createSystemMessageBox(SysHandle handle, const wchar_t* title, const wchar_t* message, MessageBoxIcon icons);

    static void SendMessage(SysHandle handle, unsigned int msg, long long wParam = 0, long long lParam = 0);
    static void ReceiveMessages(SysHandle handle);

    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static long getScreenWidth(SysHandle handle);
    static long getScreenHeight(SysHandle handle);
    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);

    static bool isMaximized(ConstSysHandle handle);
    static bool isIconic(SysHandle handle);

    static void maximizeWindow(ConstSysHandle handle);
    static void minimizeWindow(SysHandle handle);
    static void showWindow(SysHandle handle);
    static void hideWindow(SysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
    static bool isPointInRect(WindowRect* window_rect, WindowPoint pt);
    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
    static void SetMoveResize(SysHandle handle, WindowEvent* event);
    static void resizeWindow(ConstSysHandle handle, long width, long height);
};


