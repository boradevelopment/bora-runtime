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
} WindowRect;

inline boolean operator!=(WindowRect a, WindowRect b) {
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
    None,
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
    WindowEventType type = WindowEventType::None;
    u8 originalMessage;
    u64Pointer wordParameter;
    i64Pointer longParameter;

    i64Pointer customResult;
    // Keyboard
    int key       = 0;      // Engine keycode
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
