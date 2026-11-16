#ifdef BORA_HAS_OPENGL
#include "bnGraphicsOGL.h"
#include <iostream>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <AppKit/AppKit.h>

bnGraphicsOGL::bnGraphicsOGL(SysHandle& handle, IGraphicsDeviceConfig& config) : bnGraphicsOGLCommon(handle, config) {

}

bool bnGraphicsOGL::Init()
{
    NSView* view = (NSView*)windowHandle;

    // Step 2: Define Pixel Format Attributes
    NSOpenGLPixelFormatAttribute attribs[] = {
            NSOpenGLPFAColorSize, 32,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFAStencilSize, 8,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAccelerated,
            //  Core Profile 4.1 (highest supported on Mac)
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
            0
    };

    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (!pixelFormat) {
        std::cerr << "Failed to create NSOpenGLPixelFormat\n";
        return false;
    }

    // Step 3: Create and Initialize the Context (Replaces wglCreateContext)
    NSOpenGLContext* contextPtr = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                                             shareContext:nil];
    [pixelFormat release]; // We are done with the format object

    if (!contextPtr) {
        std::cerr << "Failed to create NSOpenGLContext\n";
        return false;
    }

    // Connect the context to the NSView
    [contextPtr setView:view];
    [contextPtr makeCurrentContext];

    // Store in your DeviceOGL wrapper (Manual Management)
    device->SetHandle(view, (void*)contextPtr);

    // Step 4: OpenGL Setup (No GLEW needed on Mac)
    std::cout << "OpenGL Context initialized. Version: "
              << glGetString(GL_VERSION) << "\n";

    // This is for image copying as the one I was using was NOT supported :(
    //glGenFramebuffers(1, &mBlitReadFbo);
    //glGenFramebuffers(1, &mBlitDrawFbo);

    // Debugging doesn't exist on Apple instances of OGL!!!
    return true;
}


void bnGraphicsOGL::Shutdown()
{
    bnGraphicsOGL::Shutdown();
    if (context->renderContext) {
        context->Release();
    }

    if (device->GetNativeHandle()) {
        device->Release();
    }
}

void bnGraphicsOGL::Present()
{
    glFinish();
    NSOpenGLContext* ctx = (NSOpenGLContext*)device->GetNativeHandle();
    GLint sync = config.vsync ? 1 : 0;
    [ctx setValues:&sync forParameter:NSOpenGLCPSwapInterval];

    bnGraphicsOGLCommon::Present();
    [ctx flushBuffer];
}

void DeviceOGL::Release() {
    if (mContext) {
        // macOS: Clear the drawable before releasing
        [(NSOpenGLContext*)mContext clearDrawable];
        [(NSOpenGLContext*)mContext release];
        mContext = nullptr;
    }
    mView = nullptr;
}

void DeviceOGL::SetHandle(void *handle, void *context) {
    mView = handle;    // This is the NSView*
    mContext = context; // This is the NSOpenGLContext*

    if (mContext) {
        [(NSOpenGLContext*)mContext retain];
        // Link the context to the view
        [(NSOpenGLContext*)mContext setView:(NSView*)mView];
    }
}

#endif