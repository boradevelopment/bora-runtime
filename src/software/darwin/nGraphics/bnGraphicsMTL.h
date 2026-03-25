// For Apple's Metal [bnGraphics]
#pragma once
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include "nGraphics/GraphicsUtilities.h"

class CommandBufferMTL : public ICommandBuffer {
private:
    void EndCurrentEncoder();
public:
    void* buffer;
    CommandPoolDesc::Type type;

    // Metal Encoders (Only one can be active at a time)
    void*  renderEncoder = nullptr;
    void*  computeEncoder = nullptr;
    void*  blitEncoder = nullptr;

    CommandBufferMTL(void* buf, CommandPoolDesc::Type t)
            : buffer(buf), type(t) {}

    // --- BARRIERS ARE NO-OPS IN METAL ---
    void PipelineBarrier(ITexture *image, ImageLayout oldL, ImageLayout newL,
                         ImageAccessLayout src, ImageAccessLayout dst) override {
        // Metal handles this automatically via Hazard Tracking.
        // We just update the engine's internal tracking state.
        image->explicitLayout = newL;
    }

    void PipelineBarrierBatched(ITexture *image, ImageLayout oldL, ImageLayout newL,
                                ImageAccessLayout src, ImageAccessLayout dst) override {
        image->explicitLayout = newL;
    }

    void FlushBatchedBarriers() override {
        // Nothing to flush!
    }
};

class CommandPoolMTL : public ICommandPool {
public:
    void* queue;
    CommandPoolDesc desc;

    CommandPoolMTL(void* q, const CommandPoolDesc& d)
            : queue(q), desc(d) {}
};

class DescriptorPoolMTL : public IDescriptorPool {
public:
    DescriptorPoolMTL(const DescriptorPoolDesc& desc) : desc(desc) {}
    DescriptorPoolDesc desc;
};

struct BindingInfo {
    uint32_t binding;
    DescriptorType type;
    IShaderStage stages;
};

class DescriptorSetLayoutMTL : public IDescriptorSetLayout {
public:
    std::vector<BindingInfo> bindings;

    DescriptorSetLayoutMTL(const DescriptorSetLayoutDesc& desc) {
        for (const auto& b : desc.bindings) {
            bindings.push_back({ b.binding, b.type, b.stageFlags });
        }
    }
};

class InputLayoutMTL : public IInputLayout {
public:
    void* inputLayout;

    InputLayoutMTL() {

    }

    void Release() override;

    void *GetNativeHandle() override {
        return inputLayout;
    }
};

class ShaderMTL : public IShader {
public:
    void* shaderFunction;

    ShaderMTL() {

    }

    void Release() override;

    void *GetNativeHandle() override {
        return shaderFunction;
    }
};

class SamplerMTL : public ISamplerState {
public:
    void* sampler = nullptr;

    SamplerMTL() {

    }

    void Release()override;

    void *GetNativeHandle() override {
        return sampler;
    }
};

class BufferMTL : public IBuffer {
public:
    BufferMTL()
    {
    }

    ~BufferMTL();

    void* GetNativeHandle() override { return mHandle; }
private:
    void* mHandle;
    friend class bnGraphicsMTL;
};

class DeviceContextMTL : public IDeviceContext {
public:
    void* GetNativeHandle() override {
        return currentFrame;
    }

    void Release() override;

    void* operator->() {
        return currentFrame;
    }

    operator void*() const { return currentFrame; }
private:
    void* currentFrame;
    friend class bnGraphicsMTL;
};

class CommandListMTL : public ICommandList {
private:
    void* m_cmdBuffer;
    void* m_encoder;

    // Internal helper to ensure we have an active encoder
    void EnsureEncoder();

public:
    CommandListMTL(void* buffer) : m_cmdBuffer(buffer), m_encoder(nullptr) {}

    ~CommandListMTL();

    void BindPipeline(IPipeline* pipeline) override;

    void BindBuffer(IBuffer* buffer) override;

    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override;

    void Release() override;
};

class TextureMTL : public ITexture {
public:
    ~TextureMTL();

    TextureMTL(bool ownership = true)  {
        owner = ownership;
    };

    void* GetNativeHandle() override { return mNativeTexture; }
    void* mNativeTexture = nullptr;
};

class DeviceMTL : public IDevice {
public:
    void* GetNativeHandle() override {
        return device;
    }
    void Release() override;

    void*  operator->() {
        return device;
    }

    operator void*() const { return device; }

private:
    void* device;
    friend class bnGraphicsMTL;
};


struct ResourceBinding {
    uint32_t slot;
    void* resource; // Can be cast to Buffer or Texture
    void* sampler;
    DescriptorType type;
};

class DescriptorSetMTL : public IDescriptorSet {
public:
    // We store a map or vector of what is bound to which slot
    std::map<uint32_t, ResourceBinding> bindings;

    void SetBuffer(uint32_t slot, IBuffer* buffer) override;

    void SetTexture(uint32_t slot, ITexture* texture) override;

    void SetSampler(uint32_t slot, ISamplerState* sampler) override;

    void Update() override {
        // In Vulkan, Update() calls vkUpdateDescriptorSets.
        // In Metal, this is a No-Op because we just store the pointers locally.
    }
};

struct RasterizerMTL : public IRasterizerState {
    uint cullMode;
    uint winding;
    uint fillMode;
    RasterizerDesc desc; // Keep the original for PSO flags

    RasterizerMTL(const RasterizerDesc& d);

    void* GetNativeHandle() override { return nullptr; }
};

class DepthStencilStateMTL : public IDepthStencilState {
public:
    void* handle;

    DepthStencilStateMTL(void* device, const DepthStencilDesc& desc);

    ~DepthStencilStateMTL();

    void* GetNativeHandle() override { return (void*)handle; }
};

class BlendStateMTL : public IBlendState {
public:
    BlendStateDesc desc;

    BlendStateMTL(const BlendStateDesc& d) : desc(d) {}

    // This helper will be called inside your PipelineMTL constructor
    void ApplyToDescriptor(void* pipelineDesc);

    void* GetNativeHandle() override { return nullptr; }
};
class RenderTargetMTL : public IRenderTarget {
public:
    struct Attachment {
        void* texture;
        rgba clearColor;
        uint loadAction;
        uint storeAction;
    };

    std::vector<Attachment> colorAttachments;

    // Depth/Stencil
    void* depthTexture = nullptr;
    float depthClearValue = 1.0f;
    u32 stencilClearValue = 0;

    RenderTargetMTL(const RenderTargetDesc& desc);

    void* GetNativeHandle() override { return nullptr; }

    void Release() override;
};

class PipelineMTL : public IPipeline {
public:
    IGraphicsDeviceConfig& config;
    void* device = nullptr;
    void* pipelineState = nullptr;

    // Metal handles these as simple CPU metadata containers
    DescriptorSetLayoutMTL* descriptorSetLayout = nullptr;
    DescriptorPoolMTL* descriptorPool = nullptr;
    std::map<uint32_t, DescriptorSetMTL*> descriptorSets;

    RenderTargetMTL* renderTarget;
    sVec<PipelineMTL*>* releaseVec;

    PipelineMTL(
            sVec<PipelineMTL*>* releaseVec,
            void* dev,
            DescriptorSetLayoutMTL* setLayout,
            DescriptorPoolMTL* pool,
            std::vector<ShaderMTL*> shaders,
            InputLayoutMTL* inputLayout,
            RasterizerMTL* rasterizer,
            DepthStencilStateMTL* depthStencil,
            BlendStateMTL* blendState,
            RenderTargetMTL* renderTarget,
            IGraphicsDeviceConfig& config)
            : config(config), device(dev), descriptorSetLayout(setLayout),
              descriptorPool(pool), releaseVec(releaseVec), renderTarget(renderTarget)
    {
        // In Metal, there is no "Pipeline Layout".
        // We go straight to creating the Graphics Pipeline (PSO).
        CreateGraphicsPipeline(shaders, inputLayout, rasterizer, depthStencil, blendState);
    }

    ~PipelineMTL() {
        releaseVec->push_back(this);
    }

    void* GetNativeHandle() override { return (void*)pipelineState; }

    // Descriptor sets in Metal are just CPU-side resource trackers
    IDescriptorSet* CreateDescriptorSet(uint32_t slot = 0) override {
        // We don't call a 'vkAllocate'. we just 'new' our tracker.
        DescriptorSetMTL* ds = new DescriptorSetMTL();
        descriptorSets[slot] = ds;
        return ds;
    }

private:
    void CreateGraphicsPipeline(
            const std::vector<ShaderMTL*>& shaders,
            InputLayoutMTL* inputLayout,
            RasterizerMTL* rasterizer,
            DepthStencilStateMTL* depthStencil,
            BlendStateMTL* blendState);
};

class PipelineBuilderMTL: public IPipelineBuilder {
public:
    PipelineBuilderMTL(IGraphicsDeviceConfig& config, sVec<PipelineMTL*>* releaseVec, VkDevice device, RenderTargetMTL* pass) : config(config), device(device), renderPass(pass), releaseVec(releaseVec) {}

    PipelineBuilderMTL& AddShader(IShader* shader) override {
        auto vShader = dynamic_cast<ShaderMTL*>(shader);

        if (!vShader) return *this;

        shaders.push_back(vShader);
        return *this;
    }

    PipelineBuilderMTL& SetInputLayout(IInputLayout* layout) override {
        auto vLayout = dynamic_cast<InputLayoutMTL*>(layout);

        if (!vLayout) return *this;

        inputLayout = vLayout;
        return *this;
    }

    PipelineBuilderMTL& SetRasterizer(IRasterizerState* raster) {
        auto vRaster = dynamic_cast<RasterizerMTL*>(raster);

        if (!vRaster) return *this;

        rasterizer = vRaster;
        return *this;
    }

    PipelineBuilderMTL& SetDepthStencil(IDepthStencilState* depth) {
        auto vDepth = dynamic_cast<DepthStencilStateMTL*>(depth);

        if (!vDepth) return *this;

        depthStencil = vDepth;
        return *this;
    }

    PipelineBuilderMTL& SetBlendState(IBlendState* blend) {
        auto vBlend = dynamic_cast<BlendStateMTL*>(blend);

        if (!vBlend) return *this;

        blendState = vBlend;
        return *this;
    }

    PipelineBuilderMTL& SetDescriptorPool(IDescriptorPool* pool) {
        auto vPool = dynamic_cast<DescriptorPoolMTL*>(pool);

        if (!vPool) return *this;

        descriptorPool = vPool;
        return *this;
    }

    IPipelineBuilder& SetRenderTarget(IRenderTarget* target) {
        auto vRenderTarget = dynamic_cast<RenderTargetMTL*>(target);

        if (!vRenderTarget) return *this;

        renderPass = vRenderTarget;
        return *this;
    };

    PipelineBuilderMTL& SetDescriptorSetLayout(IDescriptorSetLayout* layout) {
        auto vLayout = dynamic_cast<DescriptorSetLayoutMTL*>(layout);

        if (!vLayout) return *this;

        descriptorSetLayout = vLayout;
        return *this;
    }

    sVec<IShader*>* GetShaders() {
        if (shaders.size() != interfaceShaders.size()) {
            interfaceShaders.clear();
            for (auto* s : shaders) {
                interfaceShaders.push_back(static_cast<IShader*>(s));
            }
        }

        return &interfaceShaders;
    }

    IPipeline* Build() override {
        if (!inputLayout || !rasterizer || !depthStencil || !blendState || !renderPass) {
            delete this;
            return nullptr;
        }

        PipelineMTL* pipeline = new PipelineMTL(
                releaseVec,
                device,
                descriptorSetLayout,
                descriptorPool,
                shaders,
                inputLayout,
                rasterizer,
                depthStencil,
                blendState,
                renderPass,
                config
        );

        delete this;
        return pipeline;
    }

public:
    sVec<IShader*> interfaceShaders;
    IGraphicsDeviceConfig& config;
    VkDevice device;
    std::vector<ShaderMTL*> shaders;
    InputLayoutMTL* inputLayout = nullptr;
    RasterizerMTL* rasterizer = nullptr;
    DepthStencilStateMTL* depthStencil = nullptr;
    BlendStateMTL* blendState = nullptr;
    DescriptorSetLayoutMTL* descriptorSetLayout = nullptr;
    DescriptorPoolMTL* descriptorPool = nullptr;
    RenderTargetMTL* renderPass = VK_NULL_HANDLE;
    sVec<PipelineMTL*>* releaseVec;
};


class bnGraphicsMTL : public IGraphicsDeviceExplicit {
public:
    IGraphicsDeviceConfig& config;
    bnGraphicsMTL(SysHandle& handle, IGraphicsDeviceConfig& config);

    const char* GetAPIName() const {
        return "METAL";
    }
    uint32_t GetAPIVersion() const {
        return 1;
    }
    bool IsFeatureSupported(const std::string& feature) const {
        return true;
    }
    bool Init() override;
    ICommandList* GetCommandList() override;
    void DestroyPending();
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(long width, long height) override;

    void ReleaseShader(IShader**) override;
    void ReleaseBuffer(IBuffer**) override;
    void ReleaseTexture(ITexture**) override;
    void ReleaseCommandPool(ICommandPool** pool) override;
    void ReleaseDescriptorSetLayout(IDescriptorSetLayout** layout) override;
    void ReleaseOnPend(void*) override;
    void Shutdown() override;

    IPipelineBuilder* CreatePipelineBuilder() override;
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
    ICommandPool* CreateCommandPool(CommandPoolDesc desc) override;
    IDescriptorPool* CreateDescriptorPool(DescriptorPoolDesc desc) override;
    IDescriptorSetLayout* CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc) override;
    IDeviceContext* getContext() override {
        return &mCurrentFrameBuffer;
    }
    IDevice* getDevice() override {
        return &device;
    }
    void* getCommandQueue() {
        return mNativeCommandQueue;
    }
    void WaitForNewFrame();
    ICommandBuffer* BeginSingleTimeCommands(ICommandPool* pool) override;
    void EndSingleTimeCommands(ICommandBuffer* buffer) override;
    void CopyToBuffer(IBuffer* buffer, ICommandBuffer* pool, void* data, size_t size) override;
    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;
    void ClearPendingReleases() override;
    void WaitTillImFree() override;ITexture* GetSwapchainImage() override;
    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF) override;

private:
    DeviceMTL device;
    void* mNativeSwapchain = nullptr;
    DeviceContextMTL mCurrentFrameBuffer;
    void* mNativeCommandQueue = nullptr;
    void* mCurrentDrawable = nullptr;
    SysHandle& handle;
};

