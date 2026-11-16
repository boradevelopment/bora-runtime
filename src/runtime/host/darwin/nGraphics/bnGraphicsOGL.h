#pragma once
#ifdef BORA_HAS_OPENGL
#include "nGraphics/GraphicsUtilities.h"
#include "nGraphics/OGL/bnGraphicsOGLCommon.h"

class DeviceOGL : public IDevice {
public:
    void* GetNativeHandle() override {
        return mContext; // return HDC as void*
    }

    void Release() override;
private:
    void SetHandle(void* handle, void* context);
    void* mView = nullptr;    // Replacement for HWND
    void* mContext = nullptr; // Replacement for HDC
    
    friend class bnGraphicsOGL;
};


class DeviceContextOGL : public IDeviceContext {
    void* GetNativeHandle() override {
        return renderContext; // return HDC as void*
    }


    void Release();

    void*   Get()   const { return renderContext; }

private:
    void* renderContext = nullptr; // OpenGL rendering context
    friend class bnGraphicsOGL;};


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