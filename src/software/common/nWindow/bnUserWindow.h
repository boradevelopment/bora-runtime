// The window that is given to the user to use, in the SDK, its a bnWindow but in the runtime, alot of GUI elements are bnWindows 
// so we derive from it to make things easier to manage
#pragma once
#include <set>

#include "bnWindow.h"
#include "nGUI/skia/win32/SkiaD3D12Renderer.h"

struct bnWindowTitlebarConfig;

class bnUserWindow;

typedef void (*UpdateProc)(bnUserWindow* wnd, void* userObject, unsigned int msg, uintptr_t wParam, intptr_t lParam);

enum WindowUpdateMessages {
    ON_INIT,
    ON_UPDATE,
    ON_RESIZE,
    ON_RELEASE,
    ON_CHANGE_GRAPHICS,
    ON_INPUT_CHANGED
};

struct InputState {
    // Keyboard
    std::set<int> keysPressed;
    std::set<int> keysReleased;

    // Mouse
    std::set<int> mousePressed;
    std::set<int> mouseReleased;
    Offset2D mouseOffset;
};

struct bnUserWindowConfig : bnWindowConstructorStruct {
    UpdateProc update;
    void* userObject = nullptr;
};

class bnGraphics;

class bnUserWindow : public bnWindow
{
public:
    bnUserWindow(bnUserWindowConfig data) : configUser(std::move(data)), bnWindow(&configUser) {
   
        if (!configuration->titleBarConfig) {
            configuration->titleBarConfig = new bnWindowTitlebarConfig();
        }

        auto titleBarCOnfig = configuration->titleBarConfig;

        titleBar = std::make_unique<bnWindowTitlebar>(*this, *titleBarCOnfig);
        choice = D3D12;
        //if (configUser.width == -1) {
        //    configUser.width = GetSystemMetrics(SM_CXSCREEN); // full screen width
        //    configUser.height = GetSystemMetrics(SM_CYSCREEN); // full screen height
        //}

        create(this);
        bool maximize = false;
        if (configuration->width < 1 && configuration->height < 1) {
            maximize = true;
            configuration->width = GetSystemMetrics(SM_CXSCREEN); // full screen width
            configuration->height = GetSystemMetrics(SM_CYSCREEN); // full screen height
        }

        if (maximize) {
            ShowWindow(handle, SW_MAXIMIZE);
        }
        UpdateWindow(handle);
        init();
        ShowWindow(handle, SW_SHOW);

    }
    void run();
    //void close();
    void init() override;
    void preDestroyFunctions() override;
public: // Functions & Structs
    WindowCallbackCodes windowCallback(WindowEvent* event) override;
public: // Dynamic Functions
    void shutdown(bool perm = true);
    bool InitGraphicsSystem();
    void ResizeSwapChain(UINT width, UINT height);
    void UpdateViewports(UINT width, UINT height);
    void RenderFrame();
    //void FinishUpRenderThread();
    void UpdateTitleBar();
    //void UpdateUserThread();
    void UpdateInputThread();
public:
    //bool updateAll = false;
    bool timeToRender = false;
    std::atomic<bool> predraw = false;
    std::atomic<bool> plsnomoretoday = false;
    std::atomic<bool> scrready = false;
    //std::mutex cachedTexMutex;
    ResourceHandle<ITexture> cachedUserRender;
    bool firstFrame = true;
    bool titleUpdated = false;
    std::mutex boraThreadedLock;
    ViewPort vpTitlebar;
    ViewPort vpMain;
    ResourceHandle<IViewPort> iTitleBar;
    ResourceHandle<IViewPort> iMain;
    bool frameSkipped = false;
    //std::unique_ptr<bnWindowTitlebar> titleBar = nullptr;
    //IDescriptorPool* dPool;
    //IDescriptorSetLayout* dSetLayout;
    //IDescriptorPool* dPoolUT;
    //IDescriptorSetLayout* dSetLayoutUT;
    //ICommandPool* copyTexturePool;
    //ResourceHandle<IShader> vertexShader;
    //IShader* pixelShader;
    bool messageBoxActive = false;
    ResourceHandle<IBuffer> stagingBuffer;
    ResourceHandle<ICommandPool> copyTexturePool;
    ResourceHandle<IShader> vertexShader;
    ResourceHandle<IShader> pixelShader;
    ResourceHandle<IInputLayout> inputLayout;
    ResourceHandle<ISamplerState> samplerState;
    ResourceHandle<IBlendState> blending;
    ResourceHandle<IRasterizerState> rast;
    ResourceHandle<IDepthStencilState> depth;
    ResourceHandle<IDescriptorPool> dPool;
    ResourceHandle<IDescriptorSetLayout> dSetLayout;
    ResourceHandle<IDescriptorPool> dPoolUT;
    ResourceHandle<IDescriptorSetLayout> dSetLayoutUT;
    // Unresposive Backup Protocal Shader - Which just renders a screenshot.
    ResourceHandle<IShader> utVertexShader;
    ResourceHandle<IShader> utPixelShader;
    ResourceHandle<IBuffer> utCBO;
    ResourceHandle<IBuffer> utVB;
    ResourceHandle<IPipeline>* pipeline = nullptr;
    ResourceHandle<IDescriptorSet>* set = nullptr;
   /* IBlendState* fadeBlend;
    IRasterizerState* rast;
    IDepthStencilState* depth;*/
     std::map<int, bool> firstFrames;
    std::atomic_bool justRendered = false;
    BITMAP memBitmap;
    //  std::mutex cachedTexMutex;
    ITexture* pendingCachedRender = nullptr;
    ResourceHandle<ITexture> swpText;
    std::atomic<bool> renderReady{ false };
 /*   int updateIt = -1;
    std::thread updateThread;
    bool updateInProgress = false;
    std::atomic<bool> frozenProcess{ false };
    std::chrono::steady_clock::time_point updateStart;
    int updateItNum;*/
    // Thread management
    std::thread updateThread;              // The persistent worker thread
    std::mutex updateMutex;                // Synchronization for shared state
    std::condition_variable updateCv;      // Signals the worker that there�s work to do

    // Flags and state
    bool stopThread = false;               // Signals the worker to exit cleanly
    bool workAvailable = true;            // True when a new frame update is requested
    bool updateInProgress = false;         // (Optional) Track whether the worker is running
    bool pendingIsRenderable = false;      // Passed to worker to know if window is visible

    // Optional: tracking frame iteration / timing
    u64 updateIt = 0;                 // Your existing counter
    u64 updateItNum = 0;              // Used for tracking inside worker
    std::chrono::steady_clock::time_point updateStart; // Timestamp when update begins
   // bool dontAffectAnything = false;
    sVec<std::thread> inputThreads;

    void StartUserThread();
    void StopThreads();
    void UserThreadLoop();
public:
    bool invertAnimation = false;
    bnUserWindowConfig configUser;
    InputState currentInputState;
    bnGraphics* userGraphics = nullptr;
    std::thread frozenMessageBoxThread;
    float getHeight() const {
        return vpMain.height;
    }
    float getWidth() const {
        return vpMain.width;
    }
private:
    friend class bnWindowTitlebar;
    friend class bnGraphics;
public:
    SkiaD3D12Renderer* guiRenderer;
};

