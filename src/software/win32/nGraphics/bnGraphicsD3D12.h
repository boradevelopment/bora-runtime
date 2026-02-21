#include "nGraphics/ExplicitGraphicsAbstract.h"
#ifdef WIN32
#include "d3d12.h"
#include "D3dx12.h"
#include "dxgi.h"
#include "dxgi1_4.h"
#include <wrl/client.h>
#include <comdef.h>
#include <dxgi1_6.h>
#endif
#include <optional>
#include <stdexcept>
#include "nGraphics/GraphicsUtilities.h"
using namespace Microsoft::WRL;

class TextureD3D12 : public ITexture {
public:
    ID3D12Resource* resource = nullptr; // The actual texture
    ID3D12DescriptorHeap* srvHeap = nullptr; // Shader Resource View heap (optional)
    ID3D12DescriptorHeap* samplerHeap = nullptr; // Sampler heap (optional)
  //  bool owner = true;
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
    RenderTargetD3D12& operator=(RenderTargetD3D12&& other) noexcept {
        if (this != &other) {
            // Move ownership of resources
            colorTargets = std::move(other.colorTargets);
            depth = other.depth;
            rtvHeap = other.rtvHeap;
            dsvHeap = other.dsvHeap;
            rtvDescriptorSize = other.rtvDescriptorSize;
            clearColors = std::move(other.clearColors);
            depthClear = other.depthClear;
            stencilClear = other.stencilClear;
            width = other.width;
            height = other.height;
            mipLevels = other.mipLevels;

            // Reset other's state
            other.depth = nullptr;
            other.rtvHeap = nullptr;
            other.dsvHeap = nullptr;
            other.rtvDescriptorSize = 0;
            other.depthClear = 1.0f;
            other.stencilClear = 0;
            other.width = 0;
            other.height = 0;
            other.mipLevels = 1;
        }
        return *this;
    }

    // Destructor (no ownership of COM objects assumed here)
    ~RenderTargetD3D12() = default;

    // Return a native handle (optional, can be RTV heap)
    void* GetNativeHandle() override {
        return rtvHeap;
    }

    // Release resources (optional, if using ComPtr they auto-release)
    void Release() override {
        if (rtvHeap) rtvHeap->Release();
        if (dsvHeap) dsvHeap->Release();
        
        for (auto tex : colorTargets) {
            tex->Release();
            delete tex;
            tex = nullptr;
        }
        depth = nullptr;
    }
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

    InputLayoutD3D12(const InputLayoutDesc& desc) {
        elements.reserve(desc.elements.size());
        semanticNames.reserve(desc.elements.size());

        for (const auto& attrib : desc.elements) {
            semanticNames.push_back(attrib.semanticName); // copy string

            D3D12_INPUT_ELEMENT_DESC el = {};
            el.SemanticName = semanticNames.back().c_str(); // pointer into owned storage
            el.SemanticIndex = attrib.semanticIndex;
            el.Format = ToDXGIFormat(attrib.type);
            el.InputSlot = attrib.inputSlot;
            el.AlignedByteOffset = (UINT)attrib.offset;
            el.InputSlotClass = attrib.perInstance
                ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            el.InstanceDataStepRate = attrib.perInstance ? 1 : 0;

            elements.push_back(el);
        }
    }

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
        // Samplers live in descriptor heaps, so nothing to destroy directly
        // Just let the heap manage lifetime

        delete this;
    }
};

struct ViewPortD3D12 : public IViewPort {
    D3D12_VIEWPORT viewport;
    D3D12_RECT  scissorRect;

    ViewPortD3D12(const ViewPortDesc& desc) {
        viewport.TopLeftX = desc.viewport->x;
        viewport.TopLeftY = desc.viewport->y;
        viewport.Width = static_cast<float>(desc.viewport->width);
        viewport.Height = static_cast<float>(desc.viewport->height);
        viewport.MinDepth = desc.viewport->minDepth;
        viewport.MaxDepth = desc.viewport->maxDepth;

        scissorRect.left = static_cast<LONG>(desc.viewport->x);
        scissorRect.top = static_cast<LONG>(desc.viewport->y);
        scissorRect.right = static_cast<LONG>(desc.viewport->x + desc.viewport->width);
        scissorRect.bottom = static_cast<LONG>(desc.viewport->y + desc.viewport->height);
    }

    void* GetNativeHandle() override {
        return &viewport; // Return pointer to D3D12_VIEWPORT
    }
};

struct RasterizerStateD3D12 : public IRasterizerState {
    D3D12_RASTERIZER_DESC desc;

    RasterizerStateD3D12(const RasterizerDesc& src) {
        desc.FillMode = src.fillMode == IFillMode::Solid ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
        desc.CullMode = src.cullMode == CullMode::Back ? D3D12_CULL_MODE_BACK :
            src.cullMode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_NONE;
        desc.FrontCounterClockwise = src.frontCounterClockwise;
        desc.DepthBias = src.depthBias;
        desc.DepthBiasClamp = src.depthBiasClamp;
        desc.SlopeScaledDepthBias = src.slopeScaledDepthBias;
        desc.DepthClipEnable = src.depthClipEnable;
        desc.MultisampleEnable = src.multisampleEnable;
        desc.AntialiasedLineEnable = src.antialiasedLineEnable;
        desc.ForcedSampleCount = 0;
        desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }

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

    DepthStencilStateD3D12(const DepthStencilDesc& src) {
        desc.DepthEnable = src.depthEnable;
        desc.DepthWriteMask = src.depthWriteMask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = ConvertComparisonFunc(src.depthFunc);

        desc.StencilEnable = src.stencilEnable;
        desc.StencilReadMask = src.stencilReadMask;
        desc.StencilWriteMask = src.stencilWriteMask;

        desc.FrontFace.StencilFailOp      = ConvertStencilOp(src.frontFace.failOp);
        desc.FrontFace.StencilDepthFailOp = ConvertStencilOp(src.frontFace.depthFailOp);
        desc.FrontFace.StencilPassOp      = ConvertStencilOp(src.frontFace.passOp);
        desc.FrontFace.StencilFunc        = ConvertComparisonFunc(src.frontFace.func);

        desc.BackFace.StencilFailOp      = ConvertStencilOp(src.backFace.failOp);
        desc.BackFace.StencilDepthFailOp = ConvertStencilOp(src.backFace.depthFailOp);
        desc.BackFace.StencilPassOp      = ConvertStencilOp(src.backFace.passOp);
        desc.BackFace.StencilFunc        = ConvertComparisonFunc(src.backFace.func);
    }

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

    BlendStateD3D12(const BlendStateDesc& src) {
        desc.AlphaToCoverageEnable = src.alphaToCoverageEnable ? TRUE : FALSE;
        desc.IndependentBlendEnable = src.independentBlendEnable ? TRUE : FALSE;

        for (size_t i = 0; i < 8; i++) {
            D3D12_RENDER_TARGET_BLEND_DESC& rt = desc.RenderTarget[i];
            const auto& rtsrc = src.renderTarget[i];

            rt.BlendEnable = rtsrc.blendEnable ? TRUE : FALSE;
            rt.LogicOpEnable = FALSE;  // <-- MUST initialize
            rt.SrcBlend = ConvertBlend(rtsrc.srcBlend);
            rt.DestBlend = ConvertBlend(rtsrc.destBlend);
            rt.BlendOp = ConvertBlendOp(rtsrc.blendOp);
            rt.SrcBlendAlpha = ConvertBlend(rtsrc.srcBlendAlpha);
            rt.DestBlendAlpha = ConvertBlend(rtsrc.destBlendAlpha);
            rt.BlendOpAlpha = ConvertBlendOp(rtsrc.blendOpAlpha);
            rt.LogicOp = D3D12_LOGIC_OP_NOOP;   // <-- safe default
            rt.RenderTargetWriteMask = rtsrc.renderTargetWriteMask;
        }
    }

    void* GetNativeHandle() override {
        return &desc;
    }

    void Release() override {
        delete this;
    }
};

//struct DescriptorHeapBlock {
//    ComPtr<ID3D12DescriptorHeap> heap;
//    UINT size = 0;
//    UINT used = 0;
//    bool inUse = false;
//    UINT64 frameFence = 0;
//};
//
//class HeapResourceManager {
//public:
//    HeapResourceManager(ID3D12Device* device) : device(device) {}
//
//    DescriptorHeapBlock* AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT requestedSize) {
//        auto& heaps = (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) ? samplerHeaps : srvCbvUavHeaps;
//
//        // Reuse free block
//        for (auto& block : heaps) {
//            if (!block->inUse && block->size >= requestedSize) {
//                block->inUse = true;
//                block->used = requestedSize;
//                return block.get();
//            }
//        }
//
//        // Make new one
//        UINT heapSize = std::max(requestedSize, 16u);
//        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//        desc.Type = type;
//        desc.NumDescriptors = heapSize;
//        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//
//        auto block = std::make_unique<DescriptorHeapBlock>();
//        block->size = heapSize;
//        block->used = requestedSize;
//        block->inUse = true;
//
//        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&block->heap))))
//            throw std::runtime_error("Failed to create descriptor heap");
//
//        heaps.push_back(std::move(block));
//        return heaps.back().get();
//    }
//
//    void FreeHeap(DescriptorHeapBlock* block) {
//        block->inUse = false;
//        block->used = 0;
//    }
//
//private:
//    ID3D12Device* device;
//    std::vector<std::unique_ptr<DescriptorHeapBlock>> srvCbvUavHeaps;
//    std::vector<std::unique_ptr<DescriptorHeapBlock>> samplerHeaps;
//};

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

    DescriptorPoolD3D12(ID3D12Device* dev, const DescriptorPoolDesc& desc) : device(dev), desc(desc) {
        // Calculate total descriptors for CBV/SRV/UAV and Sampler separately
        CreateResources();
    }

    void CreateResources() {
        UINT totalCBVSRVUAV = 0;
        UINT totalSamplers = 0;

        for (auto& ps : desc.poolSizes) {
            switch (ps.first) {
            case DescriptorType::UniformBuffer:
            case DescriptorType::StorageBuffer:
            case DescriptorType::CombinedImageSampler:
                totalCBVSRVUAV += ps.second;
                totalSamplers += ps.second;
                break;
            case DescriptorType::StorageImage:
                totalCBVSRVUAV += ps.second;
                break;
            case DescriptorType::Sampler:
                totalSamplers += ps.second;
                break;
            }
        }

        if (totalCBVSRVUAV > 0) {
            //cbvSrvUavHeap = manager->AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, totalCBVSRVUAV);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = totalCBVSRVUAV;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvSrvUavHeap));
            cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        if (totalSamplers > 0) {
            //samplerHeap = manager->AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, totalCBVSRVUAV);
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = totalSamplers;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&samplerHeap));
            samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate(DescriptorType type) {
        if (type == DescriptorType::Sampler) {
            if (!samplerHeap) throw std::runtime_error("No sampler heap allocated!");
         /*   if (samplerHeap->size < samplerAllocated) {
                throw std::runtime_error("No memory!");
            }*/
            D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += samplerAllocated++ * samplerDescriptorSize;
            return handle;
        }
        else {
            //if (!cbvSrvUavHeap) throw std::runtime_error("No CBV/SRV/UAV heap allocated!");
            D3D12_CPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += cbvSrvUavAllocated++ * cbvSrvUavDescriptorSize;
            return handle;
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(DescriptorType type, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
        UINT offset = 0;
        if (type == DescriptorType::Sampler) {
            offset = static_cast<UINT>((cpuHandle.ptr - samplerHeap->GetCPUDescriptorHandleForHeapStart().ptr) / samplerDescriptorSize);
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = samplerHeap->GetGPUDescriptorHandleForHeapStart();
            gpuHandle.ptr += offset * samplerDescriptorSize;
            return gpuHandle;
        }
        else {
            offset = static_cast<UINT>((cpuHandle.ptr - cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr) / cbvSrvUavDescriptorSize);
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
            gpuHandle.ptr += offset * cbvSrvUavDescriptorSize;
            return gpuHandle;
        }
    }

    ID3D12DescriptorHeap* GetHeap(DescriptorType type) const {
        return type == DescriptorType::Sampler ? samplerHeap : cbvSrvUavHeap;
    }

    void ClearAllocations() {
     /*   if (cbvSrvUavHeap) manager->FreeHeap(cbvSrvUavHeap);
        if (samplerHeap) manager->FreeHeap(cbvSrvUavHeap);*/
        cbvSrvUavAllocated = 0;
        samplerAllocated = 0;
    }

    void FreeResources() {
        if (cbvSrvUavHeap) {
            cbvSrvUavHeap->Release();
            cbvSrvUavHeap = nullptr;
        }
        if (samplerHeap) {
            samplerHeap->Release();
            samplerHeap = nullptr;
        }
        cbvSrvUavAllocated = 0;
        samplerAllocated = 0;
    }

    void Release() {
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

    DescriptorSetLayoutD3D12(ID3D12Device* device, const DescriptorSetLayoutDesc& desc) : desc(desc) {
        std::vector<D3D12_ROOT_PARAMETER> rootParams;
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges;

        auto size = desc.bindings.size();

        for (auto& binding : desc.bindings) {
            if (binding.type == DescriptorType::CombinedImageSampler) {
                size++;
            }
        }

        rootParams.resize(size);

        for (auto& binding : desc.bindings) {
            switch (binding.type) {
            case DescriptorType::UniformBuffer: {
                allRanges.emplace_back(); // add a new vector
                auto& srvVec = allRanges.back();
                srvVec.push_back({});
                auto& range = srvVec.back();
                //D3D12_DESCRIPTOR_RANGE range = {};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                range.NumDescriptors = binding.count;
                range.BaseShaderRegister = binding.binding;
                range.RegisterSpace = 0;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = 1;
                param.DescriptorTable.pDescriptorRanges = &range;
                param.ShaderVisibility = ConvertShaderStage(binding.stageFlags);

              //  descriptorRanges.push_back(range);
                rootParams[binding.binding] = param;
                break;
            }

            case DescriptorType::CombinedImageSampler: {
                // Have to make both since D3D12 doesnt do uniform CBS
                allRanges.emplace_back(); // add a new vector
                auto& srvVec = allRanges.back();
                srvVec.push_back({});
                auto& srvRange = srvVec.back();
               // D3D12_DESCRIPTOR_RANGE srvRange = {};
                srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                srvRange.NumDescriptors = binding.count;
                srvRange.BaseShaderRegister = binding.binding; // t0
                srvRange.RegisterSpace = 0;
                srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                D3D12_ROOT_PARAMETER srvParam = {};
                srvParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                srvParam.DescriptorTable.NumDescriptorRanges = 1;
                srvParam.DescriptorTable.pDescriptorRanges = &srvRange;
                srvParam.ShaderVisibility = ConvertShaderStage(binding.stageFlags);

             //   descriptorRanges.push_back(srvRange);
                rootParams[binding.binding] = srvParam;

                allRanges.emplace_back();
                auto& sampVec = allRanges.back();
                sampVec.push_back({});
                auto& samplerRange = sampVec.back();
               // D3D12_DESCRIPTOR_RANGE samplerRange = {};
                samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                samplerRange.NumDescriptors = binding.count;
                samplerRange.BaseShaderRegister = binding.binding; // s0
                samplerRange.RegisterSpace = 0;
                samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                D3D12_ROOT_PARAMETER samplerParam = {};
                samplerParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                samplerParam.DescriptorTable.NumDescriptorRanges = 1;
                samplerParam.DescriptorTable.pDescriptorRanges = &samplerRange;
                samplerParam.ShaderVisibility = ConvertShaderStage(binding.stageFlags);

                //descriptorRanges.push_back(samplerRange);
                rootParams[binding.binding+1] = samplerParam;

                break;
            }

            case DescriptorType::Sampler: {
                allRanges.emplace_back();
                auto& sampVec = allRanges.back();
                sampVec.push_back({});
                auto& samplerRange = sampVec.back();
               // D3D12_DESCRIPTOR_RANGE samplerRange = {};
                samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                samplerRange.NumDescriptors = binding.count;
                samplerRange.BaseShaderRegister = binding.binding;
                samplerRange.RegisterSpace = 0;
                samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                D3D12_ROOT_PARAMETER samplerParam = {};
                samplerParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                samplerParam.DescriptorTable.NumDescriptorRanges = 1;
                samplerParam.DescriptorTable.pDescriptorRanges = &samplerRange;
                samplerParam.ShaderVisibility = ConvertShaderStage(binding.stageFlags);

               // descriptorRanges.push_back(samplerRange);
                rootParams[binding.binding] = samplerParam;
                break;
            }

            case DescriptorType::StorageBuffer: {
                allRanges.emplace_back();
                auto& sampVec = allRanges.back();
                sampVec.push_back({});
                auto& uavRange = sampVec.back();;
                //D3D12_DESCRIPTOR_RANGE uavRange = {};
                uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                uavRange.NumDescriptors = binding.count;
                uavRange.BaseShaderRegister = binding.binding;
                uavRange.RegisterSpace = 0;
                uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable.NumDescriptorRanges = 1;
                param.DescriptorTable.pDescriptorRanges = &uavRange;
                param.ShaderVisibility = ConvertShaderStage(binding.stageFlags);

               // descriptorRanges.push_back(uavRange);
                rootParams[binding.binding] = param;
                break;
            }

            default:
                continue;
            }
        }

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = static_cast<UINT>(rootParams.size());
        rootDesc.pParameters = rootParams.data();
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signatureBlob;
        ComPtr<ID3DBlob> errorBlob;

        if (FAILED(D3D12SerializeRootSignature(&rootDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signatureBlob,
            &errorBlob))) {
            if (errorBlob) {
                std::string errMsg((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
                throw std::runtime_error("Failed to serialize root signature: " + errMsg);
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        if (FAILED(device->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)))) {
            throw std::runtime_error("Failed to create root signature");
        }
    }

    void* GetNativeHandle() override {
        return rootSignature;
    }

    void Release() {
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

    DescriptorSetD3D12(ID3D12Device* dev, DescriptorPoolD3D12* p, DescriptorSetLayoutD3D12* layout)
        : device(dev), pool(p)
    {
        bindings = layout->desc.bindings;
        slotCount = 0;

        // Count total slots
        for (auto& b : bindings) {
            if (b.type == DescriptorType::CombinedImageSampler) {
                slotCount += 2;
            } else slotCount += b.count;
        }

        cpuHandles.resize(slotCount);
        gpuHandles.resize(slotCount);

        // Allocate descriptors from the pool based on binding type
        for (auto& b : bindings) {
            for (uint32_t i = 0; i < b.count; ++i) {
                // Compute corresponding GPU handle
                D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
                if(b.type != DescriptorType::CombinedImageSampler && b.type != DescriptorType::Sampler) {
                    D3D12_CPU_DESCRIPTOR_HANDLE cpu = pool->Allocate(b.type);
                    cpuHandles[b.binding] = cpu;
                    gpu.ptr = pool->cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr +
                        (cpu.ptr - pool->cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);

                    slotsB[b.type] = {
                        .normalSlot = static_cast<u32>(b.binding)
                    };

                    gpuHandles[b.binding] = gpu;
                }
                else if (b.type == DescriptorType::CombinedImageSampler) {
                    D3D12_CPU_DESCRIPTOR_HANDLE cpu = pool->Allocate(DescriptorType::StorageImage);
                    cpuHandles[b.binding] = cpu;
                    gpu.ptr = pool->cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr +
                        (cpu.ptr - pool->cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart().ptr);
                    gpuHandles[b.binding] = gpu;

                    D3D12_CPU_DESCRIPTOR_HANDLE cpu1 = pool->Allocate(DescriptorType::Sampler);
                    cpuHandles[b.binding+1] = cpu1;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpu1 = {};
                    gpu1.ptr = pool->samplerHeap->GetGPUDescriptorHandleForHeapStart().ptr +
                        (cpu1.ptr - pool->samplerHeap->GetCPUDescriptorHandleForHeapStart().ptr);

                    gpuHandles[b.binding+1] = gpu1;

                    slotsB[b.type] = {
                        .normalSlot = static_cast<u32>(b.binding),
                        .samplerSlot = static_cast<u32>(b.binding+1)
                    };
                } else if (b.type == DescriptorType::Sampler) {
                    D3D12_CPU_DESCRIPTOR_HANDLE cpu = pool->Allocate(b.type);
                    cpuHandles[b.binding] = cpu;
                    gpu.ptr = pool->samplerHeap->GetGPUDescriptorHandleForHeapStart().ptr +
                        (cpu.ptr - pool->samplerHeap->GetCPUDescriptorHandleForHeapStart().ptr);

                    slotsB[b.type] = {
                        .samplerSlot = static_cast<u32>(b.binding)
                    };

                    gpuHandles[b.binding] = gpu;
                }
                
                
            }
        }
    }

    void SetTexture(u32 slot, ITexture* tex) override {
        if (!tex || slot >= cpuHandles.size()) return;

        auto texture = dynamic_cast<TextureD3D12*>(tex);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = ToDXGIFormat(tex->desc.format); // your wrapper function
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = tex->desc.mipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;

        auto it = std::find_if(bindings.begin(), bindings.end(), [=](const DescriptorSetLayoutBindingDesc& obj) {
            return obj.binding == slot; // your condition
            });

        if (it == bindings.end()) return;

        if (it->type == DescriptorType::CombinedImageSampler) {
            slot = slotsB[DescriptorType::CombinedImageSampler].normalSlot;
        }

        device->CreateShaderResourceView(texture->resource, &srvDesc, cpuHandles[slot]);
    }

    void SetBuffer(u32 slot, IBuffer* buf) override {
        if (!buf || slot >= cpuHandles.size()) return;


        auto buffer = dynamic_cast<BufferD3D12*>(buf);

        auto it = std::find_if(bindings.begin(), bindings.end(), [=](const DescriptorSetLayoutBindingDesc& obj) {
            return obj.binding == slot; // your condition
            });

        if (it == bindings.end()) return;

       slot = slotsB[DescriptorType::UniformBuffer].normalSlot;

        if (buffer->type == BufferType::Constant) {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            cbvDesc.BufferLocation = buffer->resource->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (static_cast<UINT>(buffer->desc.size) + 255) & ~255;
            device->CreateConstantBufferView(&cbvDesc, cpuHandles[slot]);
        }
        else if (buffer->type == BufferType::Storage) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.NumElements = buffer->desc.size / buffer->desc.stride;
            uavDesc.Buffer.StructureByteStride = buffer->desc.stride;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;

            device->CreateUnorderedAccessView(buffer->resource, nullptr, &uavDesc, cpuHandles[slot]);
        }
    }

    void SetSampler(u32 slot, ISamplerState* sampler) override {
        if (!sampler || slot >= cpuHandles.size()) return;

        auto it = std::find_if(bindings.begin(), bindings.end(), [=](const DescriptorSetLayoutBindingDesc& obj) {
            return obj.binding == slot; // your condition
            });

        if (it == bindings.end()) return;

        if (it->type == DescriptorType::CombinedImageSampler) {
            slot = slotsB[DescriptorType::CombinedImageSampler].samplerSlot;
        }

        auto samplerD3D = dynamic_cast<SamplerStateD3D12*>(sampler);

        D3D12_SAMPLER_DESC desc = samplerD3D->desc;
        device->CreateSampler(&desc, cpuHandles[slot]);
    }

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
    // Optional: store descriptor sets per slot
    std::unordered_map<u32, DescriptorSetD3D12*> descriptorSets;
    RenderTargetD3D12* d3dRenderTarget;
    PipelineD3D12(sVec<PipelineD3D12*>* pipe, ID3D12Device* device, DescriptorPoolD3D12* pool, DescriptorSetLayoutD3D12* layout, RenderTargetD3D12* target,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc,
        const D3D12_ROOT_SIGNATURE_DESC& rootDesc) : releasePipelines(pipe), device(device), descriptorPool(pool), descriptorLayout(layout), d3dRenderTarget(target)
    {
     
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = psoDesc;
        desc.pRootSignature = layout->rootSignature;
        desc.SampleDesc.Count = target->colorTargets[0]->desc.samples;

        rootSignature = layout->rootSignature;
          
        if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)))) {
            throw std::runtime_error("Failed to create pipeline state!");
        }
    }

    void* GetNativeHandle() override {
        return pso;
    }

    void Release() override {
        releasePipelines->push_back(this);
        //pso->Release(); pso = nullptr;
        //rootSignature->Release(); rootSignature = nullptr;
        //for (auto& [slot, set] : descriptorSets) {
        //    set->Release();
        //}
        //descriptorSets.clear(); 
    }

    IDescriptorSet* CreateDescriptorSet(u32 slot = 0) override {
        if (!descriptorPool) return nullptr; // make sure a heap/pool exists

        // Create the D3D12 descriptor set wrapper
        auto set = new DescriptorSetD3D12(
            device,
            descriptorPool, descriptorLayout
        );

        // Store in unordered_map by slot
        descriptorSets[slot] = set;

        return set;
    }

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

    IPipelineBuilder& AddShader(IShader* shader) override {
        shaders.push_back(shader);
        return *this;
    }

    IPipelineBuilder& SetInputLayout(IInputLayout* layout) override {
        inputLayout = layout;
        return *this;
    }

    IPipelineBuilder& SetRenderTarget(IRenderTarget* target) override {
        renderTarget = target;
        return *this;
    }

    IPipelineBuilder& SetRasterizer(IRasterizerState* raster) override {
        rasterizer = raster;
        return *this;
    }

    IPipelineBuilder& SetDepthStencil(IDepthStencilState* depth) override {
        depthStencil = depth;
        return *this;
    }

    IPipelineBuilder& SetBlendState(IBlendState* blend) override {
        blendState = blend;
        return *this;
    }

    IPipelineBuilder& SetDescriptorPool(IDescriptorPool* pool) override {
        auto dPool = dynamic_cast<DescriptorPoolD3D12*>(pool);

        descriptorPool = dPool;
        return *this;
    }

    IPipelineBuilder& SetDescriptorSetLayout(IDescriptorSetLayout* layout) override {
        auto dlayout = dynamic_cast<DescriptorSetLayoutD3D12*>(layout);

        descriptorLayout = dlayout;
        return *this;
    }

    sVec<IShader*>* GetShaders() override {
        return &shaders;
    }

    IPipeline* Build() override {
        if (!device) return nullptr;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        ZeroMemory(&psoDesc, sizeof(psoDesc));

        // Shaders
        for (auto shader : shaders) {
            ShaderD3D12* s = dynamic_cast<ShaderD3D12*>(shader);
            if (!s) continue;
            switch (s->type) {
            case ShaderDesc::Type::Vertex:   psoDesc.VS = { s->binary.data(), s->binary.size()}; break;
            case ShaderDesc::Type::Pixel:    psoDesc.PS = { s->binary.data(), s->binary.size() }; break;
            case ShaderDesc::Type::Geometry: psoDesc.GS = { s->binary.data(), s->binary.size() }; break;
            case ShaderDesc::Type::TessControl: psoDesc.HS = { s->binary.data(), s->binary.size() }; break;
            case ShaderDesc::Type::TessEval:    psoDesc.DS = { s->binary.data(), s->binary.size() }; break;
            default: break;
            }
        }

        // Input layout
        if (inputLayout) {
            InputLayoutD3D12* il = dynamic_cast<InputLayoutD3D12*>(inputLayout);
            psoDesc.InputLayout = { il->elements.data(), (UINT)il->elements.size() };
        }

        // Rasterizer
        if (rasterizer) {
            RasterizerStateD3D12* rs = dynamic_cast<RasterizerStateD3D12*>(rasterizer);
            psoDesc.RasterizerState = rs->desc;
        }

        // Blend
        if (blendState) {
            BlendStateD3D12* bs = dynamic_cast<BlendStateD3D12*>(blendState);
            psoDesc.BlendState = bs->desc;
        }

        // Depth/stencil
        if (depthStencil) {
            DepthStencilStateD3D12* ds = dynamic_cast<DepthStencilStateD3D12*>(depthStencil);
            psoDesc.DepthStencilState = ds->desc;
        }

        // Render target formats
        if (renderTarget) {
            RenderTargetD3D12* rt = dynamic_cast<RenderTargetD3D12*>(renderTarget);
            for (size_t i = 0; i < rt->colorTargets.size(); i++) {
                TextureD3D12* tex = rt->colorTargets[i];
                psoDesc.RTVFormats[i] = ToDXGIFormat(tex->desc.format);
            }
            psoDesc.NumRenderTargets = (UINT)rt->colorTargets.size();
            if (rt->depth) {
                psoDesc.DSVFormat = ToDXGIFormat(rt->depth->texture->desc.format);
            }
        }

        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.SampleDesc.Count = 1;

        std::vector<CD3DX12_ROOT_PARAMETER> rootParams;

        for (const auto& binding : descriptorLayout->desc.bindings) {
            switch (binding.type) {
            case DescriptorType::UniformBuffer: {
                CD3DX12_ROOT_PARAMETER param;
                param.InitAsConstantBufferView(binding.count, 0, D3D12_SHADER_VISIBILITY_ALL);
                rootParams.push_back(param);
                break;
            }

            case DescriptorType::StorageBuffer: {
                CD3DX12_DESCRIPTOR_RANGE range;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, binding.count, 0);
                CD3DX12_ROOT_PARAMETER param;
                param.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);
                rootParams.push_back(param);
                break;
            }

            case DescriptorType::CombinedImageSampler:
            case DescriptorType::StorageImage: {
                CD3DX12_DESCRIPTOR_RANGE range;
                range.Init(
                    binding.type == DescriptorType::CombinedImageSampler ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                    1,
                    binding.count,
                    0
                );
                CD3DX12_ROOT_PARAMETER param;
                param.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);
                rootParams.push_back(param);
                break;
            }

            case DescriptorType::Sampler: {
                CD3DX12_DESCRIPTOR_RANGE samplerRange;
                samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, binding.count, 0);
                CD3DX12_ROOT_PARAMETER param;
                param.InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_ALL);
                rootParams.push_back(param);
                break;
            }
            }
        }

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = static_cast<UINT>(rootParams.size());
        rootDesc.pParameters = rootParams.data();
        rootDesc.NumStaticSamplers = 0;
        rootDesc.pStaticSamplers = nullptr;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        auto pipeline = new PipelineD3D12(pipelineRelease, device, descriptorPool, descriptorLayout, dynamic_cast<RenderTargetD3D12*>(renderTarget), psoDesc, rootDesc);
        delete this;

        return pipeline;
    }
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
      /*  auto d3dPipeline = dynamic_cast<PipelineD3D12*>(pipeline);
        list->SetPipelineState(d3dPipeline->pso.Get());*/
    }

    void BindDescriptorSet(IDescriptorSet* set, uint32_t index = 0) override {
        pDraw.ds = set;
        pDraw.dsIndex = index;
        //auto d3dSet = dynamic_cast<DescriptorSetD3D12*>(set);
        //list->SetGraphicsRootDescriptorTable(index, d3dSet->GetGPUHandle(index));
    }

    void BindViewPort(IViewPort* vp) override {
        pDraw.viewport = vp;
        //auto d3dVp = dynamic_cast<ViewPortD3D12*>(vp);
        //list->RSSetViewports(1, &d3dVp->viewport);
    }

    void BindScissor(IViewPort* vp) override {
        pDraw.scissor = vp;
      /*  auto d3dVp = dynamic_cast<ViewPortD3D12*>(vp);
        list->RSSetScissorRects(1, &d3dVp->scissorRect);*/
    }

    void BindBuffer(IBuffer* buffer) override {
        pDraw.buffer = buffer;
        //auto d3dBuffer = dynamic_cast<BufferD3D12*>(buffer);
        //D3D12_VERTEX_BUFFER_VIEW vbv{};
        //vbv.BufferLocation = d3dBuffer->resource->GetGPUVirtualAddress();
        //vbv.SizeInBytes = static_cast<UINT>(d3dBuffer->desc.size);
        //vbv.StrideInBytes = d3dBuffer->desc.stride;
        //list->IASetVertexBuffers(0, 1, &vbv);
    }

    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override {
        pDraw.vertexCount = vertexCount;
        pDraw.type = type;
        pDraw.vertexOffset = vertexOffset;

        pDraws->push_back(pDraw);

        //list->IASetPrimitiveTopology(ConvertPrimitive(type));
        //list->DrawInstanced(static_cast<UINT>(vertexCount), 1, static_cast<UINT>(vertexOffset), 0);
    }

    void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override {
        pDraw.type = type;
        pDraw.indexBuffer = indexBuffer;
        pDraw.indexCount = indexCount;
        pDraw.indexCount = indexOffset;

        pDraws->push_back(pDraw);

        //auto d3dIndexBuffer = dynamic_cast<BufferD3D12*>(indexBuffer);
        //D3D12_INDEX_BUFFER_VIEW ibv{};
        //ibv.BufferLocation = d3dIndexBuffer->resource->GetGPUVirtualAddress();
        //ibv.Format = DXGI_FORMAT_R32_UINT; // or 16-bit depending on your index buffer
        //ibv.SizeInBytes = static_cast<UINT>(indexCount * d3dIndexBuffer->desc.stride);
        //list->IASetIndexBuffer(&ibv);
        //list->IASetPrimitiveTopology(ConvertPrimitive(type));
        //list->DrawIndexedInstanced(static_cast<UINT>(indexCount), 1, static_cast<UINT>(indexOffset), 0, 0);
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

    ID3D12GraphicsCommandList* AllocateCommandList() {
        // Try to find a free one
        for (auto& entry : commandLists) {
            if (!entry.inUse) {
                entry.inUse = true;
                // Reset allocator and list for new recording
                entry.allocator->Reset();
                entry.cmdList->Reset(entry.allocator, nullptr);
                return entry.cmdList;
            }
        }

        // No free one, create a new allocator + list
        ID3D12CommandAllocator* allocator = nullptr;
        if (FAILED(device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)))) {
            throw std::runtime_error("Failed to create command allocator");
        }

        ID3D12GraphicsCommandList* cmdList = nullptr;
        if (FAILED(device->CreateCommandList(0, type, allocator, nullptr, IID_PPV_ARGS(&cmdList)))) {
            allocator->Release();
            throw std::runtime_error("Failed to create command list");
        }

        // Close immediately (required by D3D12) and reset
        cmdList->Close();
        cmdList->Reset(allocator, nullptr);

        // Track in pool
        commandLists.push_back({ cmdList, allocator, true });

        return cmdList;
    }

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
    ImageAccessLayout dstAccessMask) {
        auto texD3D12 = dynamic_cast<TextureD3D12*>(image);
        if (!texD3D12 || !list) return {};

        D3D12_RESOURCE_STATES oldState = ToD3D12ResourceState(oldLayout, srcAccessMask);
        D3D12_RESOURCE_STATES newState = ToD3D12ResourceState(newLayout, dstAccessMask);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texD3D12->resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = oldState;
        barrier.Transition.StateAfter = newState;

        return barrier;
    }

    void PipelineBarrierBatched(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override {
        auto texD3D12 = dynamic_cast<TextureD3D12*>(image);
        if (!texD3D12 || !list) return;

        auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
        pendingBarriers.push_back(barrier);
    
        image->explicitLayout = newLayout;
    }

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
        ImageAccessLayout dstAccessMask) override
    {
        auto texD3D12 = dynamic_cast<TextureD3D12*>(image);
        if (!texD3D12 || !list) return;
            
        auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
        list->ResourceBarrier(1, &barrier);

        image->explicitLayout = newLayout;
    }

  
    void Release() {
        //if (list) {
        //    list->Release();
        //    list = nullptr;
        //}

        pool->FreeCommandList(list);

        //if (pool) { // not ours
        //    pool->ReleaseAllocator(allocator);
        //}
        //else { // ours
        //    allocator->Release();
        //}
        //if (allocator) {
        //    allocator->Release();
        //    allocator = nullptr;
        //}
    }

    void* GetNativeHandle() override {
        return list;
    }

    ID3D12GraphicsCommandList* operator->() { return list; }
};

//class HeapD3D12 {
//public:
//    ComPtr<ID3D12DescriptorHeap> heap;
//    D3D12_DESCRIPTOR_HEAP_TYPE type;
//    UINT descriptorSize;
//    UINT numDescriptors;
//    UINT allocatedCount = 0;
//
//    // Create a heap
//    HeapD3D12(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE t, UINT maxDescriptors, bool shaderVisible = false)
//        : type(t), numDescriptors(maxDescriptors)
//    {
//        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//        desc.Type = t;
//        desc.NumDescriptors = numDescriptors;
//        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
//        desc.NodeMask = 0;
//
//        if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap))))
//            throw std::runtime_error("Failed to create descriptor heap!");
//
//        descriptorSize = device->GetDescriptorHandleIncrementSize(type);
//    }
//
//    void Release() {
//        heap.Reset();
//        allocatedCount = 0;
//    }
//
//    // Allocate a descriptor and return CPU handle
//    D3D12_CPU_DESCRIPTOR_HANDLE AllocateCPU() {
//        if (allocatedCount >= numDescriptors)
//            throw std::runtime_error("Descriptor heap full!");
//        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
//        handle.ptr += allocatedCount * descriptorSize;
//        allocatedCount++;
//        return handle;
//    }
//
//    // Allocate a descriptor and return GPU handle (for shader-visible heaps)
//    D3D12_GPU_DESCRIPTOR_HANDLE AllocateGPU() {
//        if (allocatedCount >= numDescriptors)
//            throw std::runtime_error("Descriptor heap full!");
//        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap->GetGPUDescriptorHandleForHeapStart();
//        handle.ptr += allocatedCount * descriptorSize;
//        allocatedCount++;
//        return handle;
//    }
//
//    // Reset heap allocations (for transient heaps)
//    void Reset() {
//        allocatedCount = 0;
//    }
//
//    void* GetNativeHandle() {
//        return heap.Get();
//    }
//};


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
