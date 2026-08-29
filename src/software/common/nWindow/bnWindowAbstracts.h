#pragma once
#include "Data.h"

#if !defined(WIN32)
typedef void* HINSTANCE;
#endif

inline sVec<SysHandle> handles;

enum SysButtonFlags : u8 {
    CLOSE = 1 << 0, // 0000 0001
    MINIMIZE = 1 << 1, // 0000 0010
    MAXIMIZE = 1 << 2, // 0000 0100
    ALL = CLOSE | MINIMIZE | MAXIMIZE
};

// Shown means that there is no hide logic by default, it will always be shown
// Hide UnFocus means that it will hide if the cursor isn't focusing on the titlebar in more than 3 seconds (if the titlebar show shortcut is pressed, it will be shown for 5 seconds)
// Hide Always means it will never been shown unless the application explicitly tells the titlebar to show or hide it.
enum TitleBarProperties : u8 {
    SHOWN = 1 << 0, // 0000 0001
    HIDEUNFOCUS = 1 << 1, // 0000 0010
    HIDEALWAYS = 1 << 2, // 0000 0100
    NO_LOGIC = 1 << 3,
    DEFAULT = SHOWN
};

typedef struct WindowRect {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;

    [[nodiscard]] constexpr long GetWidth() const { return right - left; }
    [[nodiscard]] constexpr long GetHeight() const { return bottom - top; }

    [[nodiscard]] constexpr bool IsEmpty() const {
        return GetWidth() <= 0 || GetHeight() <= 0;
    }
} WindowRect;

inline bool operator!=(const WindowRect& a, const WindowRect& b) {
    return (a.left != b.left || a.top != b.top || a.right != b.right || a.bottom != b.bottom);
}

typedef struct WindowPoint {
    long x = 0;
    long y = 0;
} WindowPoint;

typedef struct WindowSize
{
    long cx = 0;
    long cy = 0;
} WindowSize;


struct bnWindowTitlebarConfig {
    rgb borderColor = {0, 0, 0};
    rgb backgroundColor = { 0, 0, 0 };
    rgb buttonHoverColor = { 50, 50, 50 };
    rgb buttonPressedColor = { 100, 100,100 };
    rgb closeButtonColor = {180, 0, 0};
    rgb closeButtonPressedColor = { 225, 0, 0 };
    u8 sysButtons = SysButtonFlags::ALL;
    u8 properties = TitleBarProperties::HIDEUNFOCUS;
    float hideSpeed = 8.5f;
    bool enabled = true;
};

struct bnWindowConstructorStruct {
    long width = -1;
    long height = -1;
    HINSTANCE hInstance;
    sWstring id = L"Window";
    sWstring title;
    int frameLimit = -1;
    Data* logo = nullptr;
    rgba clearColor = { 255, 255, 255, 1.0f };
    rgb titleBarColor = { 0, 0, 0 };
    u8 aliasLevelCount = 8;
    bool enableAntiAliasing = true;
    bnWindowTitlebarConfig* titleBarConfig = nullptr;
};

enum class WindowEventType {
    Unknown,
    Destroy,
    Initalize,
    InitalizeDraw,
    NonClientAreaCalcSize,
    NonClientHitTest,
    NonClientMouseMove,
    NonClientMouseButtonDown,
    NonClientButtonDoubleClick,
    NonClientMouseButtonUp,
    NonClientMouseLeave,
    GetMinMaxInfo,
    Activate,
    SetCursor,
    Resize,
    KeyDown,
    KeyUp,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel
};

enum class WindowCallbackCodes {
    OKAY,
    ISSUE,
    DEFAULT,
    CUSTOM_RESULT
};

struct WindowEvent {
    WindowEventType type = WindowEventType::Unknown;
    u8 originalMessage;
    u64 wordParameter;
    i64 longParameter;

    i64 customResult;
    // Keyboard
    int key       = 0;  // converted to character
    int scancode  = 0;

    // Mouse
    int x = 0;
    int y = 0;
    int button = 0;
    int delta = 0;           // Wheel

    // Window
    int width  = 0;
    int height = 0;
};

enum class SystemCursorShape {
    Arrow,
    IBeam,
    Hand,
    ResizeNS,
    ResizeEW
};

#ifdef __linux
#include "nWindow/linuxAbstracts.h"
#endif

struct WindowCursor {
    SystemCursorShape shape = SystemCursorShape::Arrow;
#ifdef __linux__
    LinuxCursorHandle nativeHandle;
#endif
};


#ifndef WIN32

struct WindowMinMaxInfo
{
    long minWidth, minHeight, maxWidth, maxHeight;
};

enum class HitTestResult : u64 {
    Nowhere     = 0,
    Client      = 1,
    Caption     = 2,
    Left        = 10,
    Right       = 11,
    Top         = 12,
    TopLeft     = 13,
    TopRight    = 14,
    Bottom      = 15,
    BottomLeft  = 16,
    BottomRight = 17
};

inline HitTestResult PerformHitTest(int x, int y, int width, int height, int borderMargin = 6, int captionHeight = 30) {
    bool top    = y < borderMargin;
    bool bottom = y >= (height - borderMargin);
    bool left   = x < borderMargin;
    bool right  = x >= (width - borderMargin);

    if (top && left)     return HitTestResult::TopLeft;
    if (top && right)    return HitTestResult::TopRight;
    if (bottom && left)  return HitTestResult::BottomLeft;
    if (bottom && right) return HitTestResult::BottomRight;
    if (top)             return HitTestResult::Top;
    if (bottom)          return HitTestResult::Bottom;
    if (left)            return HitTestResult::Left;
    if (right)           return HitTestResult::Right;

    if (y < captionHeight) return HitTestResult::Caption;

    return HitTestResult::Client;
}
#endif