#pragma once
#ifdef BORA_HAS_OPENGL
#include "nGraphics/GraphicsUtilities.h"
#include <EGL/egl.h>
#include "nGraphics/OGL/bnGraphicsOGLCommon.h"

class DeviceOGL : public IDevice {
public:
    void* GetNativeHandle() override {
        return display; // return HDC as void*
    }

    void Release() override {
        if (display != EGL_NO_DISPLAY) {
            eglTerminate(display);
            display = EGL_NO_DISPLAY;
        }
    }

    EGLDisplay Get() const { return display; }

private:
    void SetHandle(EGLSurface handle, EGLDisplay dpy) {
        surface = handle;
        display = dpy;
    }
    EGLSurface surface = EGL_NO_SURFACE;
    EGLDisplay display = EGL_NO_DISPLAY;
    
    friend class bnGraphicsOGL;
};

class DeviceContextOGL : public IDeviceContext {
public:
    void* GetNativeHandle() override {
        return renderContext; // return EGLContext as void*
    }

    void Release() override {
        if (renderContext != EGL_NO_CONTEXT) {
            if (display != EGL_NO_DISPLAY) {
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
            eglDestroyContext(display, renderContext);
            renderContext = EGL_NO_CONTEXT;
        }
    }

    EGLContext Get() const { return renderContext; }

private:
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext renderContext = EGL_NO_CONTEXT;

    friend class bnGraphicsOGL;
};

/// The full port of OpenGL for Bora Native Graphics.
class bnGraphicsOGL : public bnGraphicsOGLCommon
{
private:
    std::unique_ptr<DeviceOGL> device;
    std::unique_ptr<DeviceContextOGL> context;
public:
    bnGraphicsOGL(SysHandle& handle, IGraphicsDeviceConfig& config);

    /// Initalizes the OpenGL GPU (Currently for linux 4.6 MIN!!!)
    /// @return True if successful, otherwise false.
    bool Init() override;
    /// Shutdowns the OpenGL GPU
    void Shutdown() override;
    /// Presents and swaps the buffer to the surface
    void Present() override;

    IDeviceContext* getContext() override { return context.get(); };
    IDevice* getDevice() override { return device.get(); };
};
#endif