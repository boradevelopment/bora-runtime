#pragma once
#include "nGraphics/ImmediateGraphicsAbstract.h"
#include "nGraphics/GraphicsUtilities.h"
#include <set>

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
    friend class bnGraphicsOGL;
};


class TextureOGL : public ITexture {
public:
    TextureOGL() : textureID(0) {}

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<u32>(textureID));
    }

    void Release();

    u32 Get() { return textureID; }

private:
    friend class bnGraphicsOGL;
    u32 textureID;
    GLTextureFormat format;
};

class ShaderOGL : public IShader {
public:
    ShaderOGL() : shaderID(0) {}
  //  ~ShaderOGL() { Release(); }

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(shaderID));
    }

    void Release();

    uintptr_t Get() const { return shaderID; }

private:
    friend class bnGraphicsOGL;
    uintptr_t shaderID;
};

class BufferOGL : public IBuffer {
public:
    BufferOGL() : bufferID(0) {}
   // ~BufferOGL() { Release(); }

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(bufferID));
    }

    void Release() override {
        if (bufferID != 0) {
            glDeleteBuffers(1, &bufferID);
            bufferID = 0;
        }
        delete this;
    }

    GLuint Get() const { return bufferID; }
    bool dynamic;
    u8 stride;
private:
    friend class bnGraphicsOGL;
    GLuint bufferID;
    sVec<u8> dataBuffer;
};

class InputLayoutOGL : public IInputLayout {
public:
    InputLayoutOGL() : vao(0) {}
    //~InputLayoutOGL() { Release(); }

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(vao));
    }

    void Release() override {
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        delete this;
    }

    GLuint Get() const { return vao; }

private:
    friend class bnGraphicsOGL;
    GLuint vao;
};

class SamplerOGL : public ISamplerState {
public:
    SamplerOGL() : samplerID(0) {}
//    ~SamplerOGL() { Release(); }

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(samplerID));
    }

    void Release() override {
        if (samplerID != 0) {
            glDeleteSamplers(1, &samplerID);
            samplerID = 0;
        }
        delete this;
    }

    GLuint Get() const { return samplerID; }

private:
    friend class bnGraphicsOGL;
    GLuint samplerID;
    u8 slot;
};

class ViewPortOGL : public IViewPort {
public:
    ViewPortOGL(const ViewPortDesc& desc) {
        if (desc.viewport) {
            vp = *desc.viewport;
        }
    }

    void* GetNativeHandle() override {
        // No native GPU object for viewport, return pointer to stored struct
        return &vp;
    }

    void Release() override {
        // Nothing to delete, just clear data
        // Optionally: zero it
        vp = {};
    }

    const ViewPort& Get() const { return vp; }

private:
    friend class bnGraphicsOGL;
    ViewPort vp{};
};

class RasterizerOGL : public IRasterizerState {
public:
    RasterizerOGL(const RasterizerDesc& d) : desc(d) {}
    //~RasterizerOGL() { Release(); }

    void* GetNativeHandle() override {
        return nullptr; // No real OpenGL object
    }

    void Release() override {
        // Nothing to delete
        delete this;
    }

    const RasterizerDesc& GetDesc() const { return desc; }

private:
    friend class bnGraphicsOGL;
    RasterizerDesc desc;
};

class DepthStencilStateOGL : public IDepthStencilState {
public:
    DepthStencilStateOGL(const DepthStencilDesc& d) : desc(d) {}
    //~DepthStencilStateOGL() { Release(); }

    void* GetNativeHandle() override {
        return nullptr; // no native GPU object
    }

    void Release() override {
        // Nothing to delete
        delete this;
    }

    const DepthStencilDesc& GetDesc() const { return desc; }

private:
    friend class bnGraphicsOGL;
    DepthStencilDesc desc;
};

class BlendStateOGL : public IBlendState {
public:
    BlendStateOGL(const BlendStateDesc& d) : desc(d) {}
    //~BlendStateOGL() { Release(); }

    void* GetNativeHandle() override { return nullptr; } // no GPU object
    void Release() override {
        delete this;
    }

    const BlendStateDesc& GetDesc() const { return desc; }

private:
    friend class bnGraphicsOGL;
    BlendStateDesc desc;
};

class DepthStencilOGL : public IDepthStencil {
public:
    DepthStencilOGL(ITexture* tex) : texture(tex) {}
   // ~DepthStencilOGL() { Release(); }

    void* GetNativeHandle() override {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<TextureOGL*>(texture)->Get()));  // this sucks.
    }

    void Release() override {
        // We don't own the texture, so nothing to delete
        texture = nullptr;
        delete this;
    }

    ITexture* GetTexture() const { return texture; }

private:
    friend class bnGraphicsOGL;
    ITexture* texture = nullptr;
};


class RenderTargetOGL : public IRenderTarget {
public:
    RenderTargetOGL(const RenderTargetDesc& d) : desc(d) {}
   // ~RenderTargetOGL() { }

    void* GetNativeHandle() override { return (void*)fbo; }

    void Release() override {
        colorAttachments.clear();
        colorAttachments.shrink_to_fit();
        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
        delete this;
    }

    GLuint GetFBO() const { return fbo; }

    const RenderTargetDesc& GetDesc() const { return desc; }

private:
    friend class bnGraphicsOGL;
    RenderTargetDesc desc;
    GLuint fbo = 0;                 // OpenGL framebuffer
    std::vector<GLuint> colorAttachments; // GL texture IDs
    GLuint depthAttachment = 0;     // GL texture ID
};


class bnGraphicsOGL : public IGraphicsDeviceImmediate
{
public:
    using ShaderSet = std::set<GLuint>;
    bnGraphicsOGL(SysHandle& handle, IGraphicsDeviceConfig& config);

    const char* GetAPIName() const {
        return "OGL";
    }

    uint32_t GetAPIVersion() const {
        return 110;
    }

    bool IsFeatureSupported(const std::string& feature) const {
        if (feature == "SPIRV") {
            return IsGLExtensionSupported("GL_ARB_gl_spirv");
        }
        return false;
    }

    bool Init() override;
    void BeginFrame() override;
    void EndFrame() override;
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
    void BindTexture(ITexture* texture, uint slot = 0) override;
    void BindInputLayout(IInputLayout* layout) override;
    void BindSamplerState(ISamplerState* samplerState, uint slot = 0) override;
    void BindViewPort(IViewPort* viewPort) override;
    void BindRasterizerState(IRasterizerState*) override;
    void BindDepthStencilState(IDepthStencilState*, uint stencilRef = 0) override;
    void BindBlendState(IBlendState*, const float blendFactor[4], uint sampleMask = 0xFFFFFFFF) override;
    void BindRenderTarget(IRenderTarget*, IDepthStencil* = nullptr) override;
    void ClearRenderTarget(IRenderTarget* target, const float color[4]) override;
    void ClearDepthStencil(IDepthStencil* target, float depth, uint stencil) override;
    void DispatchCompute(uint x, uint y, uint z) override;

    void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override;
    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override;
    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override;

    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;

    IDeviceContext* getContext() override { return &context; };
    IDevice* getDevice() override { return &device; };

    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF) override;

    long width, height;
private:
   /* void UpdateViewPorts();*/
    DeviceOGL device;
    DeviceContextOGL context;
    SysHandle& windowHandle;
    IGraphicsDeviceConfig& config;
    InputLayoutOGL* cLayoutDraw;
    sMap<ShaderSet, GLuint> shaderProgramMap;
    sMap<GLuint, GLuint> cShPrograms;         // bufferID -> programID
    sVec<ShaderOGL*> cShaders;
    BlendStateOGL* currentBlendState = nullptr;
    RenderTargetOGL* swpchTarget = nullptr;
    ITexture* msaaSWPCHTarget;
    ITexture* depthTexture;
    IDepthStencil* depthStencil = nullptr;
    ITexture* currentSwapChainImage;
    std::vector<BufferOGL*> mappedPointers;
    std::map<BufferOGL*, void**> repeatMappedAddress;
    std::map<BufferOGL*, void**> currentRepeatMappedAddress;
    GLuint mBlitReadFbo = 0;
    GLuint mBlitDrawFbo = 0;
};