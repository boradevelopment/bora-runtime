#pragma once
#include "windowUtilities.h"
#include "nGUI/GUIRoot.h"

#ifdef WIN32
#include "nWindow/win32Window.h"
#include "avrt.h"
#pragma comment(lib, "avrt.lib")
#endif
#ifdef __linux__
#include "nWindow/linuxWindow.h"
#endif
#ifdef __APPLE__
#include "nWindow/darwinWindow.h"
#endif

#include <thread>
#include <future>
#include "bnWindowAbstracts.h"
#include "nAudio/bnAudio.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include "nGraphics/GraphicsAbstractions.h"
#include "nGraphics/ImmediateGraphicsAbstract.h"
#include "bnWindowTitlebar.h"

class bnWindowTitlebar;
class bnGraphics;

// todo: clean the fuck out of this, this is ass.
class bnWindow
{
public:
	bnWindow(bnWindowConstructorStruct* data) : configuration(data)
    {
	    guiManager = std::make_unique<GUIRoot>(this);
    }

    virtual ~bnWindow();
    template <typename T>
    void create(T* object, bool isChild = false) {
        static_assert(std::is_base_of_v<bnWindow, T>,
            "T must derive from bnWindow");

        derivedObject = object;
        audioThread = std::thread([&]() {
          while (running) {
              if (rootAudioDevice && !rootAudioDevice->Paused()) {
                  rootAudioDevice->UpdateFrame();
              }
          }
        });
#ifdef WIN32
       handle = win32Window::createWindow(object, configuration, isChild);
        audioThread = std::thread([&]() {
            DWORD taskIndex = 0;
             HANDLE mmcssHandle = AvSetMmThreadCharacteristics(L"Pro Audio", &taskIndex);
        while (running) {
            if (rootAudioDevice && !rootAudioDevice->Paused()) {
                rootAudioDevice->UpdateFrame();
            }
        }
            if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
});
        SetThreadPriority(audioThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
#elif defined(__linux__)
        handle = linuxWindow::createWindow(object, configuration, isChild);
        // SetThreadPriority(audioThread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
#elif  defined(__APPLE__)
        handle = darwinWindow::createWindow(object, configuration, isChild);
        // todo: audio engine!
#endif
        gDeltaTimes[handle] = 0.0f;
        running = true;
    }
    virtual void init() = 0;
    virtual void shutdown(bool perm = true) {};
    virtual void run() {};
    virtual void preDestroyFunctions() {};
    virtual long getWindowHeight() = 0;
    virtual long getWindowWidth() = 0;
    void close();
    void switchGraphics(GraphicsChoice choice = NONE);
public: // Functions & Structs
    virtual WindowCallbackCodes windowCallback(WindowEvent* event) = 0;
    virtual bool isPointInNonClientArea(WindowPoint cursor) = 0;
    void PollMessages() const
    {
        windowUtilities::ReceiveMessages(handle);
// #ifdef WIN32
//         MSG message;
//         while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
//             TranslateMessage(&message);
//             DispatchMessageW(&message);
//         }
// #endif
    }
    static i64 GetDefaultProcedure(u8 message, u64 w_param, u64 l_param) {
#ifdef WIN32
        return DefWindowProc(handle, message, w_param, l_param);
#endif

        return 0;
    }
public: // DYnamic Variables
    bool firstFrame = true;
    bool frozenProcess = false;
    bool updateAll = false;
    std::atomic<bool> running = false;
    std::atomic<bool> canBeDeleted = false;
    std::atomic<bool> tBarChanged = false;
    bool pauseRender = false;
    bool dontAffectAnything = false;
    bool usingExpl{};
    GraphicsChoice choice = GraphicsChoice::NONE;
    SysHandle handle{};
    bool enableTitlebar = true;
    std::unique_ptr<bnWindowTitlebar> titleBar = nullptr;
    IAudioDevice* rootAudioDevice = nullptr;
    bnAudio* audio = nullptr;
    std::unique_ptr<GUIRoot> guiManager = nullptr;
    bnFontMgr fontManager;
    IAudioDeviceConfig audioConfig;
    IGraphicsDevice* rootDevice = nullptr;
    IGraphicsDeviceConfig gEConfig;
    IGraphicsDeviceExplicit* gEngineExpl = nullptr;
    IGraphicsDeviceImmediate* gEngineImm = nullptr;
    bnGraphics* graphics = nullptr;
private:
    std::thread audioThread;
public:
    std::atomic<bool> rendering;
    u8 aliasLevel = 0;
    u8 aliasQuality = 0;
    std::vector<u8> supportedMsaaLevels;
    bool antiAliasing = false;
    bnWindowConstructorStruct* configuration = nullptr;
    void* hIcon = nullptr; // temp, use SysIcon type
    bool hasRecentlySwitched = false;
    private:
    void* derivedObject{};
    friend class bnWindowTitlebar;
    friend class bnGraphics;
};

