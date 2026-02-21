
#include "bnWindow.h"
#include "nGraphics/bnGraphics.h"
#include "Utils.h"
#include "bnWindowTitlebar.h"
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "Comctl32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Propsys.lib")


bnWindow::~bnWindow()
{
    if (audioThread.joinable()) audioThread.join();
    handles.erase(std::find(handles.begin(), handles.end(), handle));
    bnWindow::preDestroyFunctions();
    delete rootAudioDevice;
    delete graphics;
}

void bnWindow::close() {
    running = false;
    PostMessage(handle, WM_CLOSE, 0, 0);
    while (!canBeDeleted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}


void bnWindow::switchGraphics(GraphicsChoice choice)
{
    while (rendering) {

    }
    preDestroyFunctions();
    pauseRender = true;
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;
    auto oldChoice = this->choice;
    graphics->commands.clear();
   
    shutdown(false);
    // do a ms check
    if (RequiresNewHandle(this->choice)) {
        dontAffectAnything = true;
        PostMessage(handle, WM_CLOSE, 0, 0);
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }       
    }
    if (choice != NONE) {
        this->choice = choice;
    }   

    if (rootDevice) {
        delete rootDevice;
        rootDevice = nullptr;
    }
    firstFrame = true;
    pauseRender = true;
    if (RequiresNewHandle(oldChoice)) {
        #ifdef WIN32
        win32Window::createWindow(derivedObject, configuration);
        #endif
    }


    init();
    ShowWindow(handle, SW_SHOW);
    SendMessage(handle, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height));
    hasRecentlySwitched = true;
    pauseRender = false;
}

