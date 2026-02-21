#include "bnWindowTitlebar.h"
#include "bnWindow.h"
#include "nCommon/MemoryCommands.h"
#include "software/common/Utils.h"
#ifdef WIN32
#pragma comment(lib, "usp10.lib")
#endif

struct TitleBarCache {
    std::wstring lastTitle;
    float lastMinimizeAlpha = -1.0f;
    float lastMaximizeAlpha = -1.0f;
    float lastCloseAlpha = -1.0f;
    u8 lastSysButtons = 0;
    WindowRect lastRect;
    bnWindowTitlebar::CustomTitleBarPressedButton lastPressedButton;
    int width;
    int height;
};

TitleBarCache titleBarCache;

bnWindowTitlebar::bnWindowTitlebar(bnWindow& window, bnWindowTitlebarConfig& config) : window(window), config(config)
{
   titleFont = mgr.getFont("Segoe UI", 16);
}

bnWindowTitlebar::~bnWindowTitlebar()
{
    Release();
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};

    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(),
        nullptr, 0, nullptr, nullptr
    );

    std::string strTo(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(),
        strTo.data(), size_needed, nullptr, nullptr
    );

    return strTo;
}

void bnWindowTitlebar::Release()
{
    if (cachedLogo) {
        delete cachedLogo.release();
    }
    BYTE* lastBuffer = titleBarImage.load();
    if (lastBuffer) {
        delete[] lastBuffer;
        titleBarImage.store(nullptr);
    }
}

auto makePaint = [](SkColor color, bool aa = true, float strokeWidth = 0.0f, bool fill = true) {
    SkPaint p;
    p.setAntiAlias(aa);
    p.setColor(color);
    if (fill) {
        p.setStyle(SkPaint::kFill_Style);
    }
    else {
        p.setStyle(SkPaint::kStroke_Style);
        p.setStrokeWidth(strokeWidth);
    }
    return p;
    };

void bnWindowTitlebar::Update()
{
    if (!config.enabled) return;


    if (llpMMI) {
        updateLPMMI(llpMMI);
        RECT clientRect;
        GetClientRect(window.handle, &clientRect);
        int width = clientRect.right;

        if (llpMMI->ptMinTrackSize.x > width) {
            // call resize
            DWORD style = GetWindowLong(window.handle, GWL_STYLE);
            DWORD exStyle = GetWindowLong(window.handle, GWL_EXSTYLE);
            RECT wndRect = { 0, 0, llpMMI->ptMinTrackSize.x, llpMMI->ptMinTrackSize.y };
            AdjustWindowRectEx(&wndRect, style, FALSE, exStyle);

            int newWidth = wndRect.right - wndRect.left;
            int newHeight = wndRect.bottom - wndRect.top;

            // Apply new size, keep top-left position
            SetWindowPos(window.handle, nullptr, 0, 0, newWidth, newHeight,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        llpMMI = nullptr;
    }


    if (config.properties == HIDEUNFOCUS) {
        WindowPoint pt = win32Window::getCursorPositionFromWindowSpace(window.handle);

        WindowRect winRect = win32_titlebar_rect(true);
        bool atTop = win32Window::isPointInRect(&winRect, pt);
        auto now = std::chrono::steady_clock::now();

        if (atTop) {
            if (!cursorOnTop) {
                cursorEnterTime = now;
                cursorOnTop = true;
            }

            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - cursorEnterTime).count();
            if (diff > 155) {
                isHidden = false;
            }

        }
        else {
            if (cursorOnTop) {
                cursorLeaveTime = now;
                cursorOnTop = false;
            }

            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - cursorLeaveTime).count();
            if (diff > 255) {
                isHidden = true;
            }
        }
    }
    else if (config.properties == HIDEALWAYS) {
        isHidden = true;
    }
    else if (config.properties == SHOWN) {
        isHidden = false;
    }

    UpdateHidePos();

    std::wstring title = window.configuration->title;
    // An application must be stable enough to not get the frozenProcess trigger
    // The text will stay for 5 more frames to ensure that the text won't disappear
    // If the application's still unstable.
    if (window.frozenProcess || framesAfterFrozen < 5) {
        framesAfterFrozen = 0;
        title.append(L" (Not Responding)");
    }
    if (!window.frozenProcess && framesAfterFrozen < 5) {
        framesAfterFrozen++;
    }

    SetWindowTextW(window.handle, title.c_str());
    auto crBorderColor = RGB(config.borderColor.r, config.borderColor.g, config.borderColor.b);

    DwmSetWindowAttribute(
        window.handle,
        DWMWA_BORDER_COLOR,
        &crBorderColor,
        sizeof(crBorderColor)
    );

    HoverFadeState oldMin = minimizeFade;
    HoverFadeState oldMax = maximizeFade;
    HoverFadeState oldClose = closeFade;

  
    UpdateFade(minimizeFade);
    UpdateFade(maximizeFade);
    UpdateFade(closeFade);
    if (isHidden && hideOffset == 1.0f) {
        titleBarBit = {};
        UpdateBuffer(nullptr, 0);
        return; // Waste of resources
     }

    WindowRect title_bar_rect = win32_titlebar_rect();
    if (!hasChanged()) return;
    titleBarCache.lastRect = title_bar_rect;
    titleBarCache.lastSysButtons = config.sysButtons;
    lastLogoPtr = window.configuration->logo->getDataPtr()->data();
    titleBarCache.lastTitle = title;
    titleBarCache.lastMinimizeAlpha = minimizeFade.alpha;
    titleBarCache.lastMaximizeAlpha = maximizeFade.alpha;
    titleBarCache.lastCloseAlpha = closeFade.alpha;
    titleBarCache.lastPressedButton = title_bar_pressed_button;

    SkImageInfo info = SkImageInfo::Make(
        (title_bar_rect.right - title_bar_rect.left),
        (title_bar_rect.bottom - title_bar_rect.top),
        kBGRA_8888_SkColorType,   // matches Win32 DIB
        kPremul_SkAlphaType
    );

    size_t rowBytes = info.minRowBytes();
    std::vector<uint8_t> pixels(rowBytes * (title_bar_rect.bottom - title_bar_rect.top));

    auto canvas = SkCanvas::MakeRasterDirect(
        info,
        pixels.data(),
        rowBytes
    );

    if (!canvas) return;

    COLORREF title_bar_color = RGB(config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b);
    COLORREF title_bar_hover_color = RGB(config.buttonHoverColor.r, config.buttonHoverColor.g, config.buttonHoverColor.b);
    COLORREF title_bar_hover_pressed_color = RGB(config.buttonPressedColor.r, config.buttonPressedColor.g, config.buttonPressedColor.b);

    canvas->clear({ config.backgroundColor.r / 255, config.backgroundColor.g / 255, config.backgroundColor.b / 255, 255 });
    //canvas->drawTextBlob()f
    CustomTitleBarButtonRects button_rects = win32_get_title_bar_button_rects(&title_bar_rect);

    UINT dpi = GetDpiForWindow(window.handle);
    int icon_dimension = win32_dpi_scale(10, dpi);

    if (config.sysButtons & SysButtonFlags::MINIMIZE) {
        if (minimizeFade.alpha > 0.0f) {
            SkColor c = BlendColor(
                title_bar_pressed_button == CustomTitleBarPressedButton_Minimize
                ? title_bar_hover_pressed_color
                : title_bar_hover_color,
                minimizeFade.alpha);

            SkRect r = SkRect::MakeLTRB(
                (float)button_rects.minimize.left,
                (float)button_rects.minimize.top,
                (float)button_rects.minimize.right,
                (float)button_rects.minimize.bottom);

            canvas->drawRect(r, makePaint(c));
        }

        WindowRect icon_rect_min;
        icon_rect_min.right = icon_dimension;
        icon_rect_min.bottom = 1;
        win32_center_rect_in_rect(&icon_rect_min, &button_rects.minimize);

        SkRect iconRect = SkRect::MakeLTRB(
            (float)icon_rect_min.left,
            (float)icon_rect_min.top,
            (float)icon_rect_min.right,
            (float)icon_rect_min.bottom);

        SkColor textCol = SkColorSetRGB(
            255,
            255,
            255);

        canvas->drawRect(iconRect, makePaint(textCol));
    }

    // Maximize button
    if (config.sysButtons & SysButtonFlags::MAXIMIZE) {
        if (maximizeFade.alpha > 0.0f) {
            SkColor c = BlendColor(
                title_bar_pressed_button == CustomTitleBarPressedButton_Maximize
                ? title_bar_hover_pressed_color
                : title_bar_hover_color,
                maximizeFade.alpha);

            SkRect r = SkRect::MakeLTRB(
                (float)button_rects.maximize.left,
                (float)button_rects.maximize.top,
                (float)button_rects.maximize.right,
                (float)button_rects.maximize.bottom);

            canvas->drawRect(r, makePaint(c));
        }

        WindowRect icon_rect_max;
        icon_rect_max.right = icon_dimension;
        icon_rect_max.bottom = icon_dimension;
        win32_center_rect_in_rect(&icon_rect_max, &button_rects.maximize);

        SkRect iconRect = SkRect::MakeLTRB(
            (float)icon_rect_max.left,
            (float)icon_rect_max.top,
            (float)icon_rect_max.right,
            (float)icon_rect_max.bottom);

        if (win32_window_is_maximized()) {
            SkRect maximizedRect = SkRect::MakeXYWH(
                (float)icon_rect_max.left + WIN32_MAXIMIZED_RECTANGLE_OFFSET,
                (float)icon_rect_max.top - WIN32_MAXIMIZED_RECTANGLE_OFFSET,
                (float)(icon_rect_max.right - icon_rect_max.left),
                (float)(icon_rect_max.bottom - icon_rect_max.top));

            canvas->drawRect(maximizedRect, makePaint(SK_ColorWHITE, false, 1.0f, false));

            SkColor bColor2 = BlendColor(
                title_bar_hovered_button == CustomTitleBarHoveredButton_Maximize
                ? title_bar_hover_color
                : title_bar_color,
                1.0f);

            SkColor fillCol = (title_bar_pressed_button == CustomTitleBarPressedButton_Maximize)
                ? SkColorSetRGB(
                    GetRValue(title_bar_hover_pressed_color),
                    GetGValue(title_bar_hover_pressed_color),
                    GetBValue(title_bar_hover_pressed_color))
                : bColor2;

            canvas->drawRect(iconRect, makePaint(fillCol));
        }

        if (title_bar_pressed_button == CustomTitleBarPressedButton_Maximize) {
            canvas->drawRect(iconRect, makePaint(SK_ColorWHITE));
        }

        canvas->drawRect(iconRect, makePaint(SK_ColorWHITE, false, 1.0f, false));
    }

    // Close button
    if (config.sysButtons & SysButtonFlags::CLOSE) {
        if (closeFade.alpha > 0.0f) {
            SkColor c = BlendColor(
                title_bar_pressed_button == CustomTitleBarPressedButton_Close
                ? RGB(config.closeButtonPressedColor.r,
                    config.closeButtonPressedColor.g,
                    config.closeButtonPressedColor.b)
                : RGB(config.closeButtonColor.r,
                    config.closeButtonColor.g,
                    config.closeButtonColor.b),
                closeFade.alpha);

            SkRect r = SkRect::MakeLTRB(
                (float)button_rects.close.left,
                (float)button_rects.close.top,
                (float)button_rects.close.right,
                (float)button_rects.close.bottom);

            canvas->drawRect(r, makePaint(c));
        }

        WindowRect icon_rect_close;
        icon_rect_close.right = icon_dimension;
        icon_rect_close.bottom = icon_dimension;
        win32_center_rect_in_rect(&icon_rect_close, &button_rects.close);

        SkPaint pen = makePaint(SK_ColorWHITE, true, 1.5f, false);
        canvas->drawLine(
            (float)icon_rect_close.left, (float)icon_rect_close.top,
            (float)icon_rect_close.right, (float)icon_rect_close.bottom,
            pen);

        canvas->drawLine(
            (float)icon_rect_close.left, (float)icon_rect_close.bottom,
            (float)icon_rect_close.right, (float)icon_rect_close.top,
            pen);
    }

    //if (!fontTypeFace) {
    //    printf("Typeface is null!\n");
    //}

    //if (!fontTypeFace->countGlyphs()) {
    //    printf("Typeface has no glyphs!\n");
    //}


    SkPaint textPaint;
    textPaint.setColor(SkColorSetARGB(
        255,
        255,
        255,
        255
    ));
    textPaint.setAntiAlias(true);  // smooth text


    int icon_right = !window.hIcon ? 5 : 5 + 32 + win32_dpi_scale(5, dpi);
    int button_left = button_rects.getFirstAvailable().left - win32_dpi_scale(5, dpi);
    if (button_left < 0) button_left = title_bar_rect.right - win32_dpi_scale(5, dpi);
    int logo_width = window.hIcon ? 32 + win32_dpi_scale(5, dpi) : 0;
    int text_left = 5 + logo_width;  // leave margin + logo
    int text_right = button_left;
    int available_width = text_right - text_left;
    int title_height = title_bar_rect.bottom - title_bar_rect.top;

    // Skia uses float for coordinates
    SkRect textRect = SkRect::MakeLTRB(
        static_cast<float>(icon_right),
        static_cast<float>(title_bar_rect.top),
        static_cast<float>(button_left),
        static_cast<float>(title_bar_rect.bottom)
    );

    auto u16 = wstring_to_utf16(title);


  //  if (textWidth > available_width) {
    if (titleFont.measureText(u16.c_str(), u16.size() * sizeof(char16_t), SkTextEncoding::kUTF16) > available_width) {
        // truncate
        float ellipsisWidth = titleFont.measureText(u"\u2026", sizeof(char16_t), SkTextEncoding::kUTF16);
        size_t len = u16.size();
        while (len > 0) {
            auto w = titleFont.measureText(u16.data(), len * sizeof(char16_t), SkTextEncoding::kUTF16);
            if (w + ellipsisWidth <= available_width) break;
            len--;
        }

        // add ellipsis
        if (len > 0) {
            u16.resize(len);
            u16.push_back(u'\u2026');
        }
    }
   // }

        SkRect bounds;
        titleFont.measureText(u16.data(), u16.size() * sizeof(char16_t), SkTextEncoding::kUTF16, &bounds);

        // Horizontal centering
        float textWidth = bounds.width();
        float x = !window.hIcon ? 4 : text_left + (available_width - textWidth) / 2.0f;

        // Vertical centering
        float textHeight = bounds.height();
        float y = title_bar_rect.top + (title_height - textHeight) / 2.0f - bounds.top();

    auto blob = SkTextBlob::MakeFromText(
        u16.data(),
        u16.size() * sizeof(char16_t),
        titleFont,
        SkTextEncoding::kUTF16
    );

        if (available_width > 15) {
        canvas->drawTextBlob(blob, x, y, textPaint);
    }

    if (window.configuration->logo != nullptr && window.configuration->logo->getState() == 1) {

        if (cachedLogoSource != window.configuration->logo) {
            // Delete old cached image
            delete cachedLogo.release();


            // Load new image from memory
            cachedLogo = LoadImageFromMemory(
                window.configuration->logo->getData().data(),
                window.configuration->logo->getData().size()
            );

            cachedLogoSource = window.configuration->logo; // update cached source
        }

        if (cachedLogo) {
            //graphics.DrawImage(cachedLogo, 5, 3, 32, 32);
            SkSamplingOptions sampling(SkCubicResampler::Mitchell());
            float size = title_bar_rect.bottom - title_bar_rect.top;
            float wh = std::min(size, 32.0f);

            canvas->drawImageRect(
                cachedLogo,
                SkRect::MakeXYWH(5, 0, wh, wh),
                sampling,
                nullptr
            );
        }
    }
    else {
       // printf("Unsupported Image Format!");
    }

    titleBarBit.bmType = 0;  // 0 = device-independent bitmap
    titleBarBit.bmWidth = (title_bar_rect.right - title_bar_rect.left);   // width of the bitmap
    titleBarBit.bmHeight = (title_bar_rect.bottom - title_bar_rect.top); // height
    titleBarBit.bmWidthBytes = rowBytes;
    titleBarBit.bmPlanes = 1;
    titleBarBit.bmBitsPixel = 32;
    titleBarBit.bmBits = pixels.data();

    if (flickerEffect) {
        const size_t pixelCount = pixels.size() / 4; // 4 bytes per pixel

        for (size_t i = 0; i < pixelCount; ++i) {
            size_t offset = i * 4;
            // Windows bitmaps are usually BGRA, adjust if needed
            pixels[offset + 0] = 255 - pixels[offset + 0]; // B
            pixels[offset + 1] = 255 - pixels[offset + 1]; // G
            pixels[offset + 2] = 255 - pixels[offset + 2]; // R
            // Alpha remains the same
            // pixels[offset + 3] = pixels[offset + 3]; 
        }
    }

    UpdateBuffer(pixels.data(), pixels.size());

}

void bnWindowTitlebar::OnNCMouseMove()
{
    if (!config.enabled) return;
    mouseOnBar = true; 
    WindowPoint cursor_point = win32Window::getCursorPositionFromWindowSpace(window.handle);
    HCURSOR hCursor = LoadCursor(nullptr, IDC_ARROW);
    SetCursor(hCursor);

    WindowRect title_bar_rect = win32_titlebar_rect();
    CustomTitleBarButtonRects button_rects = win32_get_title_bar_button_rects(&title_bar_rect);

    CustomTitleBarHoveredButton new_hovered_button = CustomTitleBarHoveredButton_None;
    if (win32Window::isPointInRect(&button_rects.close, cursor_point)) {
        new_hovered_button = CustomTitleBarHoveredButton_Close;
    }
    else if (win32Window::isPointInRect(&button_rects.minimize, cursor_point)) {
        new_hovered_button = CustomTitleBarHoveredButton_Minimize;
    }
    else if (win32Window::isPointInRect(&button_rects.maximize, cursor_point)) {
        new_hovered_button = CustomTitleBarHoveredButton_Maximize;
    }

    // Update hover states for fade
    minimizeFade.hovered = (new_hovered_button == CustomTitleBarHoveredButton_Minimize);
    maximizeFade.hovered = (new_hovered_button == CustomTitleBarHoveredButton_Maximize);
    closeFade.hovered = (new_hovered_button == CustomTitleBarHoveredButton_Close);

    if (new_hovered_button != title_bar_hovered_button) {
            // InvalidateRect(window.handle, &button_rects.close, FALSE);
            // InvalidateRect(window.handle, &button_rects.minimize, FALSE);
            // InvalidateRect(window.handle, &button_rects.maximize, FALSE);

        title_bar_hovered_button = new_hovered_button;
    }

  
    window.tBarChanged = true;

    if (new_hovered_button != CustomTitleBarHoveredButton_None) {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE | TME_NONCLIENT, window.handle, 0 };
        TrackMouseEvent(&tme);
    }
}

void bnWindowTitlebar::OnMouseMove()
{
    mouseOnBar = false;
    if (!config.enabled) return;
    if (title_bar_hovered_button != CustomTitleBarHoveredButton_None) {
        title_bar_hovered_button = CustomTitleBarHoveredButton_None;
        minimizeFade.hovered = false;
        maximizeFade.hovered = false;
        closeFade.hovered = false;
    }
    if (title_bar_pressed_button != CustomTitleBarPressedButton_None) {
        title_bar_pressed_button = CustomTitleBarPressedButton_None;
    }
}

int bnWindowTitlebar::OnNCMouseButtonDown()
{
    if (!config.enabled) return 1;
    if (title_bar_hovered_button != CustomTitleBarPressedButton_None) {
        if (title_bar_hovered_button == CustomTitleBarHoveredButton_Close) {
            title_bar_pressed_button = CustomTitleBarPressedButton_Close;
        }
        if (title_bar_hovered_button == CustomTitleBarHoveredButton_Minimize) {
            title_bar_pressed_button = CustomTitleBarPressedButton_Minimize;
        }
        if (title_bar_hovered_button == CustomTitleBarHoveredButton_Maximize) {
            title_bar_pressed_button = CustomTitleBarPressedButton_Maximize;
        }
     
        return 0;
    }

    InvalidateRect(window.handle, NULL, FALSE);
    title_bar_pressed_button = CustomTitleBarPressedButton_None;
    window.tBarChanged = true;
    return 1;
}

void bnWindowTitlebar::OnNCMouseButtonUp()
{
    if (!config.enabled) return;
    title_bar_pressed_button = CustomTitleBarPressedButton_None;
    window.firstFrame = true;
    InvalidateRect(window.handle, NULL, FALSE);

    if (title_bar_hovered_button == CustomTitleBarHoveredButton_Close) {
        PostMessageW(window.handle, WM_CLOSE, 0, 0);
        return;
    }
    else if (title_bar_hovered_button == CustomTitleBarHoveredButton_Minimize) {
        ShowWindow(window.handle, SW_MINIMIZE);
        return;
    }
    else if (title_bar_hovered_button == CustomTitleBarHoveredButton_Maximize) {
        int mode = win32_window_is_maximized() ? SW_NORMAL : SW_MAXIMIZE;
        ShowWindow(window.handle, mode);
        return;
    }

}

void bnWindowTitlebar::OnNCRMouseButtonUp(LPARAM l_param)
{
    if (!config.enabled) return;
        BOOL isMaximized = IsZoomed(window.handle);
        MENUITEMINFO menu_item_info = {};
        menu_item_info.cbSize = sizeof(menu_item_info);
        menu_item_info.fMask = MIIM_STATE;

        HMENU sys_menu = GetSystemMenu(window.handle, false);

        // First, check if our item already exists by scanning for the label
        const UINT_PTR customItemID = 1; // use a unique, harmless ID
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING;
        wchar_t buffer[256];
        mii.dwTypeData = buffer;
        mii.cch = ARRAYSIZE(buffer);

        bool found = false;
        for (int i = 0; GetMenuItemInfoW(sys_menu, i, TRUE, &mii); ++i) {
            if (wcscmp(buffer, L"--- BORA ---") == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            InsertMenuW(sys_menu, 0, MF_BYPOSITION | MF_STRING | MF_DISABLED, customItemID, L"--- BORA ---");
        }

        set_menu_item_state(sys_menu, &menu_item_info, SC_RESTORE, isMaximized);
        set_menu_item_state(sys_menu, &menu_item_info, SC_MOVE, !isMaximized);
        set_menu_item_state(sys_menu, &menu_item_info, SC_SIZE, !isMaximized);
        set_menu_item_state(sys_menu, &menu_item_info, SC_MINIMIZE, (config.sysButtons & SysButtonFlags::MINIMIZE) != 0);
        set_menu_item_state(sys_menu, &menu_item_info, SC_MAXIMIZE, (config.sysButtons & SysButtonFlags::MAXIMIZE) != 0 && !isMaximized);
        set_menu_item_state(sys_menu, &menu_item_info, SC_CLOSE, (config.sysButtons & SysButtonFlags::CLOSE) != 0);
        BOOL result = TrackPopupMenu(sys_menu, TPM_RETURNCMD, GET_X_PARAM(l_param), GET_Y_PARAM(l_param), 0, window.handle, NULL);
        if (result != 0) {
            PostMessage(window.handle, WM_SYSCOMMAND, result, 0);
        }
    }

LRESULT bnWindowTitlebar::OnNCHitTest(LPARAM l_param)
{
    if (!config.enabled) return HTTRANSPARENT;
    if (title_bar_hovered_button == CustomTitleBarHoveredButton_Maximize) {
        return HTMAXBUTTON;
    }

    UINT dpi = GetDpiForWindow(window.handle);
    int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
    int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    POINT cursor_point = { GET_X_PARAM(l_param), GET_Y_PARAM(l_param) };
    ScreenToClient(window.handle, &cursor_point);

    if (!win32_window_is_maximized() && cursor_point.y > 0 && cursor_point.y < frame_y + padding) {
        return HTTOP;
    }

    if (cursor_point.y < win32_titlebar_rect().bottom) {
        return HTCAPTION;
    }
    return HTCLIENT;
}

void bnWindowTitlebar::OnNCCalcResult(LPARAM l_param)
{
  //  if (!config.enabled) return;
    UINT dpi = GetDpiForWindow(window.handle);

    int frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
    int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
    int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

    NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)l_param;
    RECT* requested_client_rect = params->rgrc;

    requested_client_rect->right -= frame_x + padding;
    requested_client_rect->left += frame_x + padding;
    requested_client_rect->bottom -= frame_y + padding;

    if (win32_window_is_maximized()) {
        requested_client_rect->top += padding;
    }
}

void bnWindowTitlebar::OnGetMaxInfo(LPARAM l_param)
{
    if (!config.enabled) return;

    LPMINMAXINFO lpMMI = (LPMINMAXINFO)l_param;
    updateLPMMI(lpMMI);

    llpMMI = lpMMI;
    return;
}

void bnWindowTitlebar::OnNCMouseLeave()
{
    if (!config.enabled) return;
    if (title_bar_hovered_button != CustomTitleBarHoveredButton_None) {
        title_bar_hovered_button = CustomTitleBarHoveredButton_None;
        title_bar_pressed_button = CustomTitleBarPressedButton_None;
        window.updateAll = true;
        minimizeFade.hovered = false;
        maximizeFade.hovered = false;
        closeFade.hovered = false;
    }
}

SkColor bnWindowTitlebar::BlendColor(COLORREF baseColor, float alpha)
{
    BYTE r = GetRValue(baseColor);
    BYTE g = GetGValue(baseColor);
    BYTE b = GetBValue(baseColor);
    BYTE a = static_cast<BYTE>(alpha * 255);
    return SkColorSetARGB(a, r, g, b);
}

WindowRect bnWindowTitlebar::win32_fake_shadow_rect(HWND handle)
{
    WindowRect rect = win32Window::getWindowRect(handle);
    rect.bottom = rect.top + WIN32_FAKE_SHADOW_HEIGHT;
    return rect;
}

void bnWindowTitlebar::set_menu_item_state(HMENU menu, MENUITEMINFO* menuItemInfo, UINT item, bool enabled)
{
    menuItemInfo->fState = enabled ? MF_ENABLED : MF_DISABLED;
    SetMenuItemInfo(menu, item, false, menuItemInfo);
}

void bnWindowTitlebar::UpdateFade(HoverFadeState& state, const float speed)
{
    if (state.hovered) {
        if (state.alpha < 1.0f)
            state.alpha = std::min(1.0f, state.alpha + (speed * gDeltaTimes[window.handle]));
    }
    else {
        if (state.alpha > 0.0f)
            state.alpha = std::max(0.0f, state.alpha - (speed * gDeltaTimes[window.handle]));
    }

    return;
}

int bnWindowTitlebar::win32_dpi_scale(int value, UINT dpi)
{
    return (int)((float)value * dpi / 96);
}

inline void bnWindowTitlebar::win32_center_rect_in_rect(WindowRect* to_center, const WindowRect* outer_rect)
{
    int to_width = to_center->right - to_center->left;
    int to_height = to_center->bottom - to_center->top;
    int outer_width = outer_rect->right - outer_rect->left;
    int outer_height = outer_rect->bottom - outer_rect->top;

    int padding_x = (outer_width - to_width) / 2;
    int padding_y = (outer_height - to_height) / 2;

    to_center->left = outer_rect->left + padding_x;
    to_center->top = outer_rect->top + padding_y;
    to_center->right = to_center->left + to_width;
    to_center->bottom = to_center->top + to_height;
}

bool bnWindowTitlebar::hasChanged()
{
    bool needsRedraw =  false;
    WindowRect title_bar_rect = win32_titlebar_rect();
    std::wstring title = window.configuration->title;
    if (window.frozenProcess) {
        title.append(L" (Not Responding)");
    }

    if(title_bar_pressed_button != titleBarCache.lastPressedButton) needsRedraw = true;
    if (title_bar_rect != titleBarCache.lastRect) needsRedraw = true;
    if (title != titleBarCache.lastTitle) needsRedraw = true;
    if (lastLogoPtr != window.configuration->logo->getDataPtr()->data()) needsRedraw = true;
    if (minimizeFade.alpha != titleBarCache.lastMinimizeAlpha) needsRedraw = true;
    if (maximizeFade.alpha != titleBarCache.lastMaximizeAlpha) needsRedraw = true;
    if (closeFade.alpha != titleBarCache.lastCloseAlpha) needsRedraw = true;
    if (config.sysButtons != titleBarCache.lastSysButtons) needsRedraw = true;
    if (hideOffset > 0 && hideOffset < 1) needsRedraw = true;
    if (window.updateAll) needsRedraw = true;

    changed = needsRedraw;

    return needsRedraw;
}

inline bnWindowTitlebar::CustomTitleBarButtonRects bnWindowTitlebar::win32_get_title_bar_button_rects(const WindowRect* title_bar_rect)
{
    //UINT dpi = GetDpiForWindow(window.handle);
    //CustomTitleBarButtonRects button_rects;
    //int button_width = win32_dpi_scale(47, dpi);
    //button_rects.close = *title_bar_rect;
    //button_rects.close.top += WIN32_FAKE_SHADOW_HEIGHT;

    //button_rects.close.left = button_rects.close.right - button_width;
    //button_rects.maximize = button_rects.close;
    //button_rects.maximize.left -= button_width;
    //button_rects.maximize.right -= button_width;
    //button_rects.minimize = button_rects.maximize;
    //button_rects.minimize.left -= button_width;
    //button_rects.minimize.right -= button_width;
    //return button_rects;

    UINT dpi = GetDpiForWindow(window.handle);
    CustomTitleBarButtonRects button_rects{};
    int button_width = win32_dpi_scale(47, dpi);

    // Start at the right edge of the title bar
    WindowRect current = *title_bar_rect;
    current.top += WIN32_FAKE_SHADOW_HEIGHT;
    current.left = current.right - button_width;

    // Helper lambda to assign and move left
    auto assign_button = [&](WindowRect& dest) {
        dest = current;
        // Move one button left
        current.right -= button_width;
        current.left -= button_width;
        };

    
    if (config.sysButtons & SysButtonFlags::CLOSE) {
        assign_button(button_rects.close);
    }
    if (config.sysButtons & SysButtonFlags::MAXIMIZE) {
        assign_button(button_rects.maximize);
    }
    if (config.sysButtons & SysButtonFlags::MINIMIZE) {
        assign_button(button_rects.minimize);
    }

    return button_rects;
}

bool bnWindowTitlebar::win32_window_is_maximized()
{
    WINDOWPLACEMENT placement = { 0 };
    placement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(window.handle, &placement)) {
        return placement.showCmd == SW_SHOWMAXIMIZED;
    }
    return false;
}

WindowRect bnWindowTitlebar::win32_titlebar_rect(bool noHideOffset)
{
    WindowSize title_bar_size = win32Window::getThemePartSize(window.handle, L"WINDOW", WP_CAPTION, CS_ACTIVE);
    UINT dpi = win32Window::getDPIOfWindow(window.handle);
    const int top_and_bottom_borders = 2;
    int height = win32_dpi_scale(title_bar_size.cy, dpi) + top_and_bottom_borders;

    WindowRect rect = win32Window::getWindowRect(window.handle);
    if (noHideOffset) {
        rect.bottom = rect.top + height;
    }
    else {
        int offset = static_cast<int>(height * hideOffset);
        rect.bottom = rect.top + height - offset;
    }
    
    return rect;
}

float EaseInOut(float t) {
    return t * t * (3.0f - 2.0f * t); // smoothstep
}

float EaseInOutOpposite(float t) {
    return 1.0f - EaseInOut(1.0f - t);
}


void bnWindowTitlebar::UpdateHidePos()
{
    float target = isHidden ? 1.0f : 0.0f;

    // Move progress toward target
    if (isHidden && animProgress < 1.0f) {
        animProgress = std::min(animProgress + gDeltaTimes[window.handle] / animDuration, 1.0f);
        hideOffset = EaseInOut(animProgress);
    }
    else if (!isHidden && animProgress > 0.0f) {
        animProgress = std::max((float)(animProgress - gDeltaTimes[window.handle] / animDuration * 1.5), 0.0f);
        hideOffset = EaseInOutOpposite(animProgress);
    }

    // Apply easing curve
  
}

void bnWindowTitlebar::updateLPMMI(LPMINMAXINFO lpMMI)
{
    auto titleRect = win32_titlebar_rect();

    if (!lpMMI) return;

    lpMMI->ptMinTrackSize.y = (titleRect.bottom - titleRect.top) * 2;

    CustomTitleBarButtonRects button_rects = win32_get_title_bar_button_rects(&titleRect);

    if ((config.sysButtons & SysButtonFlags::MAXIMIZE) == 0) {
        lpMMI->ptMinTrackSize.x = window.configuration->width;
        lpMMI->ptMinTrackSize.y = window.configuration->height;
        lpMMI->ptMaxTrackSize.x = window.configuration->width;
        lpMMI->ptMaxTrackSize.y = window.configuration->height;
    }
    else {

        int pHValue = 80 * button_rects.totalAvailable();
        int pTValue = 60 * button_rects.totalAvailable();

        // Set minimum size
        if (window.hIcon != nullptr) lpMMI->ptMinTrackSize.x = pHValue; // Texts go in reverse beyond this limit, and it may glitch with the ICON with the ICON, so i added a limit
        else if (window.configuration->title.empty()) lpMMI->ptMinTrackSize.x = pTValue;
        else {
            SIZE textSize = {};
            HDC hdc = GetDC(window.handle);
            LOGFONT logical_font;
            SystemParametersInfoForDpi(SPI_GETICONTITLELOGFONT, sizeof(logical_font), &logical_font, false, GetDpiForWindow(window.handle));

            HFONT theme_font = CreateFontIndirect(&logical_font);
            HFONT hFontOld = (HFONT)SelectObject(hdc, theme_font);

            sWstring titleCut = window.configuration->title;
            if (titleCut.length() > 15) {
                titleCut = titleCut.substr(0, 15); // keep only first 15 chars
            }

            GetTextExtentPoint32(hdc, titleCut.c_str(), (int)titleCut.length(), &textSize);
            SelectObject(hdc, hFontOld);
            ReleaseDC(window.handle, hdc);

            int padding = 241;

            lpMMI->ptMinTrackSize.x = textSize.cx + padding;

            if (lpMMI->ptMinTrackSize.x < 241) {
                lpMMI->ptMinTrackSize.x = 241;
            }
        }
    }

    
}

void bnWindowTitlebar::RenderFrame(bnGraphics* graphics, GraphicsAdvanced* advanced, GraphicAdvancedCommandList* list,
    ResourceHandle<IInputLayout>* inputLayout,
    ResourceHandle<ISamplerState>* samplerState,
    ResourceHandle<IBlendState>* blending,
    ResourceHandle<IRasterizerState>* rast,
    ResourceHandle<IDepthStencilState>* depth,ResourceHandle<ICommandPool>* copyTexturePool,
    ResourceHandle<IShader>* vertexShader,
    ResourceHandle<IShader>* pixelShader, ResourceHandle<IViewPort>* viewport,
    ResourceHandle<IDescriptorPool>* dPool,
    ResourceHandle<IDescriptorSetLayout>* dSetLayout
)
{   
    graphics->PushGroup("Title Bar Render");
    bool changed = this->changed;

    if (changed) {
        graphics->ReleaseTexture(&titleBarTexture);
        graphics->ReleaseBuffer(&titleBarVertice);
    }

    auto title_bar_rect = win32_titlebar_rect();

    if (titleBarBit.bmWidth == 0) return;

    if (config.enabled) {
        if (titleBarBit.bmWidth != 0) {

            
            bool exists = titleBarTexture.Exists();
            if (!exists && changed) {
                TextureDesc tbDesc;
                tbDesc.format = window.choice == VULKAN ? TextureFormat::BGRA8_UNorm_SRGB : TextureFormat::BGRA8_UNorm;
                tbDesc.width = titleBarBit.bmWidth;
                tbDesc.height = titleBarBit.bmHeight;
                tbDesc.widthBytes = titleBarBit.bmWidthBytes;
                tbDesc.CpuAccessWrite = true;

                graphics->CreateTexture(tbDesc, nullptr, &titleBarTexture);

            }
        }

        if (!titleBarVertice.Exists() && changed) {
            RECT rect;
            GetClientRect(window.handle, &rect);

            float aspectRatio = (float)titleBarBit.bmHeight / ((rect.bottom - rect.top) - (float)(title_bar_rect.bottom - title_bar_rect.top));
            float titlebarRatio = (float)titleBarBit.bmHeight / ((rect.bottom - rect.top) - (float)(title_bar_rect.bottom - title_bar_rect.top));

            Vertex vertices[] = {
                { -1.0f, 1.0f,                          0.0f, 0.0f, 0.0f }, // top-left
                {  1.0f, 1.0f,                          0.0f, 1.0f, 0.0f }, // top-right
                { -1.0f, (1.0f - 2.0f * titlebarRatio), 0.0f, 0.0f, 1.0f }, // bottom-left
                {  1.0f, (1.0f - 2.0f * titlebarRatio), 0.0f, 1.0f, 1.0f }, // bottom-right
            };

            BufferDesc tbBufferDesc;
            tbBufferDesc.type = BufferType::Vertex;
            tbBufferDesc.size = std::size(vertices);
            tbBufferDesc.byteWidth = sizeof(Vertex) * std::size(vertices);
            tbBufferDesc.stride = sizeof(Vertex);
            tbBufferDesc.dynamic = false;

            graphics->CreateBuffer(tbBufferDesc, &vertices, &titleBarVertice);
        }

    }


    if (changed) {
        BufferDesc stagingDesc = {};
        stagingDesc.size = titleBarBit.bmWidthBytes * titleBarBit.bmHeight * 4;
        stagingDesc.type = BufferType::Staging;

        if (!stagingBuffer.Exists()) graphics->CreateBuffer(stagingDesc, nullptr, &stagingBuffer);
        advanced->MapBufferMemory(&stagingBuffer, &data);

        for (int y = 0; y < titleBarBit.bmHeight; ++y) {
            MemoryCommands::Copy(advanced->getCommands(),
                &data,
                titleBarImage + y * titleBarBit.bmWidthBytes,
                titleBarBit.bmWidthBytes,
                y * titleBarBit.bmWidthBytes);
        }

        advanced->UnmapBufferMemory(&stagingBuffer);

        BufferImageCopyDesc region{};
        region.imageSubresource.aspectMask = IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            static_cast<uint32_t>(titleBarBit.bmWidth),
            static_cast<uint32_t>(titleBarBit.bmHeight),
            1
        };


        auto poolCommand = advanced->BeginSingleTimeCommands(copyTexturePool);
        advanced->CopyBufferToImage(poolCommand, &stagingBuffer, &titleBarTexture, region);
        advanced->EndSingleTimeCommands(poolCommand);
        graphics->ReleaseBuffer(&stagingBuffer);
    }

    if (!pipeline || !set) {
        auto builderObj = advanced->CreatePipelineBuilder();
        auto builder = builderObj->Get();

        advanced->BuilderAddShader(builderObj, pixelShader);
        advanced->BuilderAddShader(builderObj, vertexShader);
        advanced->BuilderSetBlendState(builderObj, blending);
        advanced->BuilderSetInputLayout(builderObj, inputLayout);
        advanced->BuilderSetRasterizer(builderObj, rast);
        advanced->BuilderSetDepthStencil(builderObj, depth);
        advanced->BuilderSetDescriptorPool(builderObj, dPool);
        advanced->BuilderSetDescriptorSetLayout(builderObj, dSetLayout);

        pipeline = advanced->CreatePipeline(builderObj);
        set = advanced->CreateDescriptorSet(pipeline);

        changed = true;
    }

    if (changed) {
        advanced->SetDescriptorSetTexture(set, &titleBarTexture);
        advanced->SetDescriptorSetSamplerState(set, samplerState);
        advanced->UpdateDescriptorSet(set);
    }


    list->BindViewPort(viewport);
    list->BindScissor(viewport);
    list->BindPipeline(pipeline);
    list->BindDescriptorSet(set);
    list->BindBuffer(&titleBarVertice);
    list->Draw(PrimitiveType::TriangleStrip, 4);
    graphics->PopGroup();

}

void bnWindowTitlebar::ReleaseRenderVars(IGraphicsDevice* graphics)
{
    if (pipeline) {
        pipeline->Get()->Release();
        set->DestroyHandle();
        pipeline->DestroyHandle();
        set = nullptr;
        pipeline = nullptr;
    }
    graphics->ReleaseBuffer(titleBarVertice);
    graphics->ReleaseTexture(titleBarTexture);
}

