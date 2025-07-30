// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

/* ? [Common or PDS (Platform Dependent Source)]

 * FileName: window.h
 * Title: ?
 * Author: ?
 * Purpose: ?

 * Compatibility: ?

 * Updates - ?
 * Known issues - ?
 */

#ifndef BORA_WINDOW_H
#define BORA_WINDOW_H


#pragma once
#include <string>

struct WindowProperties {
    sString test;
};


class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void poll();         // Run one iteration of the message loop
    bool shouldClose();  // Check if the window should close

    void* getNativeHandle(); // Optional: HWND, NSWindow*, etc.

private:
    struct Impl; // Platform-specific
    Impl* impl;
};


#endif //BORA_WINDOW_H
