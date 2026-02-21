#pragma once
#include "Data.h"
#include "Utils.h"
#include "chrono"
#include "bskia/include/core/SkSurface.h"
#include "bskia/include/core/SkCanvas.h"
#include "bskia/include/core/SkPaint.h"
#include "bskia/include/core/SkFont.h"
#include "bskia/include/core/SkImage.h"
#include "bskia/include/core/SkImageInfo.h"
#include "bskia/include/core/SkData.h"
#include "bskia/include/core/SkStream.h"
#include "bskia/include/core/SkShader.h"
#include "bskia/include/effects/SkGradientShader.h"
#include "nGUI/bnFontMgr.h"
#include "nGraphics/graphSimple.h"
#include "nGraphics/graphAdvanced.h"
#include "nGraphics/bnGraphics.h"
#include "bnWindowAbstracts.h"

class bnWindow;

/// <b>Bora Native Window Titlebar</b>
/// \n This class is a subsystem designed to manage and control the input and output of the titlebar system inside of a bnWindow instance.
/// \n Originally, the titlebar was supposed to be apart of the bnWindow class but the titlebar design and system became so overcomplicated that seperating it into another class would be ideal for effiency
class bnWindowTitlebar
{
public:
    bnWindowTitlebar(bnWindow& window, bnWindowTitlebarConfig& config);
    ~bnWindowTitlebar();

    void Release();

    void Update();
    void OnNCMouseMove();
    void OnMouseMove();
    int OnNCMouseButtonDown();
    void OnNCMouseButtonUp();
    void OnNCRMouseButtonUp(LPARAM l_param);
    LRESULT OnNCHitTest(LPARAM l_param);
    void OnNCCalcResult(LPARAM l_param);
    void OnGetMaxInfo(LPARAM l_param);
    void OnNCMouseLeave();

    void Show() { isHidden = false; };
    void Hide() { isHidden = true; };

bool CopyBuffer(BYTE* destination, size_t maxSize) {
    BYTE* buffer = titleBarImage.load(std::memory_order_acquire);
    size_t size = titleBarImageSize.load(std::memory_order_acquire);

    if (!buffer || size > maxSize) return false;  // Prevent buffer overflow

    memcpy(destination, buffer, size);
    return true;
}

typedef enum {
    CustomTitleBarHoveredButton_None = 0,
    CustomTitleBarHoveredButton_Minimize,
    CustomTitleBarHoveredButton_Maximize,
    CustomTitleBarHoveredButton_Close,
} CustomTitleBarHoveredButton;

enum CustomTitleBarPressedButton {
    CustomTitleBarPressedButton_None,
    CustomTitleBarPressedButton_Minimize,
    CustomTitleBarPressedButton_Maximize,
    CustomTitleBarPressedButton_Close
};

public:

    void UpdateBuffer(const BYTE* source, size_t size) {
        BYTE* newBuffer = new BYTE[size];
        memcpy(newBuffer, source, size);

        BYTE* oldBuffer = titleBarImage.exchange(newBuffer, std::memory_order_acq_rel);
        size_t oldSize = titleBarImageSize.exchange(size, std::memory_order_release);

        if (oldBuffer) delete[] oldBuffer;
    }


    bnWindow& window;
    bnWindowTitlebarConfig& config;
    friend bnWindow;

    struct HoverFadeState {
        float alpha = 0.0f;   // 0.0 (no hover) .. 1.0 (fully hovered)
        bool hovered = false;
        float rippleAlpha;       // 0.0 .. 1.0
        float rippleRadius;      // expands with time
        SkPoint hoverPos;        // cursor position
        SkPoint ripplePos;       // press position
    };

    static SkColor BlendColor(COLORREF baseColor, float alpha);
    static WindowRect win32_fake_shadow_rect(HWND handle);
    static void set_menu_item_state(HMENU menu, MENUITEMINFO* menuItemInfo, UINT item, bool enabled);

    void UpdateFade(HoverFadeState& state, const float speed = 5);
    struct CustomTitleBarButtonRects {
        WindowRect close;
        WindowRect maximize;
        WindowRect minimize;

        WindowRect getFirstAvailable() {
            if (minimize.right != 0 && minimize.left != 0) return minimize;
            if (maximize.right != 0 && maximize.left != 0) return maximize;
            if (close.right != 0 && close.left != 0) return close;

            return WindowRect();
        }

        int totalAvailable(){
            int value = 0;
            if (minimize.right != 0 && minimize.left != 0) value++;
            if (maximize.right != 0 && maximize.left != 0) value++;
            if (close.right != 0 && close.left != 0) value++;

            return value;
        }
    } ;



    static int win32_dpi_scale(int value, UINT dpi);
    static void win32_center_rect_in_rect(WindowRect* to_center, const WindowRect* outer_rect);
    std::atomic_bool mouseOnBar = false;
    bool changed = false;
    bool hasChanged();
    CustomTitleBarButtonRects win32_get_title_bar_button_rects(const WindowRect* title_bar_rect);
    bool win32_window_is_maximized();
    WindowRect win32_titlebar_rect(bool noHideOffset = false);
    void UpdateHidePos();
    void updateLPMMI(LPMINMAXINFO lpMMI);
    void RenderFrame(bnGraphics* graphics, GraphicsAdvanced* advanced, GraphicAdvancedCommandList* list,
    ResourceHandle<IInputLayout>* inputLayout,
    ResourceHandle<ISamplerState>* samplerState,
    ResourceHandle<IBlendState>* blending,
    ResourceHandle<IRasterizerState>* rast,
    ResourceHandle<IDepthStencilState>* depth,
    ResourceHandle<ICommandPool>* copyTexturePool,
    ResourceHandle<IShader>* vertexShader,
    ResourceHandle<IShader>* pixelShader,
    ResourceHandle<IViewPort>* viewport, ResourceHandle<IDescriptorPool>* dPool,
        ResourceHandle<IDescriptorSetLayout>* dSetLayout);


    void ReleaseRenderVars(IGraphicsDevice* graphics);

    BITMAP titleBarBit;
    SkFont titleFont;
    bnFontMgr mgr;
    bool flickerEffect = false;
private:
    HoverFadeState minimizeFade = {};
    HoverFadeState maximizeFade = {};
    HoverFadeState closeFade = {};
    CustomTitleBarPressedButton title_bar_pressed_button = CustomTitleBarPressedButton_None;
    CustomTitleBarHoveredButton title_bar_hovered_button = CustomTitleBarHoveredButton_None;

    std::atomic<u8*> titleBarImage;
    std::atomic<size_t> titleBarImageSize;
    LOGFONT logical_font;
    HFONT old_font = NULL;
    HFONT theme_font = NULL;
    HDC memDC;
    HDC hdc;
  
    float hideOffset = 0.0f;
    std::chrono::steady_clock::time_point lastFocusTime;
    bool isHidden = true;
    float animProgress = 0.0f; // 0..1 progress toward target
    float animDuration = 0.3f; // 300ms animation
    std::chrono::steady_clock::time_point cursorEnterTime;
    std::chrono::steady_clock::time_point cursorLeaveTime;
    bool cursorOnTop = false;
    Data* cachedLogoSource = nullptr; // pointer to currently cached source
    u8* lastLogoPtr;
    sk_sp<SkImage> cachedLogo = nullptr;
    LPMINMAXINFO llpMMI = nullptr;
    std::wstring title;
    int framesAfterFrozen = 5; // When initalized, the application is not frozen.
    // Render Variables
    ResourceHandle<IBuffer> stagingBuffer;
    ResourceHandle<ITexture> titleBarTexture;
    ResourceHandle<IBuffer> titleBarVertice;
    ResourceHandle<IPipeline>* pipeline = nullptr;
    ResourceHandle<IDescriptorSet>* set = nullptr;
    ResourceHandle<void> data;
};

