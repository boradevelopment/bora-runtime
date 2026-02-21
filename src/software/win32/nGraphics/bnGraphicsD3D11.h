#pragma once
#include <d3d11_1.h>

#include "nGraphics/ImmediateGraphicsAbstract.h"
#ifdef WIN32
#include "nGraphics/GraphicsUtilities.h"
#include "d3d11.h"
#include <wrl/client.h>
#include <comdef.h>
#endif

#ifdef WIN32
class TextureD3D11 : public ITexture {
public:
    TextureD3D11() {}
    TextureD3D11(const TextureDesc& desc) : mDesc(desc) {}
    void* GetNativeHandle() override { 
        //if (mTexture1D) {
        //    return mTexture1D;
        //}
        //else if (mTexture2D) {
        //    return mTexture1D;
        //}
        //else if (mTexture3D) {
        //    return mTexture1D;
        //}
        return mTexture;
    }
    void Release() override {
        if (owner) {
            //if (mTexture1D) mTexture1D->Release();
            //if (mTexture2D) mTexture2D->Release();
            //if (mTexture3D) mTexture2D->Release();
            if (mTexture) mTexture->Release();
            if (mSrv) mSrv->Release();
        }

        delete this;
    }
    ID3D11Resource* mTexture = nullptr;
    ID3D11Texture1D* mTexture1D = nullptr;
    ID3D11Texture2D* mTexture2D = nullptr;
    ID3D11Texture3D* mTexture3D = nullptr;

    ID3D11ShaderResourceView* mSrv = nullptr;
    TextureDesc mDesc;
};

class ShaderD3D11 : public IShader {
public:
    void* GetNativeHandle() override {
        return mShader;
    }
    ID3D11DeviceChild* mShader = nullptr; 
    void Release() override {
        if (mShader) mShader->Release();
        delete this;
    }
};


class InputLayoutD3D11 : public IInputLayout {
public:
    void* GetNativeHandle() override {
        return mLayout;
    }
    ID3D11InputLayout* mLayout = nullptr;
    void Release() override {
        if (mLayout) mLayout->Release();
        delete this;
    }
};

class SamplerStateD3D11 : public ISamplerState {
public:
    void* GetNativeHandle() override {
        return mSamplerState;
    }
    ID3D11SamplerState* mSamplerState = nullptr;
    void Release() override {
        if (mSamplerState) mSamplerState->Release();
        delete this;
    }
};

// I don't know why i made states like this when I could put the state in the constructor.
// All of this will be using the constructor except the ones on the top, i'll update it when I'm free

class RasterizerStateD3D11 : public IRasterizerState {
public:
    RasterizerStateD3D11(ID3D11RasterizerState* state) : mRasterizerState(state) {}

    void* GetNativeHandle() override {
        return mRasterizerState;
    }
   
    void Release() override {
        if (mRasterizerState) mRasterizerState->Release();
        delete this;
    }
private:
    ID3D11RasterizerState* mRasterizerState = nullptr;
};


class DepthStencilStateD3D11 : public IDepthStencilState {
public:
    DepthStencilStateD3D11(ID3D11DepthStencilState* state) : mDepthStencilState(state) {}

    void* GetNativeHandle() override {
        return mDepthStencilState;
    }

    void Release() override {
        if (mDepthStencilState) mDepthStencilState->Release();
        delete this;
    }
private:
    ID3D11DepthStencilState* mDepthStencilState = nullptr;
};

class BlendStateD3D11 : public IBlendState {
public:
    BlendStateD3D11(ID3D11BlendState* state) : mBlendState(state) {}

    void* GetNativeHandle() override {
        return mBlendState;
    }

    void Release() override {
        if (mBlendState) mBlendState->Release();
        delete this;
    }
private:
    ID3D11BlendState* mBlendState = nullptr;
};

class RenderTargetD3D11 : public IRenderTarget {
public:
    // Constructor with multiple RTVs and optional DSV
    RenderTargetD3D11(const std::vector<ID3D11RenderTargetView*>& rtvs, ID3D11DepthStencilView* dsv = nullptr)
        : mRenderTargets(rtvs), mDepthStencil(dsv) {
    }

    // Return the first render target as the native handle (for compatibility)
    void* GetNativeHandle() override {
        return mRenderTargets.empty() ? nullptr : mRenderTargets[0];
    }

    // Return all RTVs
    const std::vector<ID3D11RenderTargetView*>& GetRenderTargets() const {
        return mRenderTargets;
    }

    // Return DSV
    ID3D11DepthStencilView* GetDepthStencil() const {
        return mDepthStencil;
    }

    // Release all resources
    void Release() override {
        for (auto rtv : mRenderTargets) {
            if (rtv) rtv->Release();
        }
        mRenderTargets.clear();

        if (mDepthStencil) {
            mDepthStencil->Release();
            mDepthStencil = nullptr;
        }

        delete this;
    }

private:
    std::vector<ID3D11RenderTargetView*> mRenderTargets;
    ID3D11DepthStencilView* mDepthStencil = nullptr;
};


class DepthStencilD3D11 : public IDepthStencil {
public:
    DepthStencilD3D11(ID3D11DepthStencilView* state, ITexture* texture) : mDepthStencil(state), texture(texture) {}

    void* GetNativeHandle() override {
        return mDepthStencil;
    }

    void Release() override {
        if (mDepthStencil) mDepthStencil->Release();
        delete this;
    }
    ITexture* texture = nullptr;
private:
    ID3D11DepthStencilView* mDepthStencil = nullptr;

};

class BufferD3D11 : public IBuffer {
public:
    BufferD3D11()
        : mBuffer(nullptr) {
    }

    BufferD3D11(ID3D11Buffer* buffer, BufferType type, UINT stride, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
        : mBuffer(buffer), mStride(stride), mIndexFormat(format) {
        type = type;
    }

    void Release() override {
        if (mBuffer) {
            mBuffer->Release();
            mBuffer = nullptr;
        }

        // Optional: reset other members if needed
        mStride = 0;
        mOffset = 0;
        mIndexFormat = DXGI_FORMAT_UNKNOWN;
        type = BufferType::Vertex;

        delete this;
    }

    std::vector<uint8_t> dataBuffer;
    UINT mStride = 0;
    ID3D11Buffer* mBuffer;
    DXGI_FORMAT mIndexFormat;
    UINT mOffset = 0;
    BufferDesc desc;


    void* GetNativeHandle() override {
        return mBuffer;
    }

};

struct ViewportData {
    sString id;
    D3D11_VIEWPORT viewport;
};


class ViewPortD3D11 : public IViewPort {
public:
    void* GetNativeHandle() override {
        return &mViewPortData.viewport;
    }
    ViewportData mViewPortData;
    void Release() override {
        //delete this;
    }
};

class DeviceContextD3D11 : public IDeviceContext {
public:
    void* GetNativeHandle() override {
        return deviceC;
    }
    void Release() override {
        if (deviceC) {
            deviceC->Release();
            deviceC = nullptr;
        }
    }

    ID3D11DeviceContext* operator->() {
        return deviceC;
    }
private:
    ID3D11DeviceContext* deviceC;
    friend class bnGraphicsD3D11;
};

class DeviceD3D11 : public IDevice {
public:
    void* GetNativeHandle() override {
        return device;
    }
    void Release() override {
        if (device) device->Release();
    }

    ID3D11Device* operator->() {
        return device;
    }
private:
    ID3D11Device* device;
    friend class bnGraphicsD3D11;
};

#endif

#ifdef WIN32
class bnGraphicsD3D11 : public IGraphicsDeviceImmediate {
public:
    bnGraphicsD3D11(SysHandle& handle, IGraphicsDeviceConfig& config);

    const char* GetAPIName() const {
        return "D3D11";
    }

    uint32_t GetAPIVersion() const {
        return 110;
    }

    bool IsFeatureSupported(const std::string& feature) const {
        return true;
    }

    bool Init();

    void BeginFrame() override;

    void EndFrame() override {
    }

    void Shutdown() override;
    void Present() override;
    void Resize(long width, long height) override;
    
    ITexture* GetSwapchainImage() override;

    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) override;
    IShader* CreateShader(const ShaderDesc& desc) override;
    IBuffer* CreateBuffer(const BufferDesc& desc, const void* data = nullptr) override;
    IInputLayout* CreateInputLayout(const InputLayoutDesc& desc) override;
    ISamplerState* CreateSamplerState(const SamplerStateDesc& desc) override;
    IViewPort* CreateViewPort(const ViewPortDesc& desc) override;
    IRasterizerState* CreateRasterizerState(const RasterizerDesc& desc) override;
    IDepthStencilState* CreateDepthStencilState(const DepthStencilDesc& desc) override;
    IBlendState* CreateBlendState(const BlendStateDesc& desc) override;
    IRenderTarget* CreateRenderTarget(const RenderTargetDesc& desc) override;
    IDepthStencil* CreateDepthStencil(ITexture* texture) override;

    // Add Releases
    void ReleaseShader(IShader**) override;
    void ReleaseBuffer(IBuffer**) override;
    void ReleaseTexture(ITexture**) override;

    void BindShader(IShader* shader) override;
    void BindBuffer(IBuffer* buffer) override;
    void BindTexture(ITexture* texture, u8 slot = 0) override;
    void BindInputLayout(IInputLayout* layout) override;
    void BindSamplerState(ISamplerState* samplerState, u8 slot = 0) override;
    void BindViewPort(IViewPort* viewPort) override;
    void BindRasterizerState(IRasterizerState*) override;
    void BindDepthStencilState(IDepthStencilState*, UINT stencilRef = 0) override;
    void BindBlendState(IBlendState*, const float blendFactor[4], UINT sampleMask = 0xFFFFFFFF) override;
    void BindRenderTarget(IRenderTarget*, IDepthStencil* = nullptr) override;
    void ClearRenderTarget(IRenderTarget* target, const float color[4]) override;
    void ClearDepthStencil(IDepthStencil* target, float depth, UINT8 stencil) override;
    void DispatchCompute(UINT x, UINT y, UINT z) override;

    void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override;
    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override;
    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override;

    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;

    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF) override;
    
    IDeviceContext* getContext() override { return &context; };
    IDevice* getDevice() override { return &device; };
    long width;
    long height;
private:
    void UpdateViewPorts();
    ID3D11Texture2D* swpBackBuffer = nullptr;
    u8 aliasLevel;
    u8 aliasQuality;
    bool enableMSAA = false;
    DeviceD3D11 device;
    DeviceContextD3D11 context;
    ID3D11RenderTargetView* rtv = nullptr;
    IDXGISwapChain* swapChain;
    ID3D11Texture2D* msaaRenderTarget = nullptr;
    ID3D11RenderTargetView* msaaRTV = nullptr;

    D3D11_PRIMITIVE_TOPOLOGY ConvertTopology(PrimitiveType type) {
        switch (type) {
        case PrimitiveType::Triangles: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::Lines: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            // others...
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
    SysHandle& windowHandle;
    IGraphicsDeviceConfig& config;
    std::vector<ViewportData*> viewports;
    std::vector<BufferD3D11*> mappedPointers;
    ID3DUserDefinedAnnotation* mAnnotation;
};

#else
class bnGraphicsD3D11 : public IGraphicsDeviceImmediate {
public:
    bnGraphicsD3D11(SysHandle& handle, bnGraphicsConfigDX11& config);

    const char* GetAPIName() const {
        return "D3D11";
    }

    uint32_t GetAPIVersion() const {
        return 110;
    }

    bool IsFeatureSupported(const std::string& feature) const {
        return false;
    }

    bool Init() { return false; };

    void BeginFrame() override {};

    void EndFrame() override {};

    void Shutdown() override {};
    void Present(bool vsync = true) override {};
    void Resize(long width, long height) override {};


    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) override { return nullptr; };
    IShader* CreateShader(const ShaderDesc& desc) override { return nullptr; };
    IBuffer* CreateBuffer(const BufferDesc& desc, const void* data = nullptr) override { return nullptr; };
    IInputLayout* CreateInputLayout(const InputLayoutDesc& desc) override { return nullptr; };
    ISamplerState* CreateSamplerState(const SamplerStateDesc& desc) override { return nullptr; };
    IViewPort* CreateViewPort(const ViewPortDesc& desc) override { return nullptr; };
    IRasterizerState* CreateRasterizerState(const RasterizerDesc& desc) override { return nullptr; };
    IDepthStencilState* CreateDepthStencilState(const DepthStencilDesc& desc) override { return nullptr; };
    IBlendState* CreateBlendState(const BlendStateDesc& desc) override { return nullptr; };
    IRenderTarget* CreateRenderTarget(ITexture* texture) override { return nullptr; };
    IDepthStencil* CreateDepthStencil(ITexture* texture) override { return nullptr; };

    // Add Releases
    void ReleaseShader(IShader*) override {};
    void ReleaseBuffer(IBuffer*) override {};
    void ReleaseTexture(ITexture*) override {};

    void BindShader(IShader* shader) override {};
    void BindBuffer(IBuffer* buffer) override {};
    void BindTexture(ITexture* texture) override {};
    void BindInputLayout(IInputLayout* layout) override {};
    void BindSamplerState (ISamplerState* samplerState) override {};
    void BindViewPort(IViewPort* viewPort) override {};
    void BindRasterizerState(IRasterizerState*) override {};
    void BindDepthStencilState(IDepthStencilState*, UINT stencilRef = 0) override {};
    void BindBlendState(IBlendState*, const float blendFactor[4], UINT sampleMask = 0xFFFFFFFF) override {};
    void BindRenderTarget(IRenderTarget*, IDepthStencil* = nullptr) override {};
    void ClearRenderTarget(IRenderTarget* target, const float color[4]) override {};
    void ClearDepthStencil(IDepthStencil* target, float depth, UINT8 stencil) override {};
    void DispatchCompute(UINT x, UINT y, UINT z) override {};

    void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override {};
    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override {};
    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override {};

    IDeviceContext* getContext() override { return nullptr; };
    IDevice* getDevice() override { return nullptr; };
};
#endif
