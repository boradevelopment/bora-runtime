#pragma once
#include "nWindow/bnWindowAbstracts.h"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

class darwinWindow {
public:
    template <typename T>
    static SysHandle createWindow(T* object, bnWindowConstructorStruct* configuration, bool isChild = false);

    static void pollEvents();
    static void paintBasedOnConfig(const bnWindowConstructorStruct* configuration, SysHandle handle);

    // Gets
    static WindowRect getWindowRect(const SysHandle handle);
    static WindowPoint getCursorPositionFromWindowSpace(const SysHandle handle);
    static WindowPoint getCursorPositionFromScreenSpace();
    static WindowSize getThemePartSize(const SysHandle handle, const wchar_t* theme, int partId, int stateId);
    static u8 getDPIOfWindow(SysHandle handle);

    // Conversions
    static WindowPoint convertFromWin32Point(const NSPoint point);
    static NSPoint convertToDarwinPoint(const WindowPoint point);
    static WindowRect convertFromWin32Rect(const NSRect point);
    static NSRect convertToDarwinRect(const WindowRect point);
    static WindowSize convertFromWin32Size(const NSSize point);
    static NSSize convertToDarwinSize(const WindowSize point);

    static bool isPointInRect(const WindowRect* rect, const WindowPoint point);
};

