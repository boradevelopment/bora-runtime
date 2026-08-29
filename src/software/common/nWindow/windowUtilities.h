#pragma once
#include "bnWindowAbstracts.h"

enum class MessageBoxIcon {
    None = 0,
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
    static WindowRect getWindowRect(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(ConstSysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace(ConstSysHandle handle);
    static u8 getDPIOfWindow(ConstSysHandle handle);
    static long getScreenWidth(SysHandle handle);
    static long getScreenHeight(SysHandle handle);
    static void setCursor(SysHandle handle, const WindowCursor& cursor);
    static WindowCursor createCursor(SysHandle handle, SystemCursorShape shape);

    static void maximizeWindow(ConstSysHandle handle);
    static void showWindow(ConstSysHandle handle);
    static void hideWindow(SysHandle handle);
    static void setIcon(ConstSysHandle handle, const u8* data, size_t size);
    static bool isPointInRect(WindowRect* window_rect, WindowPoint pt);
    static void setTitle(ConstSysHandle handle, const wchar_t* c_str);
};


