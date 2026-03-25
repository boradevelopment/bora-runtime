class bnWindow;
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

@interface DarwinView : NSView {
    bnWindow* _owner;
    NSTrackingArea* _trackingArea;
}

- (instancetype)initWithFrame:(NSRect)frame owner:(bnWindow*)owner;
- (void)updateTrackingAreas;
@end