#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include "darwinWindow.h"
#import "darwinWindowBridge.h"

template <typename T>
SysHandle darwinWindow::createWindow(T* object, bnWindowConstructorStruct* config, bool isChild) {
    // 1. Setup the frame (dimensions only, since we will center it)
    NSRect frame = NSMakeRect(0, 0, config->width, config->height);

    NSUInteger styleMask = NSWindowStyleMaskTitled |
                           NSWindowStyleMaskClosable |
                           NSWindowStyleMaskResizable |
                           NSWindowStyleMaskMiniaturizable;

    // 2. Convert std::wstring to NSString
    // macOS wchar_t is 32-bit (UTF-32)
    NSString* title = [[NSString alloc] initWithBytes:config->title.data()
                                               length:config->title.size() * sizeof(wchar_t)
                                             encoding:NSUTF32LittleEndianStringEncoding];

    // 3. Initialize the NSWindow
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:styleMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];

    DarwinView* view = [[DarwinView alloc] initWithFrame:frame owner:object];

    // 3. Attach View to Window
    [window setContentView:view];
    [window makeKeyAndOrderFront:nil];

    [window setTitle:title];
    [window center]; // This handles the "center for now" requirement
    [window makeKeyAndOrderFront:nil];

    // Ensure the application is active and visible in the Dock
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    return (void*)window;
}
void darwinWindow::pollEvents(){
    @autoreleasepool {
        NSEvent* event = nil;
        // Peek at the next event in the queue
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]))
        {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }
    }
}
