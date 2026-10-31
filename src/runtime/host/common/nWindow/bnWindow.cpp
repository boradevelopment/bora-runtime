
#include "bnWindow.h"
#include "nGraphics/bnGraphics.h"
#include "Utils.h"
#include "bnWindowTitlebar.h"
#include "windowUtilities.h"

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
    windowUtilities::SendMessage(handle, WindowMessages::MSG_CLOSE, 0, 0);
    while (!canBeDeleted) {
        PollMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void bnWindow::switchGraphics(GraphicsChoice choice)
{
    while (rendering) {

    }
    preDestroyFunctions();
    pauseRender = true;
    WindowRect clientRect = windowUtilities::getWindowRect(handle);
    int width = clientRect.right;
    int height = clientRect.bottom;
    auto oldChoice = this->choice;
    graphics->commands.clear();
    shutdown(false);
    if (RequiresNewHandle(this->choice)) {
        dontAffectAnything = true;
        windowUtilities::SendMessage(handle, WindowMessages::MSG_CLOSE, 0, 0);
        windowUtilities::ReceiveMessages(handle);
        // PostMessage(handle, WM_CLOSE, 0, 0);
        // MSG message;
        //
        // while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        //     TranslateMessage(&message);
        //     DispatchMessageW(&message);
        // }
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

    windowUtilities::hideWindow(handle);
    init();
    windowUtilities::resizeWindow(handle, width, height);
    windowUtilities::showWindow(handle);
    // SendMessage(handle, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height));
    hasRecentlySwitched = true;
    pauseRender = false;
}

