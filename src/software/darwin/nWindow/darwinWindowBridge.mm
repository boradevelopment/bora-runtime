#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include "nWindow/bnWindow.h"
#include "darwinWindowBridge.h"

@implementation DarwinView

- (instancetype)initWithFrame:(NSRect)frame owner:(bnWindow*)owner {
    self = [super initWithFrame:frame];
    if (self) {
        _owner = owner;
        // Important: Metal/QuartzCore layers for rendering
        self.wantsLayer = YES;
    }
    return self;
}

// macOS needs an "Area" defined to track mouse moves
- (void)updateTrackingAreas {
    if (_trackingArea != nil) {
        [self removeTrackingArea:_trackingArea];
    }

    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingMouseMoved | NSTrackingEnabledDuringMouseDrag);
    _trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:options
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

// --- Event Mappings ---

- (void)mouseDown:(NSEvent *)event {
    WindowEvent e;
    e.type = WindowEventType::MouseButtonDown;
    e.button = 0; // Left
    [self fillEventDetails:&e fromEvent:event];
    _owner->windowCallback(&e);
}

- (void)mouseMoved:(NSEvent *)event {
    WindowEvent e;
    e.type = WindowEventType::MouseMove;
    [self fillEventDetails:&e fromEvent:event];
    _owner->windowCallback(&e);
}

- (void)fillEventDetails:(WindowEvent*)e fromEvent:(NSEvent*)event {
    NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
    e->x = location.x;
    e->y = self.frame.size.height - location.y; // Flip Y for engine consistency
    e->width = self.frame.size.width;
    e->height = self.frame.size.height;
    // Map native message pointers if your engine uses them for Win32-style peeking
    e->originalMessage = (uint32_t)[event type];
}

// Enable key events
- (BOOL)acceptsFirstResponder { return YES; }

@end