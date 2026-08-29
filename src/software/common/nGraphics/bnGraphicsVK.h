#pragma once
#include <atomic>

#include "nGraphics/ExplicitGraphicsAbstract.h"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include "GraphicsUtilities.h"

class InputLayoutVK : public IInputLayout{
public:
    VkPipelineVertexInputStateCreateInfo vkVertexInputInfo{};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    InputLayoutVK(const InputLayoutDesc& desc);

    void* GetNativeHandle() override {
        return nullptr;
    }

    void Release() override
    {
        delete this;
    }

    VkPipelineVertexInputStateCreateInfo* GetVertexInputState() { return &vkVertexInputInfo; }
};

class BufferVK : public IBuffer
{
public:
    BufferVK(VkDevice device, const BufferDesc& desc)
        : device(device), desc(desc) {
        type = desc.type;
    }

    ~BufferVK() override
    {
        BufferVK::Release();
    }

    void Release() override
    {
        if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
        if(stagingBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, stagingBuffer, nullptr);
        if (stagingMemory != VK_NULL_HANDLE) vkFreeMemory(device, stagingMemory, nullptr);
        if (mStagingBuffer) mStagingBuffer->Release();
    }

    void* GetNativeHandle() override { return buffer; }

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDevice device ;
    BufferDesc desc;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    IBuffer* mStagingBuffer = VK_NULL_HANDLE;
    std::vector<u8> dataBuffer;
};


class ShaderVK : public IShader {
public:
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkDevice device;

    ShaderVK(VkDevice device, const ShaderDesc& desc);

    ~ShaderVK() override
    {
        ShaderVK::Release();
    }

    void Release() override
    {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
        }
    }

    void* GetNativeHandle() override { return shaderModule; }

    [[nodiscard]] VkPipelineShaderStageCreateInfo GetStageInfo() const;
};

class TextureVK : public ITexture {
public:
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    ~TextureVK() override
    {
        TextureVK::Release();
    }

    explicit TextureVK(bool ownership = true)  {
        owner = ownership;
    };

    uint32_t width{}, height{}, mipLevels{};

    void Release() override {
        if (owner) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }

            if (image != VK_NULL_HANDLE) {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }

            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }

            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }

        desc = {};

    }

    void* GetNativeHandle() override {
        return image;
    }
};

class ViewportVK : public IViewPort
{
public:
    ViewportVK(const ViewPortDesc& desc);

    void* GetNativeHandle() override { return nullptr; }

    VkViewport viewport{};
    VkRect2D   scissor{};
};

class RasterizerVK : public IRasterizerState {
public:
    VkPipelineRasterizationStateCreateInfo vkRaster{};

    explicit RasterizerVK(const RasterizerDesc& desc);

    void* GetNativeHandle() override { return nullptr; }

    void Release() override
    {
        delete this;
    }
};


class DepthStencilStateVK : public IDepthStencilState {
public:
    VkPipelineDepthStencilStateCreateInfo vkDepthStencil{};

    DepthStencilStateVK(const DepthStencilDesc& desc);

    void* GetNativeHandle() override
    {
        return nullptr; //VK!
    }

    void Release() override
    {
        delete this;
    }
};

class BlendStateVK : public IBlendState {
public:
    VkPipelineColorBlendStateCreateInfo vkBlend{};
    std::vector<VkPipelineColorBlendAttachmentState> attachments;

    BlendStateVK(const BlendStateDesc& desc);

    void Release() override
    {
        delete this; // todo: remove?
    }

    void* GetNativeHandle() override { return nullptr; }
};

class SamplerVK : public ISamplerState {
public:
    VkDevice device;
    VkSampler sampler = VK_NULL_HANDLE;

    SamplerVK(VkDevice dev, const SamplerStateDesc& desc, sVec<SamplerVK*>* samplerReleasePointer);

    void Release() override {
        samplerReleasePointer->push_back(this);
    }

    void* GetNativeHandle() override {
        return sampler;
    }

private:
    sVec<SamplerVK*>* samplerReleasePointer;
};

struct PendingDrawVK : public IPendingDraw {
    VkCommandBuffer cmdBuffer{};
};

class CommandListVK : public ICommandList {
public:
    VkDevice device;
    VkCommandBuffer cmdBuffer;
    sVec<CommandListVK*>* releases;
    sVec<PendingDrawVK>* pDraws; 
    PendingDrawVK draw;

    CommandListVK(VkDevice dev, VkCommandBuffer buffer, sVec<CommandListVK*>* rel, sVec<PendingDrawVK>* pDraws)
        : device(dev), cmdBuffer(buffer), releases(rel), pDraws(pDraws){
        draw.cmdBuffer = cmdBuffer;
    }

    void Release() override
    {
        if(releases) releases->push_back(this);
    }

    void BindPipeline(IPipeline* pipeline) override;
    void BindDescriptorSet(IDescriptorSet* set, uint32_t index) override;

    void BindViewPort(IViewPort* viewport) override;
    void BindScissor(IViewPort* scissor) override;
    void BindBuffer(IBuffer* buffer) override;

    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset) override;
    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset) override;

    void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override;

};

class PipelineVK;

class CommandBufferVK : public ICommandBuffer {
public:
    VkCommandBuffer buffer;
    VkCommandPool& pool;

    std::vector<VkImageMemoryBarrier> pendingBarriers;

    CommandBufferVK(VkCommandBuffer buffer, VkCommandPool& pool) : buffer(buffer), pool(pool) {}

    static VkImageMemoryBarrier BarrierCreator(ITexture* image,
                                               ImageLayout oldLayout,
                                               ImageLayout newLayout,
                                               ImageAccessLayout srcAccessMask,
                                               ImageAccessLayout dstAccessMask);

    void PipelineBarrierBatched(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override;

    void FlushBatchedBarriers() override;

    void PipelineBarrier(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override;


    void* GetNativeHandle() override
    {
        return buffer;
    }
};

class DescriptorSetLayoutVK : public IDescriptorSetLayout {
public:
    VkDescriptorSetLayout layout{};
    VkDevice device;
    DescriptorSetLayoutDesc desc;

    DescriptorSetLayoutVK(VkDevice device, const DescriptorSetLayoutDesc& desc);

    void* GetNativeHandle() override
    {
        return layout;
    }

    ~DescriptorSetLayoutVK() override
    {
        DescriptorSetLayoutVK::Release();
    }

    void Release() override
    {
        if (layout) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
            layout = nullptr;
        }
    }
};

class DescriptorPoolVK : public IDescriptorPool {
public:
    VkDescriptorPool pool{};
    VkDevice device;

    DescriptorPoolVK(VkDevice device, const DescriptorPoolDesc& desc);

    void* GetNativeHandle() override
    {
        return pool;
    }

    void Release() override
    {
        if (pool) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = nullptr;
        }
    }
};

class DescriptorSetVK : public IDescriptorSet {
public:
    VkDevice device;
    VkDescriptorSet descriptorSet;
    PipelineVK* pipeline;

    std::vector<BufferVK*> buffers;
    std::vector<TextureVK*> textures;
    std::vector<SamplerVK*> samplers;

    DescriptorSetVK(VkDevice dev, VkDescriptorSet set, PipelineVK* parentP) : device(dev), descriptorSet(set), pipeline(parentP) {}
    ~DescriptorSetVK() override;

    void SetBuffer(uint32_t slot, IBuffer* buffer) override;
    void SetTexture(uint32_t slot, ITexture* texture) override;
    void SetSampler(uint32_t slot, ISamplerState* sampler) override;
    void Update() override;
};

class bnGraphicsVK;

class DepthStencilVK : public IDepthStencil {
public:
    ITexture* texture;
    VkImageView depthView;
public:
    void* GetNativeHandle() override
    {
        return depthView;
    }
    void Release() override {}
};

class RenderTargetVK : public IRenderTarget {
public:
    std::vector<TextureVK*> textures;
    DepthStencilVK* depth = nullptr;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
public:
    void* GetNativeHandle() override
    {
        return renderPass;
    }
    void Release() override
    {
        for (auto& texture : textures) {
            texture->Release();
        }
    }
};


class PipelineVK : public IPipeline {
public:
    IGraphicsDeviceConfig& config;
    VkDevice device;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    DescriptorSetLayoutVK* descriptorSetLayout = VK_NULL_HANDLE;
    sVec<PipelineVK*>* releaseVec;
    DescriptorPoolVK* descriptorPool = VK_NULL_HANDLE;  // <-- new: pool for descriptor sets
    std::map<u32, DescriptorSetVK*> descriptorSets;
    IRenderTarget* renderTarget;
    void* GetNativeHandle() override { return pipeline; }

    PipelineVK(
        sVec<PipelineVK*>* releaseVec,
        VkDevice dev,
        DescriptorSetLayoutVK* setLayout,
        DescriptorPoolVK* pool,
        const std::vector<ShaderVK*>& shaders,
        InputLayoutVK* inputLayout,
        RasterizerVK* rasterizer,
        DepthStencilStateVK* depthStencil,
        BlendStateVK* blendState,
        RenderTargetVK* renderTarget,
        IGraphicsDeviceConfig& config);

    ~PipelineVK() override = default;

    void Release() override
    {
        releaseVec->push_back(this);
    }

    // Descriptor set creation (no parameters)
    IDescriptorSet* CreateDescriptorSet(u32 slot) override;

    IDescriptorSet* GetDescriptorSet(u32 slot) override;
};

class PipelineBuilderVK : public IPipelineBuilder {
public:
    PipelineBuilderVK(IGraphicsDeviceConfig& config, sVec<PipelineVK*>* releaseVec, VkDevice device, RenderTargetVK* pass) : config(config), device(device), renderPass(pass), releaseVec(releaseVec) {}

    PipelineBuilderVK& From(const IPipelineBuilder& builder) override;

    PipelineBuilderVK& AddShader(IShader* shader) override;

    PipelineBuilderVK& SetInputLayout(IInputLayout* layout) override;

    PipelineBuilderVK& SetRasterizer(IRasterizerState* raster) override;

    PipelineBuilderVK& SetDepthStencil(IDepthStencilState* depth) override;

    PipelineBuilderVK& SetBlendState(IBlendState* blend) override;

    PipelineBuilderVK& SetDescriptorPool(IDescriptorPool* pool) override;

    IPipelineBuilder& SetRenderTarget(IRenderTarget* target) override;

    PipelineBuilderVK& SetDescriptorSetLayout(IDescriptorSetLayout* layout) override;

    sVec<IShader*>* GetShaders() override;

    IPipeline* Build() override;

public:
    sVec<IShader*> interfaceShaders;
    IGraphicsDeviceConfig& config;
    VkDevice device;
    std::vector<ShaderVK*> shaders;
    InputLayoutVK* inputLayout = nullptr;
    RasterizerVK* rasterizer = nullptr;
    DepthStencilStateVK* depthStencil = nullptr;
    BlendStateVK* blendState = nullptr;
    DescriptorSetLayoutVK* descriptorSetLayout = nullptr;
    DescriptorPoolVK* descriptorPool = nullptr;
    RenderTargetVK* renderPass = VK_NULL_HANDLE;
    sVec<PipelineVK*>* releaseVec;
};

class DeviceVK : public IDevice {
public:
    void* GetNativeHandle() override {
        return device;
    }
    void Release() override {
        if (device) {
            vkDestroyDevice(device, nullptr);
        }
    }

    VkDevice operator->() const
    {
        return device;
    }


    [[nodiscard]] VkDevice Get() const
    {
        return device;
    }

    operator VkDevice() const { return device; }

private:
    VkDevice device;
    friend class bnGraphicsVK;
};

class DeviceContextVK : public IDeviceContext {
public:
    void* GetNativeHandle() override {
        return deviceC;
    }
    void Release() override {
        if (deviceC) {
            vkFreeCommandBuffers(
                device,           // VkDevice
                commandPool,     
                1,               
                &deviceC   
            );
        }
    }

    VkCommandBuffer operator->() const
    {
        return deviceC;
    }

    operator VkCommandBuffer() const { return deviceC; }

    operator VkCommandPool() const { return commandPool; }

private:
    VkDevice device;
    VkCommandBuffer deviceC;
    VkCommandPool commandPool;
    friend class bnGraphicsVK;
};

class CommandPoolVK : public ICommandPool {
public:
    VkCommandPool commandPool{};
    VkDevice device;

    CommandPoolVK(VkDevice device, CommandPoolDesc desc) : device(device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = desc.queueFamilyIndex;
        poolInfo.flags = 0;

        if (desc.transient) {
            poolInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        }
        if (desc.resettable) {
            poolInfo.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }

      
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

    }

    void Release() override
    {
        if(commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    }

    void* GetNativeHandle() override
    {
        return commandPool;
    }
};



class bnGraphicsVK : IGraphicsDeviceExplicit
{
public:
    IGraphicsDeviceConfig& config;
    bnGraphicsVK(SysHandle& handle, IGraphicsDeviceConfig& config);
    
    [[nodiscard]] const char* GetAPIName() const override
    {
        return "VULKAN";
    }
    [[nodiscard]] uint32_t GetAPIVersion() const override
    {
        return 130; // ?
    }
    [[nodiscard]] bool IsFeatureSupported(const std::string& feature) const override
    {
        return true;
    }
    bool Init() override;
    ICommandList* GetCommandList() override;
    void DestroyPending();
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(long changedWidth, long changedHeight) override;

    void ReleaseShader(IShader**) override;
    void ReleaseBuffer(IBuffer**) override;
    void ReleaseTexture(ITexture**) override;
    void ReleaseCommandPool(ICommandPool** pool) override;
    void ReleaseDescriptorPool(IDescriptorPool** pool) override;
    void ReleaseDescriptorSetLayout(IDescriptorSetLayout** layout) override;
    void ReleaseOnPend(void*) override;
    void Shutdown() override;

    IPipelineBuilder* CreatePipelineBuilder() override;
    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData) override;
    IShader* CreateShader(const ShaderDesc& desc) override;
    IBuffer* CreateBuffer(const BufferDesc& desc, const void* data) override;
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
        return &commandBuffer[currentFrame];
    }
    IDevice* getDevice() override {
        return &device;
    }

    void WaitForNewFrame() override;
    ICommandBuffer* BeginSingleTimeCommands(ICommandPool* pool) override;
    void EndSingleTimeCommands(ICommandBuffer* buffer) override;
    void CopyToBuffer(IBuffer* buffer, ICommandBuffer* pool, void* data, size_t size) override;
    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;
    void ClearPendingReleases() override;
    void WaitTillImFree() override;ITexture* GetSwapchainImage() override;
    void PushGroup(const char* name, uint32_t color) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color) override;
    sVec<PipelineVK*> releasePipelines;
    sVec<CommandListVK*> releaseCommandBuffers;
    bool dontClearYet = false;
    std::vector<VkFence> inFlightFences;
    uint32_t imageIndex{};
    size_t currentFrame{};
public:
   
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateFrameBuffers();
    u32 FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    SysHandle& handle;
    std::atomic_bool free;
    VkInstance instance{};
    DeviceVK device;
   
    VkQueue presentQueue{};
    VkPhysicalDevice physicalDevice = nullptr;
    long width{}, height{};
    VkSurfaceKHR surface = nullptr;
    VkSwapchainKHR swapChain = nullptr;
    std::vector<VkImage> swapChainImages;
    TextureVK* currentSwapChainImage = nullptr;
    VkFormat swapchainImageFormat;
    VkExtent2D swapChainExtent{};
    RenderTargetVK* swpchTarget = nullptr;
    ITexture* msaaSWPCHTarget = nullptr;
    ITexture* depthTexture = nullptr;
    IDepthStencil* depthStencil = nullptr;
    std::vector<VkImageView> swapchainImageViews;
    VkImageView depthImageView = nullptr;
    std::vector<VkFramebuffer> swapChainFrameBuffers;
    VkImage depthImage = nullptr;
    VkDeviceMemory depthImageMemory = nullptr;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    //sVec<> commandPool;
    sVec<DeviceContextVK> commandBuffer;
    sVec<CommandListVK*> commandLists;
    //VkDescriptorPool descriptorPool;
    //VkDescriptorSetLayout textureSetLayout;

    // Release bla blas
    sVec<void*> pendVoids;
    sVec<IBuffer**> bufferRelease;
    sVec<ITexture**> textureRelease;
    sVec<IShader**> shaderRelease;
    sVec<ICommandPool**> poolRelease;
    sVec<IDescriptorPool**> descriptorPoolRelease;
    sVec<IDescriptorSetLayout**> descriptorSetLayoutRelease;
    sVec<CommandBufferVK*> cbRelease;
    sVec<SamplerVK*> samplerRelease;
    CommandPoolVK* copyPool = nullptr;
    VkQueue graphicsQueue{};
    sVec<PendingDrawVK> pDraws;
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkSampleCountFlagBits msaaSamples;
};

