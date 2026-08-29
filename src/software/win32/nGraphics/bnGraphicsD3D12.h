#include "nGraphics/ExplicitGraphicsAbstract.h"
#include "d3d12.h"
#include "D3dx12.h"
#include "dxgi.h"
#include "dxgi1_4.h"
#include <wrl/client.h>
#include <comdef.h>
#include <dxgi1_6.h>
#include <optional>
#include <stdexcept>
#include "nGraphics/GraphicsUtilities.h"
using namespace Microsoft::WRL;

class TextureD3D12 : public ITexture {
public:
    ID3D12Resource* resource = nullptr; // The actual texture
    ID3D12DescriptorHeap* srvHeap = nullptr; // Shader Resource View heap (optional)
    ID3D12DescriptorHeap* samplerHeap = nullptr; // Sampler heap (optional)
public:
    TextureD3D12(bool ownership = true) {
        owner = ownership;
    }

    void Release() override {
        if (owner) {
            if(resource) resource->Release();
            if(srvHeap) srvHeap->Release();
            if(samplerHeap) samplerHeap->Release();
        }
    }

    void* GetNativeHandle() override {
        return resource;
    }
};


class DepthStencilD3D12 : public IDepthStencil {
public:
    TextureD3D12* texture = nullptr;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

    void* GetNativeHandle() override {
        return nullptr;
    }

    void Release() override {
        texture = nullptr; // actual texture cleanup happens elsewhere
    }
};


class RenderTargetD3D12 : public IRenderTarget {
public:
    // Color targets
    std::vector<TextureD3D12*> colorTargets;
    // Depth/stencil target
    DepthStencilD3D12* depth = nullptr;

    // Descriptor heaps
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    ID3D12DescriptorHeap* dsvHeap = nullptr;

    UINT rtvDescriptorSize = 0;

    // Clear values
    std::vector<rgba> clearColors;
    float depthClear = 1.0f;
    uint32_t stencilClear = 0;

    // Dimensions
    UINT width = 0;
    UINT height = 0;
    UINT mipLevels = 1;
public:

    // Default constructor
    RenderTargetD3D12() = default;

    // Non-copyable (important: prevents double-release)
    RenderTargetD3D12(const RenderTargetD3D12&) = delete;
    RenderTargetD3D12& operator=(const RenderTargetD3D12&) = delete;

    // Move constructor
    RenderTargetD3D12(RenderTargetD3D12&& other) noexcept {
        *this = std::move(other);
    }

    // Move assignment
    RenderTargetD3D12& operator=(RenderTargetD3D12&& other) noexcept;

    // Destructor (no ownership of COM objects assumed here)
    ~RenderTargetD3D12() override = default;

    // Return a native handle
    void* GetNativeHandle() override {
        return rtvHeap;
    }

    // Release resources
    void Release() override;
};

class ShaderD3D12 : public IShader {
public:
    ShaderDesc desc;
    ShaderD3D12(const ShaderDesc& d) : desc(desc) {
        binary = sVec<u8>(d.bytecode, d.bytecode + d.bytecodeSize);
        ogData = d.ogData;
        type = d.type;
        desc.ogData.clear();
        desc.bytecode = nullptr;
        desc.bytecodeSize = 0;
    }

    void* GetNativeHandle() override {
        return binary.data(); // pointer to bytecode for PSO creation
    }
};

class BufferD3D12 : public IBuffer {
public:
    ID3D12Resource* resource = nullptr;
    BufferD3D12* upload = nullptr;
    BufferDesc desc;
    std::vector<uint8_t> dataBuffer;

    BufferD3D12(const BufferDesc& d) : desc(d) {
    type = desc.type;
    }

    void* GetNativeHandle() override {
        return resource;
    }

    void Release() override {
        dataBuffer.clear();
        dataBuffer.shrink_to_fit();
        if (resource) resource->Release();
    }
};

class InputLayoutD3D12 : public IInputLayout {
public:
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    std::vector<std::string> semanticNames; // owns the memory

    explicit InputLayoutD3D12(const InputLayoutDesc& desc);

    void* GetNativeHandle() override {
        return elements.data();
    }

    void Release() override {
        semanticNames.clear();
        semanticNames.shrink_to_fit();
        elements.clear();
        elements.shrink_to_fit();
        delete this;
    }
};

class SamplerStateD3D12 : public ISamplerState {
public:
    D3D12_SAMPLER_DESC desc{};

    void* GetNativeHandle() override {
        return nullptr;
    }

    void Release() override {
        // nothing to do
        delete this;
    }
};

struct ViewPortD3D12 : public IViewPort {
    D3D12_VIEWPORT viewport;
    D3D12_RECT  scissorRect;

    ViewPortD3D12(const ViewPortDesc& desc);

    void* GetNativeHandle() override {
        return &viewport; // Return pointer to D3D12_VIEWPORT
    }
};

struct RasterizerStateD3D12 : public IRasterizerState {
    D3D12_RASTERIZER_DESC desc;

    explicit RasterizerStateD3D12(const RasterizerDesc& src);

    void Release() override {
        delete this;
    }

    void* GetNativeHandle() override {
        return &desc; // Return pointer to D3D12_RASTERIZER_DESC
    }
};

class DepthStencilStateD3D12 : public IDepthStencilState {
public:
    D3D12_DEPTH_STENCIL_DESC desc;

    DepthStencilStateD3D12(const DepthStencilDesc& src);

    void* GetNativeHandle() override {
        return &desc;
    }

    void Release() override {
        delete this;
    }
};

class BlendStateD3D12 : public IBlendState {
public:
    D3D12_BLEND_DESC desc;

    explicit BlendStateD3D12(const BlendStateDesc& src);

    void* GetNativeHandle() override {
        return &desc;
    }

    void Release() override {
        delete this;
    }
};

class DescriptorPoolD3D12 : public IDescriptorPool {
public:
    ID3D12Device* device = nullptr;
    ID3D12DescriptorHeap* cbvSrvUavHeap = nullptr;
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    DescriptorPoolDesc desc;
    UINT cbvSrvUavDescriptorSize = 0;
    UINT samplerDescriptorSize = 0;

    UINT cbvSrvUavAllocated = 0;
    UINT samplerAllocated = 0;

    DescriptorPoolD3D12(ID3D12Device* dev, const DescriptorPoolDesc& desc);

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate(DescriptorType type);

    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

    [[nodiscard]] ID3D12DescriptorHeap* GetHeap(DescriptorType type) const;

    void ClearAllocations() {
        cbvSrvUavAllocated = 0;
        samplerAllocated = 0;
    }

    void FreeResources();

    void Release() override {
        FreeResources();
        desc.poolSizes.clear();
        desc.poolSizes.shrink_to_fit();
    }

    void* GetNativeHandle() override {
        return cbvSrvUavHeap;
    }
};


class DescriptorSetLayoutD3D12 : public IDescriptorSetLayout {
public:
    ID3D12RootSignature* rootSignature = nullptr;
    DescriptorSetLayoutDesc desc;

    DescriptorSetLayoutD3D12(ID3D12Device* device, const DescriptorSetLayoutDesc& desc);

    void* GetNativeHandle() override {
        return rootSignature;
    }

    void Release() override {
        if (rootSignature) rootSignature->Release();
        desc.bindings.clear();
        desc.bindings.shrink_to_fit();
    }
};

struct D3D12DescriptorTypeSlots {
    u32 normalSlot;
    u32 samplerSlot;
};
// TODO: Finish and polish. 
class DescriptorSetD3D12 : public IDescriptorSet {
public:
    ID3D12Device* device = nullptr;
    DescriptorPoolD3D12* pool = nullptr;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandles;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles;
    std::vector<DescriptorSetLayoutBindingDesc> bindings;
    std::map<DescriptorType, D3D12DescriptorTypeSlots> slotsB;
    UINT slotCount = 0;

    DescriptorSetD3D12(ID3D12Device* dev, DescriptorPoolD3D12* p, DescriptorSetLayoutD3D12* layout);

    void SetTexture(u32 slot, ITexture* tex) override;
    void SetBuffer(u32 slot, IBuffer* buf) override;
    void SetSampler(u32 slot, ISamplerState* sampler) override;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT slot) const {
        return gpuHandles[slot];
    }

    void Release() override {
        cpuHandles.clear();
        gpuHandles.clear();
    }
};


class PipelineD3D12 : public IPipeline {
public:
    sVec<PipelineD3D12*>* releasePipelines;
    ID3D12PipelineState* pso = nullptr;
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12Device* device = nullptr;
    DescriptorPoolD3D12* descriptorPool = nullptr;
    DescriptorSetLayoutD3D12* descriptorLayout = nullptr;
    std::unordered_map<u32, DescriptorSetD3D12*> descriptorSets;
    RenderTargetD3D12* d3dRenderTarget;

    PipelineD3D12(sVec<PipelineD3D12*>* pipe, ID3D12Device* device, DescriptorPoolD3D12* pool, DescriptorSetLayoutD3D12* layout, RenderTargetD3D12* target,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc,
        const D3D12_ROOT_SIGNATURE_DESC& rootDesc);

    void* GetNativeHandle() override {
        return pso;
    }

    void Release() override {
        releasePipelines->push_back(this);
    }
    IDescriptorSet* CreateDescriptorSet(u32 slot = 0) override;
    IDescriptorSet* GetDescriptorSet(u32 slot = 0) override {
        auto it = descriptorSets.find(slot);
        return it != descriptorSets.end() ? it->second : nullptr;
    }
};

class PipelineBuilderD3D12 : public IPipelineBuilder {
private:
    ID3D12Device* device = nullptr;
    sVec<PipelineD3D12*>* pipelineRelease;
    sVec<IShader*> shaders;
    IInputLayout* inputLayout = nullptr;
    IRenderTarget* renderTarget = nullptr;
    IRasterizerState* rasterizer = nullptr;
    IDepthStencilState* depthStencil = nullptr;
    IBlendState* blendState = nullptr;
    DescriptorPoolD3D12* descriptorPool = nullptr;
    DescriptorSetLayoutD3D12* descriptorLayout = nullptr;
public:
    PipelineBuilderD3D12(sVec<PipelineD3D12*>* pipe, ID3D12Device* dev, IRenderTarget* target) : pipelineRelease(pipe), device(dev), renderTarget(target) {}

    IPipelineBuilder& From(const IPipelineBuilder& builder) override;

    IPipelineBuilder& AddShader(IShader* shader) override;

    IPipelineBuilder& SetInputLayout(IInputLayout* layout) override;

    IPipelineBuilder& SetRenderTarget(IRenderTarget* target) override;

    IPipelineBuilder& SetRasterizer(IRasterizerState* raster) override;

    IPipelineBuilder& SetDepthStencil(IDepthStencilState* depth) override;

    IPipelineBuilder& SetBlendState(IBlendState* blend) override;

    IPipelineBuilder& SetDescriptorPool(IDescriptorPool* pool) override;

    IPipelineBuilder& SetDescriptorSetLayout(IDescriptorSetLayout* layout) override;

    sVec<IShader*>* GetShaders() override;

    IPipeline* Build() override;
};

struct PendingDrawD3D12 : public IPendingDraw {
    ID3D12GraphicsCommandList* cmdBuffer = nullptr;
};


class CommandListD3D12 : public ICommandList {
public:
    ComPtr<ID3D12GraphicsCommandList> list;
    sVec<PendingDrawD3D12>* pDraws = nullptr;
    PendingDrawD3D12 pDraw;
    sVec<CommandListD3D12*>* releases = nullptr;

    CommandListD3D12(sVec<CommandListD3D12*>* releases, ComPtr<ID3D12GraphicsCommandList> cmdList, sVec<PendingDrawD3D12>* pd)
        : list(cmdList), pDraws(pd), releases(releases) {
        pDraw.cmdBuffer = list.Get();
    }

    void Release() override {
        releases->push_back(this);
    }

    void BindPipeline(IPipeline* pipeline) override {
        // Cast to D3D12 pipeline
        pDraw.pipeline = pipeline;
    }

    void BindDescriptorSet(IDescriptorSet* set, uint32_t index = 0) override {
        pDraw.ds = set;
        pDraw.dsIndex = index;
    }

    void BindViewPort(IViewPort* vp) override {
        pDraw.viewport = vp;
    }

    void BindScissor(IViewPort* vp) override {
        pDraw.scissor = vp;
    }

    void BindBuffer(IBuffer* buffer) override {
        pDraw.buffer = buffer;
    }

    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override {
        pDraw.vertexCount = vertexCount;
        pDraw.type = type;
        pDraw.vertexOffset = vertexOffset;
        pDraws->push_back(pDraw);
    }

    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override {
        pDraw.type = type;
        pDraw.indexBuffer = indexBuffer;
        pDraw.indexCount = indexCount;
        pDraw.indexCount = indexOffset;
        pDraws->push_back(pDraw);
    }

    void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override {
        // Use an upload heap or staging buffer to copy
        auto d3dBuffer = dynamic_cast<BufferD3D12*>(buffer);
        // Map/Unmap or use UpdateSubresources here
    }
};

class DeviceD3D12 : public IDevice {
public:
    // Return native handle (ID3D12Device*)
    void* GetNativeHandle() override {
        return device.Get();
    }

    // Release the device
    void Release() override {
        device.Reset(); // ComPtr automatically releases
    }

    // Operator overloads for convenience
    ID3D12Device* operator->() {
        return device.Get();
    }

    operator ID3D12Device* () const {
        return device.Get();
    }

private:
    ComPtr<ID3D12Device> device;
    friend class bnGraphicsD3D12;
};

class DeviceContextD3D12 : public IDeviceContext {
public:
    // Return native handle (ID3D12GraphicsCommandList*)
    void* GetNativeHandle() override {
        return commandList.Get();
    }

    // Release command list & allocator
    void Release() override {
        commandList.Reset();
        commandAllocator.Reset();
    }

    // Operators for convenience
    ID3D12GraphicsCommandList* operator->() {
        return commandList.Get();
    }

    operator ID3D12GraphicsCommandList* () const {
        return commandList.Get();
    }

    operator ID3D12CommandAllocator* () const {
        return commandAllocator.Get();
    }

private:
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    friend class bnGraphicsD3D12;
};

struct CommandListEntry {
    ID3D12GraphicsCommandList* cmdList;
    ID3D12CommandAllocator* allocator;
    bool inUse = false;
};

class CommandPoolD3D12 : public ICommandPool {
public:
    ID3D12Device* device = nullptr;
    D3D12_COMMAND_LIST_TYPE type;
    std::vector<CommandListEntry> commandLists; // Track both allocator + list

    CommandPoolD3D12(ID3D12Device* dev, D3D12_COMMAND_LIST_TYPE t) 
        : device(dev), type(t) {}

    ID3D12GraphicsCommandList* AllocateCommandList();

    // Return command list to pool after execution
    void FreeCommandList(ID3D12GraphicsCommandList* cmdList) {
        for (auto& entry : commandLists) {
            if (entry.cmdList == cmdList) {
                entry.inUse = false;
                break;
            }
        }
    }

    void Release() {
        for (auto& entry : commandLists) {
            if (entry.cmdList) entry.cmdList->Release();
            if (entry.allocator) entry.allocator->Release();
        }
        commandLists.clear();
    }

    void* GetNativeHandle() override { return nullptr; }
};


class CommandBufferD3D12 : public ICommandBuffer {
public:
    ID3D12GraphicsCommandList* list = nullptr;
    //ID3D12CommandAllocator* allocator = nullptr;
    CommandPoolD3D12* pool = nullptr;
    std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;

    CommandBufferD3D12(ID3D12GraphicsCommandList* cmdList, CommandPoolD3D12* pool = nullptr)
        : list(cmdList), pool(pool) {
    }

    D3D12_RESOURCE_BARRIER BarrierCreator(ITexture* image,
    ImageLayout oldLayout,
    ImageLayout newLayout,
    ImageAccessLayout srcAccessMask,
    ImageAccessLayout dstAccessMask) const;

    void PipelineBarrierBatched(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override;

    void FlushBatchedBarriers() override {
        if (!list) return;

        if (!pendingBarriers.empty()) {
            list->ResourceBarrier(static_cast<UINT>(pendingBarriers.size()), pendingBarriers.data());
            pendingBarriers.clear();
        }
    }

    void PipelineBarrier(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override;


    void Release() {
        pool->FreeCommandList(list);
    }

    void* GetNativeHandle() override {
        return list;
    }

    ID3D12GraphicsCommandList* operator->() { return list; }
};

class bnGraphicsD3D12 : IGraphicsDeviceExplicit
{
public:
    IGraphicsDeviceConfig& config;
    bnGraphicsD3D12(SysHandle& handle, IGraphicsDeviceConfig& config);

    const char* GetAPIName() const {
        return "D3D12";
    }

    uint32_t GetAPIVersion() const {
        return 120;
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
    void ReleaseDescriptorPool(IDescriptorPool** pool) override;
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
        return &deviceContexts[currentFrame];
        return nullptr;
    }
    IDevice* getDevice() override {
        return &device;
        return nullptr;
    }

    void WaitForNewFrame();
    ID3D12CommandQueue* GetCommandQueue();
    IDXGIAdapter1* GetAdapter();
    ICommandBuffer* BeginSingleTimeCommands(ICommandPool* pool) override;
    void EndSingleTimeCommands(ICommandBuffer* buffer) override;
    void CopyToBuffer(IBuffer* buffer, ICommandBuffer* pool, void* data, size_t size) override;
    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;

    ITexture* GetSwapchainImage() override;
    long width = 0;
    long height = 0;
    uint32_t imageIndex = 0;
    size_t currentFrame = 0;
    void ClearPendingReleases() override;
    void WaitTillImFree() override;

    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF) override;
private:
    ITexture* GetSwapchainImageFrame(size_t frame);
    void WaitForFence(int frameIndex);
    sVec<ComPtr<ID3D12Resource>> swapChainBuffers;
    ICommandPool* copyPool;
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateFrameBuffers();
    //u32 FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    SysHandle& handle;
    ComPtr<IDXGIFactory6> factory;
    sVec<DeviceContextD3D12> deviceContexts;
    ComPtr<ID3D12CommandQueue> commandQueue;
    DeviceD3D12 device;
    ComPtr<IDXGISwapChain3> swapChain;
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    sVec<ComPtr<ID3D12Fence>> fence;
    sVec<UINT64> fenceValues;
    HANDLE fenceEvent = nullptr;
    TextureFormat swapChainFormat;
    RenderTargetD3D12* renderTarget = nullptr;
    sVec<ITexture*> msaaSWPCHTexture;
    ITexture* depthTexture = nullptr;
    IDepthStencil* depthStencil = nullptr;

    //HeapD3D12* samplerHeap = nullptr;
    sVec<void*> pendVoids;
    sVec<IBuffer**> bufferRelease;
    sVec<ITexture**> textureRelease;
    sVec<IShader**> shaderRelease;
    sVec<ICommandPool**> poolRelease;
    sVec<IDescriptorPool**> descriptorPoolRelease;
    sVec<IDescriptorSetLayout**> descriptorSetLayoutRelease;
    sVec<CommandBufferD3D12*> cbRelease;
    sVec<PipelineD3D12*> pipelineRelease;
    sVec<CommandListD3D12*> commandLists;
    sVec<PendingDrawD3D12> pDraws;
    sVec<CommandListD3D12*> releaseCommandBuffers;

    sVec<CommandBufferD3D12*> SingleTimeCommandBuffers;
    bool frameDone;
    ComPtr<ID3D12Fence> sFence;
    UINT64 sFenceValue = 0;
    HANDLE sFenceEvent;
    ComPtr<IDXGIAdapter1> adapter = nullptr;
    friend class SkiaD3D12Renderer;
};
