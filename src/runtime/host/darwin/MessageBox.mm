#include "MessageBox.h"
#import <Cocoa/Cocoa.h>

void ShowMessageBox(const std::string& title, const std::string& message, MessageBoxType type) {
//    @autoreleasepool {
          if (!NSApp) {
        [NSApplication sharedApplication];
    }

    NSAlert* alert = [[NSAlert alloc] init];

    switch (type) {
        case MessageBoxType::INFORMATION:
            [alert setAlertStyle:NSAlertStyleInformational]; break;
        case MessageBoxType::WARNING:
            [alert setAlertStyle:NSAlertStyleWarning]; break;
        case MessageBoxType::PROBLEM:
            [alert setAlertStyle:NSAlertStyleCritical]; break;
    }

    [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
    [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
    [alert addButtonWithTitle:@"OK"];

    // Create a temporary invisible window to host the sheet
    NSScreen* screen = [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];
    NSRect windowRect = NSMakeRect(0, 0, 0, 0); // Arbitrary size
    windowRect.origin.x = NSMidX(screenFrame) - windowRect.size.width / 2;
    windowRect.origin.y = NSMidY(screenFrame) - windowRect.size.height / 2;

    NSWindow* window = [[NSWindow alloc] initWithContentRect:windowRect
                                                   styleMask:(NSWindowStyleMaskTitled |
                                                              NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskMiniaturizable |
                                                              NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];

    [window setTitle:@"Message"];
    [window makeKeyAndOrderFront:nil];

    // Center and move to front
    [window center];
    [NSApp activateIgnoringOtherApps:YES];

    [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse returnCode) {
        //scsleep(8);
         [NSApp stop:nil];
    }];

    [NSApp run];

        printf("Confirming if cooked");

    //}
}
