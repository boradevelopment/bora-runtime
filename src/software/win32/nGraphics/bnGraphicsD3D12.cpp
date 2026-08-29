#include "bnGraphicsD3D12.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#include <dxgidebug.h>
#include <cassert>
#include <iostream>
#include "pix3/pix3.h"

RenderTargetD3D12 & RenderTargetD3D12::operator=(RenderTargetD3D12 &&other) noexcept {
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

void RenderTargetD3D12::Release() {
    if (rtvHeap) rtvHeap->Release();
    if (dsvHeap) dsvHeap->Release();

    for (auto tex : colorTargets) {
        tex->Release();
        delete tex;
        tex = nullptr;
    }
    depth = nullptr;
}

InputLayoutD3D12::InputLayoutD3D12(const InputLayoutDesc &desc) {
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

ViewPortD3D12::ViewPortD3D12(const ViewPortDesc &desc) {
    viewport.TopLeftX = desc.viewport->x;
    viewport.TopLeftY = desc.viewport->y;
    viewport.Width = desc.viewport->width;
    viewport.Height = desc.viewport->height;
    viewport.MinDepth = desc.viewport->minDepth;
    viewport.MaxDepth = desc.viewport->maxDepth;

    scissorRect.left = static_cast<LONG>(desc.viewport->x);
    scissorRect.top = static_cast<LONG>(desc.viewport->y);
    scissorRect.right = static_cast<LONG>(desc.viewport->x + desc.viewport->width);
    scissorRect.bottom = static_cast<LONG>(desc.viewport->y + desc.viewport->height);
}

RasterizerStateD3D12::RasterizerStateD3D12(const RasterizerDesc &src) {
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

DepthStencilStateD3D12::DepthStencilStateD3D12(const DepthStencilDesc &src) {
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

BlendStateD3D12::BlendStateD3D12(const BlendStateDesc &src) {
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

DescriptorPoolD3D12::DescriptorPoolD3D12(ID3D12Device *dev, const DescriptorPoolDesc &desc): device(dev), desc(desc) {
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
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = totalCBVSRVUAV;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvSrvUavHeap));
        cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    if (totalSamplers > 0) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = totalSamplers;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&samplerHeap));
        samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorPoolD3D12::Allocate(DescriptorType type) {
    if (type == DescriptorType::Sampler) {
        if (!samplerHeap) throw std::runtime_error("No sampler heap allocated!");
        D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += samplerAllocated++ * samplerDescriptorSize;
        return handle;
    }
    else {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += cbvSrvUavAllocated++ * cbvSrvUavDescriptorSize;
        return handle;
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorPoolD3D12::GetGPUHandle(DescriptorType type,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const {
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

ID3D12DescriptorHeap * DescriptorPoolD3D12::GetHeap(DescriptorType type) const {
    return type == DescriptorType::Sampler ? samplerHeap : cbvSrvUavHeap;
}

void DescriptorPoolD3D12::FreeResources() {
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

DescriptorSetLayoutD3D12::DescriptorSetLayoutD3D12(ID3D12Device *device, const DescriptorSetLayoutDesc &desc): desc(desc) {
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

DescriptorSetD3D12::DescriptorSetD3D12(ID3D12Device *dev, DescriptorPoolD3D12 *p, DescriptorSetLayoutD3D12 *layout): device(dev), pool(p) {
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

void DescriptorSetD3D12::SetTexture(u32 slot, ITexture *tex) {
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

void DescriptorSetD3D12::SetBuffer(u32 slot, IBuffer *buf) {
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

void DescriptorSetD3D12::SetSampler(u32 slot, ISamplerState *sampler) {
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

PipelineD3D12::PipelineD3D12(sVec<PipelineD3D12 *> *pipe, ID3D12Device *device, DescriptorPoolD3D12 *pool,
    DescriptorSetLayoutD3D12 *layout, RenderTargetD3D12 *target, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &psoDesc,
    const D3D12_ROOT_SIGNATURE_DESC &rootDesc): releasePipelines(pipe), device(device), descriptorPool(pool), descriptorLayout(layout), d3dRenderTarget(target) {

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = psoDesc;
    desc.pRootSignature = layout->rootSignature;
    desc.SampleDesc.Count = target->colorTargets[0]->desc.samples;

    rootSignature = layout->rootSignature;

    if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)))) {
        throw std::runtime_error("Failed to create pipeline state!");
    }
}

IDescriptorSet * PipelineD3D12::CreateDescriptorSet(u32 slot) {
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

IPipelineBuilder & PipelineBuilderD3D12::From(const IPipelineBuilder &builder) {
    const auto* src = dynamic_cast<const PipelineBuilderD3D12*>(&builder);

    if (src) {
        this->shaders          = src->shaders;
        this->inputLayout      = src->inputLayout;
        this->renderTarget     = src->renderTarget;
        this->rasterizer       = src->rasterizer;
        this->depthStencil     = src->depthStencil;
        this->blendState       = src->blendState;
        this->descriptorPool   = src->descriptorPool;
        this->descriptorLayout = src->descriptorLayout;
    }

    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::AddShader(IShader *shader) {
    shaders.push_back(shader);
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetInputLayout(IInputLayout *layout) {
    inputLayout = layout;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetRenderTarget(IRenderTarget *target) {
    renderTarget = target;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetRasterizer(IRasterizerState *raster) {
    rasterizer = raster;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetDepthStencil(IDepthStencilState *depth) {
    depthStencil = depth;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetBlendState(IBlendState *blend) {
    blendState = blend;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetDescriptorPool(IDescriptorPool *pool) {
    auto dPool = dynamic_cast<DescriptorPoolD3D12*>(pool);
    descriptorPool = dPool;
    return *this;
}

IPipelineBuilder & PipelineBuilderD3D12::SetDescriptorSetLayout(IDescriptorSetLayout *layout) {
    auto dlayout = dynamic_cast<DescriptorSetLayoutD3D12*>(layout);
    descriptorLayout = dlayout;
    return *this;
}

sVec<IShader *> * PipelineBuilderD3D12::GetShaders() {
    return &shaders;
}

IPipeline * PipelineBuilderD3D12::Build() {
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
    return pipeline;
}

ID3D12GraphicsCommandList * CommandPoolD3D12::AllocateCommandList() {
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

D3D12_RESOURCE_BARRIER CommandBufferD3D12::BarrierCreator(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
                                                          ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) const {
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

void CommandBufferD3D12::PipelineBarrierBatched(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
    ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) {
    auto texD3D12 = dynamic_cast<TextureD3D12*>(image);
    if (!texD3D12 || !list) return;

    auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
    pendingBarriers.push_back(barrier);

    image->explicitLayout = newLayout;
}

void CommandBufferD3D12::PipelineBarrier(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
    ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) {
    auto texD3D12 = dynamic_cast<TextureD3D12*>(image);
    if (!texD3D12 || !list) return;

    auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
    list->ResourceBarrier(1, &barrier);

    image->explicitLayout = newLayout;
}

bnGraphicsD3D12::bnGraphicsD3D12(SysHandle& handle, IGraphicsDeviceConfig& config) : config(config), handle(handle)
{
}

bool bnGraphicsD3D12::Init()
{
    try {
        // Enable debug layer
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
            }
        }
#endif

    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    // Create D3D12 device
    //ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(
        nullptr, // default adapter
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&device.device))))
        return false;

        // todo: add this in the future.
// #ifdef DEBUG_PROCESS
//         ID3D12InfoQueue* pInfoQueue = nullptr;
//         if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&pInfoQueue)))) {
//             // Enable "Break on Severity"
//             pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
//             pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
//             pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE); // Optional: warnings too
//
//             D3D12_MESSAGE_ID hide[] = {
//                 D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
//                 // Add more IDs here as needed
//             };
//             D3D12_INFO_QUEUE_FILTER filter = {};
//             filter.DenyList.NumIDs = _countof(hide);
//             filter.DenyList.pIDList = hide;
//             pInfoQueue->AddStorageFilterEntries(&filter);
//
//             pInfoQueue->Release();
//         }
// #endif

    // Create Command Queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) return false;

    swapChainFormat = TextureFormat::RGBA8_UNorm;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        handle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1)))
        return false;

    // Optionally query IDXGISwapChain3 for frame index etc.

    if (!swapChain1) return false;
    swapChain1.As(&swapChain);

    config.framesInFlight--;

    swapChainBuffers.resize(config.framesInFlight);
    fence.resize(config.framesInFlight);
    msaaSWPCHTexture.resize(config.framesInFlight);
    //renderTargets.resize(config.framesInFlight);
    deviceContexts.resize(config.framesInFlight);
    commandLists.resize(config.framesInFlight);
    fenceValues.resize(config.framesInFlight, 0);
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr) {
        throw std::runtime_error("Failed to create fence event!");
    }

    // Create fences
    for (UINT i = 0; i < config.framesInFlight; i++) {
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i])))) {
            throw std::runtime_error("Failed to create D3D12 fence!");
        }
        fenceValues[i] = 0; // initial fence value
    }
    copyPool = CreateCommandPool({ 0, true, true, CommandPoolDesc::Type::DIRECT });


    for (UINT i = 0; i < config.framesInFlight; i++)
    {
        DeviceContextD3D12& ctx = deviceContexts[i];

        // Create a command allocator for this frame
        HRESULT hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&ctx.commandAllocator)
        );
        if (FAILED(hr)) throw std::runtime_error("Failed to create command allocator");

        // Create a command list for this frame
        hr = device->CreateCommandList(
            0,                       // Node mask
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            ctx.commandAllocator.Get(), // Associated allocator
            nullptr,                 // Initial pipeline state
            IID_PPV_ARGS(&ctx.commandList)
        );
        if (FAILED(hr)) throw std::runtime_error("Failed to create command list");



        // Command list must be closed initially
        ctx.commandList->Close();
    }

    sFenceValue = 0;
    sFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&sFence));

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (SUCCEEDED(device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
    {
        if (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
        {
            std::cout << "DXR supported, Tier: " << options5.RaytracingTier << std::endl;
        }
    }
    std::cout << "DXR not supported." << std::endl;

    //heapManager = new HeapResourceManager(device.device.Get());

    //samplerHeap = new HeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024, true);

    //dsvHeap = new HeapD3D12(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024, true);
    return true;
    }
    catch (...) {
        return false;
    }
}

ITexture* bnGraphicsD3D12::GetSwapchainImageFrame(size_t frame)
{
    if (frame >= swapChainBuffers.size()) return nullptr;

    auto tex = new TextureD3D12(false);
    tex->resource = swapChainBuffers[frame].Get();
    tex->explicitLayout = ImageLayout::Present;
    tex->desc.width = width;
    tex->desc.height = height;
    tex->desc.format = swapChainFormat;

    return tex;
}

void bnGraphicsD3D12::WaitForNewFrame() {
    WaitForFence(currentFrame);
}

ID3D12CommandQueue* bnGraphicsD3D12::GetCommandQueue() {
    return commandQueue.Get();
}

IDXGIAdapter1 * bnGraphicsD3D12::GetAdapter() {
    if (device && !adapter) {
        // 1. Get device LUID
        LUID deviceLuid = device->GetAdapterLuid();

        // 2. Create DXGI factory
        ComPtr<IDXGIFactory4> factory;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        // 3. Enumerate adapter
        for (UINT i = 0;
             factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (memcmp(&desc.AdapterLuid, &deviceLuid, sizeof(LUID)) == 0)
            {
                break; // Found correct adapter
            }
        }

       return adapter.Get();
    }

    return adapter.Get();
}

void bnGraphicsD3D12::WaitForFence(int frameIndex)
{
    // Signal the fence with the current fence value
    const UINT64 currentFenceValue = fenceValues[frameIndex];
    HRESULT hr = commandQueue->Signal(fence[frameIndex].Get(), currentFenceValue);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to signal fence!");
    }

    // Wait until the fence has been completed
    if (fence[frameIndex]->GetCompletedValue() < currentFenceValue) {
        hr = fence[frameIndex]->SetEventOnCompletion(currentFenceValue, fenceEvent);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to set event on fence completion!");
        }

        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Increment fence value for the next frame
    fenceValues[frameIndex]++;
}

ICommandList* bnGraphicsD3D12::GetCommandList()
{
    // Ensure the allocator for this frame exists
    if (!deviceContexts[currentFrame].commandAllocator) return nullptr;

    // Lazily create the command list for this frame
    if (!commandLists[currentFrame]) {
        // Wrap in your abstracted ICommandList
        commandLists[currentFrame] = new CommandListD3D12(&releaseCommandBuffers, deviceContexts[currentFrame].commandList, &pDraws);
    }


    return commandLists[currentFrame];
}



void bnGraphicsD3D12::BeginFrame()
{
    /*WaitForFence(currentFrame);

    */

    if (commandQueue) {
        for (size_t i = 0; i < fence.size(); i++) {
            const UINT64 value = fenceValues[i] + 1;
            commandQueue->Signal(fence[i].Get(), value);
            fenceValues[i] = value;

            if (fence[i]->GetCompletedValue() < value) {
                fence[i]->SetEventOnCompletion(value, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

    ClearPendingReleases();


    // Reset command allocator for this frame
    deviceContexts[currentFrame].commandAllocator->Reset();

    // Reset command list to record commands for this frame
    deviceContexts[currentFrame].commandList->Reset(deviceContexts[currentFrame], nullptr);

    frameDone = false;
}

void bnGraphicsD3D12::EndFrame()
{
    auto rtM = renderTarget;
    auto& commandList = ((CommandListD3D12*)GetCommandList())->list;
    RenderTargetD3D12* cRenderPass = nullptr;
    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

    // Backbuffer transition
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
        swapChainBuffers[currentFrame].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    ));


    if (rtM->colorTargets[currentFrame]->desc.samples > 1) {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            rtM->colorTargets[currentFrame]->resource,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ));
    }

    // Depth buffer transition (only if needed)
    if (rtM->depth && rtM->depth->texture->explicitLayout != ImageLayout::DepthStencil) {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            rtM->depth->texture->resource,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        ));

        rtM->depth->texture->explicitLayout = ImageLayout::DepthStencil;
    }

    // Execute all barriers in one call
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        barriers.clear();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtM->rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        currentFrame,
        rtM->rtvDescriptorSize
    );

    if (rtM->depth) {
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
            rtM->dsvHeap->GetCPUDescriptorHandleForHeapStart()
        );
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    }
    else {
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }

    FLOAT clearColor[4] = { config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    cRenderPass = rtM;

    for (auto& draw : pDraws) {
        auto cmdList = draw.cmdBuffer;

        // ----------------------
        // Set Render Target
        // ----------------------
        if (draw.pipeline) {
            PipelineD3D12* d3dPipeline = dynamic_cast<PipelineD3D12*>(draw.pipeline);
            if (!d3dPipeline) continue;


            // Bind pipeline state
            
          
            auto rt = d3dPipeline->d3dRenderTarget;
            if (rt && cRenderPass != rt) {
                cRenderPass = rt;
                CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rt->rtvHeap->GetCPUDescriptorHandleForHeapStart(),
                    currentFrame,
                    rt->rtvDescriptorSize);

                CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
                if (rt->depth) {
                    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        rt->depth->texture->resource,                        // ID3D12Resource*
                        D3D12_RESOURCE_STATE_PRESENT,             // Or D3D12_RESOURCE_STATE_COMMON if that's your init state
                        D3D12_RESOURCE_STATE_DEPTH_WRITE
                    );
                    commandList->ResourceBarrier(1, &barrier);
                    dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(rt->dsvHeap->GetCPUDescriptorHandleForHeapStart());
                }

                cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, rt->depth ? &dsvHandle : nullptr);

                //// Clear render targets
                FLOAT clearColor[4] = { config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a };
                cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

                if (rt->depth) {
                    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                        rt->depthClear, rt->stencilClear, 0, nullptr);
                }
            }

            cmdList->SetPipelineState(d3dPipeline->pso);
            cmdList->SetGraphicsRootSignature(d3dPipeline->rootSignature);
        }

        // ----------------------
        // Set Vertex Buffer
        // ----------------------
        if (draw.buffer) {
            BufferD3D12* d3dBuffer = static_cast<BufferD3D12*>(draw.buffer);
            D3D12_VERTEX_BUFFER_VIEW vbView{};
            vbView.BufferLocation = d3dBuffer->resource->GetGPUVirtualAddress();
            vbView.StrideInBytes = d3dBuffer->desc.stride;
            vbView.SizeInBytes = static_cast<UINT>(d3dBuffer->desc.size);
            cmdList->IASetVertexBuffers(0, 1, &vbView);
        }

        // ----------------------
        // Set Descriptor Sets
        // ----------------------
        if (draw.ds) {
            DescriptorSetD3D12* d3dSet = static_cast<DescriptorSetD3D12*>(draw.ds);

            if (!d3dSet || !draw.pipeline) continue;

            // Bind descriptor heaps
            ID3D12DescriptorHeap* heaps[] = { d3dSet->pool->cbvSrvUavHeap, d3dSet->pool->samplerHeap}; 
            UINT heapCount = 0;

            if (heaps[0]) heapCount++;
            if (heaps[1]) heapCount++;

            cmdList->SetDescriptorHeaps(heapCount, heaps);


    
            for (auto& binding : d3dSet->bindings) {
                if (binding.type == DescriptorType::CombinedImageSampler) {
                    cmdList->SetGraphicsRootDescriptorTable(binding.binding, d3dSet->GetGPUHandle(binding.binding));
                    cmdList->SetGraphicsRootDescriptorTable(binding.binding + 1, d3dSet->GetGPUHandle(binding.binding + 1));
                }
                else {
                    cmdList->SetGraphicsRootDescriptorTable(binding.binding, d3dSet->GetGPUHandle(binding.binding));
                }
            }
        }

        // ----------------------
        // Set Viewport / Scissor
        // ----------------------
        if (draw.viewport) {
            auto* vp = static_cast<ViewPortD3D12*>(draw.viewport);
            cmdList->RSSetViewports(1, &vp->viewport);
        }

        if (draw.scissor) {
            auto* vp = static_cast<ViewPortD3D12*>(draw.scissor);
            cmdList->RSSetScissorRects(1, &vp->scissorRect);
        }

        // ----------------------
        // Draw Call
        // ----------------------
        if (draw.indexBuffer) {
            auto d3dIndexBuffer = dynamic_cast<BufferD3D12*>(draw.indexBuffer);
            D3D12_INDEX_BUFFER_VIEW ibv{};
            ibv.BufferLocation = d3dIndexBuffer->resource->GetGPUVirtualAddress();
            ibv.Format = DXGI_FORMAT_R32_UINT; // or 16-bit depending on your index buffer
            ibv.SizeInBytes = static_cast<UINT>(draw.indexCount * d3dIndexBuffer->desc.stride);
            cmdList->IASetIndexBuffer(&ibv);
            cmdList->IASetPrimitiveTopology(ConvertPrimitive(draw.type));
            cmdList->DrawIndexedInstanced(static_cast<UINT>(draw.indexCount), 1, static_cast<UINT>(draw.indexOffset), 0, 0);
        }
        else {
            cmdList->IASetPrimitiveTopology(ConvertPrimitive(draw.type));
         
            cmdList->DrawInstanced(
                static_cast<UINT>(draw.vertexCount),
                1,
                static_cast<UINT>(draw.vertexOffset),
                0
            );
        }

        if (cRenderPass && cRenderPass != rtM) {
            // end frame
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                cRenderPass->colorTargets[currentFrame]->resource,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            );
            commandList->ResourceBarrier(1, &barrier);

            auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
                cRenderPass->depth->texture->resource,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_PRESENT
            );

            commandList->ResourceBarrier(1, &barrier2);
        }
    }

    pDraws.clear();


    if (rtM->colorTargets[currentFrame]->desc.samples == 1) {
        // Color target transition
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            swapChainBuffers[currentFrame].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        ));

    }
    else {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            swapChainBuffers[currentFrame].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RESOLVE_DEST
        ));

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            rtM->colorTargets[currentFrame]->resource,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE
        ));
    }

    // Depth target transition (if present)
    if (rtM->depth) {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            rtM->depth->texture->resource,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PRESENT
        ));

        rtM->depth->texture->explicitLayout = ImageLayout::Present;
    }

    // Submit all barriers at once
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    barriers.clear();

    if (rtM->colorTargets[currentFrame]->desc.samples > 1) {
        commandList->ResolveSubresource(
            swapChainBuffers[currentFrame].Get(),             // destination backbuffer
            0,
            rtM->colorTargets[currentFrame]->resource,          // source MSAA render target
            0,
            ToDXGIFormat(swapChainFormat)
        );

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            swapChainBuffers[currentFrame].Get(),
            D3D12_RESOURCE_STATE_RESOLVE_DEST,
            D3D12_RESOURCE_STATE_PRESENT
        ));

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            rtM->colorTargets[currentFrame]->resource,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            D3D12_RESOURCE_STATE_PRESENT
        ));

        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }


    // Finish recording the command list
    commandList->Close();


    std::vector<ID3D12CommandList*> listsToExecute;
    for (auto& buffer : SingleTimeCommandBuffers) {
        // 1. Close the command list
        buffer->list->Close();

        listsToExecute.push_back(buffer->list);
    }
    listsToExecute.push_back(commandList.Get());
    commandQueue->ExecuteCommandLists(static_cast<UINT>(listsToExecute.size()), listsToExecute.data());
    UINT64 waitValue = ++sFenceValue;
    commandQueue->Signal(sFence.Get(), waitValue);
    sFence->SetEventOnCompletion(waitValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    for (auto& buffer : SingleTimeCommandBuffers) {
        buffer->Release();

        // 6. Delete the buffer wrapper
        delete buffer;
    }
    SingleTimeCommandBuffers.clear();
    frameDone = true;
}

void bnGraphicsD3D12::Present()
{
    // Present the swap chain
    swapChain->Present(config.vsync ? 1 : 0, 0); // 1 = vsync, 0 = no flags
    WaitForFence(currentFrame);

    // Advance to the next frame
    currentFrame = (currentFrame + 1) % config.framesInFlight;
   // ClearPendingReleases();

}

void bnGraphicsD3D12::Resize(long width, long height)
{
    this->width = width;
    this->height = height;
    WaitForFence(currentFrame);
    for (int i = 0; i < swapChainBuffers.size(); i++)
    {
        swapChainBuffers[i].Reset();
    }
    swapChainBuffers.clear();

    HRESULT hr = swapChain->ResizeBuffers(
        config.framesInFlight,          // Buffer count
        width,
        height,
        backBufferFormat,    // e.g. DXGI_FORMAT_R8G8B8A8_UNORM
        0                    // Flags
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to resize swap chain buffers!");
    }

    swapChainBuffers.resize(config.framesInFlight);

    for (UINT i = 0; i < config.framesInFlight; i++)
    {
        HRESULT hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffers[i]));
        if (FAILED(hr))
            throw std::runtime_error("Failed to get swap chain buffer!");

        auto name = std::wstring(L"Swapchain Frame ") + std::to_wstring(i);
        swapChainBuffers[i]->SetName(name.c_str());
        
    }

    if (depthTexture) {
        depthTexture->Release();
        depthTexture = nullptr;
    }



    auto oldTarget = renderTarget;
    if (renderTarget != VK_NULL_HANDLE) {
        renderTarget->Release();
        //renderTarget->dsvHeap->Release();
    }



    TextureDesc depthFormat{};
    depthFormat.width = width;
    depthFormat.height = height;
    depthFormat.samples = config.msaaSamples;
    depthFormat.format = TextureFormat::D32_Float;
    depthFormat.isDepthStencil = true;
    depthFormat.debugName = "Depth Buffer";
    depthTexture = CreateTexture(depthFormat);
    depthStencil = CreateDepthStencil(depthTexture);

    std::vector<ITexture*> swapchainImages;

    for (size_t i = 0; i < config.framesInFlight; ++i) {



        if (config.enableMSAA) {
            TextureDesc msaaFormat{};
            msaaFormat.width = width;
            msaaFormat.height = height;
            msaaFormat.samples = config.msaaSamples;
            msaaFormat.format = swapChainFormat;
            msaaFormat.isRenderTarget = true;
            msaaSWPCHTexture[i] = CreateTexture(msaaFormat);      
        }
        else {
            
            swapchainImages.reserve(config.framesInFlight);

            auto currentSwapChainImage = GetSwapchainImageFrame(i);

            swapchainImages.push_back(currentSwapChainImage);
        }
     
    }

    if (config.msaaSamples > 1) {
        renderTarget = (RenderTargetD3D12*)CreateRenderTarget({
                       .colorTargets = msaaSWPCHTexture,
                       .depth = depthStencil,
                       .makeFramebuffer = false,
                       .colorLayout = { ImageLayout::Present }
            });
    }
    else {
        renderTarget = (RenderTargetD3D12*)CreateRenderTarget({
                      .colorTargets = swapchainImages,
                      .depth = depthStencil,
                      .makeFramebuffer = false,
                      .colorLayout = { ImageLayout::Present }
            });
    }

    //if (renderTarget) {
    //    renderTarget->rtvHeap->
    //    delete renderTarget;
    //}


    if (renderTarget && oldTarget) {
        // Copy back relevant render target state before restoring
        oldTarget->clearColors = renderTarget->clearColors;
        oldTarget->depthClear = renderTarget->depthClear;
        oldTarget->stencilClear = renderTarget->stencilClear;
        oldTarget->width = renderTarget->width;
        oldTarget->height = renderTarget->height;
        oldTarget->mipLevels = renderTarget->mipLevels;

        oldTarget->colorTargets = std::move(renderTarget->colorTargets);
        oldTarget->depth = renderTarget->depth;
        oldTarget->rtvHeap = renderTarget->rtvHeap;
        oldTarget->dsvHeap = renderTarget->dsvHeap;
        oldTarget->rtvDescriptorSize = renderTarget->rtvDescriptorSize;

        // Now delete the temporary render target
        delete renderTarget;
        renderTarget = nullptr;

        // Restore old render target
        renderTarget = oldTarget;
    }


    currentFrame = 0;

}

void bnGraphicsD3D12::ReleaseShader(IShader** shader)
{
    if (!shader) return;
    shaderRelease.push_back(shader);
}

void bnGraphicsD3D12::ReleaseBuffer(IBuffer** buffer)
{
    if (!buffer) return;
    bufferRelease.push_back(buffer);
}

void bnGraphicsD3D12::ReleaseTexture(ITexture** texture) 
{
    if (!texture) return;
    textureRelease.push_back(texture);
}

void bnGraphicsD3D12::ReleaseCommandPool(ICommandPool** pool)
{
    poolRelease.push_back(pool);
}

void bnGraphicsD3D12::ReleaseDescriptorPool(IDescriptorPool** pool)
{
    descriptorPoolRelease.push_back(pool);
}

void bnGraphicsD3D12::ReleaseDescriptorSetLayout(IDescriptorSetLayout** layout)
{
    descriptorSetLayoutRelease.push_back(layout);
}

void bnGraphicsD3D12::ReleaseOnPend(void* data)
{
    if (!data) return;
    pendVoids.push_back(data);
}

void bnGraphicsD3D12::Shutdown()
{
    // Wait for GPU to finish work
    if (commandQueue) {
        for (size_t i = 0; i < fence.size(); i++) {
            const UINT64 value = fenceValues[i] + 1;
            commandQueue->Signal(fence[i].Get(), value);
            fenceValues[i] = value;

            if (fence[i]->GetCompletedValue() < value) {
                fence[i]->SetEventOnCompletion(value, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

    ClearPendingReleases();

    //delete heapManager;
    //heapManager = nullptr;

    // Release render targets
    if (renderTarget) {
        renderTarget->Release();
        delete renderTarget;
    }
  //  renderTargets.clear();

    // Release MSAA textures
    //for (auto tex : msaaSWPCHTexture) {
    //    if (tex) {
    //        delete tex;
    //    }
    //}


    for (auto list : commandLists) {
        if (list && list->list) {
            list->list.Reset();
        }
    }



    //msaaSWPCHTexture.clear();

    // Release depth/stencil
    if (depthStencil) {
        depthStencil->Release();
        delete depthStencil;
        depthStencil = nullptr;
    }

    if (depthTexture) {
        depthTexture->Release();
        delete depthTexture;
        depthTexture = nullptr;
    }

    // Release copy pool
    if (copyPool) {
        copyPool->Release();
        delete copyPool;
        copyPool = nullptr;
    }

    if(sFence) sFence->Release();

    if (sFenceEvent) {
        CloseHandle(sFenceEvent);
    }

    // Close fence event
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    // Clear device contexts
    //deviceContexts.clear();

    // Reset swap chain buffers and fences
    for (auto& buf : swapChainBuffers) {
        buf.Reset(); // if ComPtr
    }
    swapChainBuffers.clear();
    fence.clear();
    fenceValues.clear();
    
    if (commandQueue) {
        commandQueue.Reset();
    }
    for (auto& ctx : deviceContexts) {
        ctx.commandAllocator.Reset();
        ctx.commandList.Reset();
    }
    deviceContexts.clear();

    // 8. Release swapchain
    if (swapChain) {
        swapChain.Reset();
    }

    //ComPtr<IDXGIDebug> debug;
    //if (SUCCEEDED(device.device.As(&debug))) {

    //        ReportLiveObjects()
    //}
   
    // 9. Release device
    if (device.device) {
        device.device.Reset();
    }

    // 10. Release DXGI factory
    if (factory) {
        factory.Reset();
    }

#ifdef _DEBUG
    IDXGIDebug* debugDev;
    HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDev));

    hr = debugDev->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
#endif

}

IRenderTarget* bnGraphicsD3D12::CreateRenderTarget(const RenderTargetDesc& desc)
{
    RenderTargetD3D12* target = new RenderTargetD3D12();

    target->width = desc.width;
    target->height = desc.height;
    target->mipLevels = desc.mipLevels;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = static_cast<UINT>(desc.colorTargets.size());
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&target->rtvHeap))))
        return nullptr;

    target->rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(target->rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (size_t i = 0; i < desc.colorTargets.size(); i++)
    {
        ITexture* tex = desc.colorTargets[i];
        TextureD3D12* resource = dynamic_cast<TextureD3D12*>(tex);
        if (!resource) continue;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = ToDXGIFormat(resource->desc.format); // DXGI_FORMAT
        rtvDesc.ViewDimension = resource->desc.samples <= 1 ? D3D12_RTV_DIMENSION_TEXTURE2D : D3D12_RTV_DIMENSION_TEXTURE2DMS;
        rtvDesc.Texture2D.MipSlice = 0;

        device->CreateRenderTargetView(resource->resource, &rtvDesc, handle);

        target->colorTargets.push_back(resource);
        handle.Offset(1, target->rtvDescriptorSize);
    }

    if (desc.depth)
    {
        target->depth = dynamic_cast<DepthStencilD3D12*>(desc.depth);

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1; // usually only one DSV
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&target->dsvHeap))))
        {
            delete target;
            return nullptr;
        }

        device->CreateDepthStencilView(target->depth->texture->resource, &target->depth->dsvDesc,
            target->dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }



    target->clearColors = desc.clearColors;
    target->depthClear = desc.depthClear;
    target->stencilClear = desc.stencilClear;

    return target;
}

IDepthStencil* bnGraphicsD3D12::CreateDepthStencil(ITexture* texture)
{
    auto text = dynamic_cast<TextureD3D12*>(texture);
    if (!text) return nullptr;
    auto ds = new DepthStencilD3D12();
    ds->texture = text;
    ds->dsvDesc.Format = ToDXGIFormat(ds->texture->desc.format); // DXGI_FORMAT
    ds->dsvDesc.ViewDimension = text->desc.samples <= 1 ? D3D12_DSV_DIMENSION_TEXTURE2D : D3D12_DSV_DIMENSION_TEXTURE2DMS;
    ds->dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    return ds;
}

ICommandPool* bnGraphicsD3D12::CreateCommandPool(CommandPoolDesc desc)
{
    D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT; // default

    switch (desc.type) {
    case CommandPoolDesc::Type::DIRECT: type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
    case CommandPoolDesc::Type::COMPUTE: type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
    case CommandPoolDesc::Type::COPY: type = D3D12_COMMAND_LIST_TYPE_COPY; break;
    default: type = D3D12_COMMAND_LIST_TYPE_DIRECT; break;
    }

    return new CommandPoolD3D12(device, type);
}

IDescriptorPool* bnGraphicsD3D12::CreateDescriptorPool(DescriptorPoolDesc desc)
{
    return new DescriptorPoolD3D12(device, desc);
}

IDescriptorSetLayout* bnGraphicsD3D12::CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc)
{
    return new DescriptorSetLayoutD3D12(device, desc);
}

ITexture* bnGraphicsD3D12::GetSwapchainImage()
{
    return GetSwapchainImageFrame(currentFrame);
}

IPipelineBuilder* bnGraphicsD3D12::CreatePipelineBuilder()
{
    return new PipelineBuilderD3D12(&pipelineRelease, device, renderTarget);
}

ITexture* bnGraphicsD3D12::CreateTexture(const TextureDesc& desc, const void* initialData)
{
    auto tex = new TextureD3D12(true); // owns the resource
    tex->desc = desc;

    // Determine DXGI format
    DXGI_FORMAT dxFormat = ToDXGIFormat(desc.format);

    // Resource flags
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (desc.isRenderTarget) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (desc.isDepthStencil) {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    // Resource description
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Alignment = 0;
    texDesc.Width = desc.width;
    texDesc.Height = desc.dimension == TextureDimensions::Dim1 ? 1 : desc.height;
    texDesc.DepthOrArraySize = desc.depth > 1 ? desc.depth : 1;
    texDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
    texDesc.Format = dxFormat;
    texDesc.SampleDesc.Count = desc.samples;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = flags;

    switch (desc.dimension) {
    case TextureDimensions::Dim1: texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D; break;
    case TextureDimensions::Dim2: texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
    case TextureDimensions::Dim3: texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
    }

    // Clear value for RTV / DSV
    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClear = nullptr;

    if (desc.isRenderTarget) {
        clearValue.Format = dxFormat;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;
        pClear = &clearValue;
    }
    else if (desc.isDepthStencil) {
        clearValue.Format = dxFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        pClear = &clearValue;
    }

    // Heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        (desc.isDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : (desc.CpuAccessWrite || desc.Dynamic) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON),
        pClear,
        IID_PPV_ARGS(&tex->resource)
    );

    
    tex->explicitLayout = (desc.isDepthStencil ? ImageLayout::DepthStencil : (desc.CpuAccessWrite || desc.Dynamic) ? ImageLayout::GenericRead : ImageLayout::GenericRead);

    if (FAILED(hr)) {
        delete tex;
        return nullptr;
    }

    // Upload initial data if provided
    if (initialData) {
        ComPtr<ID3D12Resource> uploadHeap;

        UINT64 uploadBufferSize = 0;
        D3D12_RESOURCE_DESC texDesc = tex->resource->GetDesc();
        device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Alignment = 0;
        uploadDesc.Width = uploadBufferSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadHeap)
        );

      
        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = initialData;         // pointer to your CPU data
        subresourceData.RowPitch = desc.widthBytes;  // bytes per row
        subresourceData.SlicePitch = desc.widthBytes * desc.height;

        // transition resource to copy destination
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            tex->resource,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
        );

        auto commandList = (CommandBufferD3D12*)BeginSingleTimeCommands(copyPool);
        // create a commandList just for copying the data
        commandList->list->ResourceBarrier(1, &barrier);

        // copy data
        UpdateSubresources(commandList->list, tex->resource, uploadHeap.Get(), 0, 0, 1, &subresourceData);

        // transition resource to final state
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            tex->resource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE // or RENDER_TARGET if needed
        );
        commandList->list->ResourceBarrier(1, &barrier);
        EndSingleTimeCommands(commandList);
    }

    // Debug name
    if (!desc.debugName.empty()) {
        tex->resource->SetName(std::wstring(desc.debugName.begin(), desc.debugName.end()).c_str());
    }

    tex->slot = desc.slot;

    return tex;
}

IShader* bnGraphicsD3D12::CreateShader(const ShaderDesc& desc)
{
    return new ShaderD3D12(desc);
}

IBuffer* bnGraphicsD3D12::CreateBuffer(const BufferDesc& desc, const void* data)
{
    auto buffer = new BufferD3D12(desc);

    D3D12_HEAP_PROPERTIES heapProps = {};
    D3D12_RESOURCE_STATES initState;

    switch (desc.type) {
    case BufferType::Vertex:
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        initState = D3D12_RESOURCE_STATE_COMMON; // will transition later to VERTEX
        break;

    case BufferType::Index:
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        initState = D3D12_RESOURCE_STATE_COMMON;
        break;

    case BufferType::Constant:
        if (desc.dynamic) {
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;     // CPU > GPU
            initState = D3D12_RESOURCE_STATE_GENERIC_READ;
        }
        else {
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            initState = D3D12_RESOURCE_STATE_COMMON;
        }
        break;

    case BufferType::Storage:
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        initState = D3D12_RESOURCE_STATE_COMMON; // transition later to UAV
        break;

    case BufferType::Staging:
        //if (desc.dynamic) {
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;     // CPU > GPU
            initState = D3D12_RESOURCE_STATE_GENERIC_READ;
        //}
        //else {
        //    heapProps.Type = D3D12_HEAP_TYPE_READBACK;   // GPU > CPU
        //    initState = D3D12_RESOURCE_STATE_COPY_DEST;
        //}
        break;
    }   

    auto decidedSize =  desc.byteWidth == 0 ? desc.size * desc.stride : desc.byteWidth;
    buffer->desc.size = desc.type == BufferType::Constant ? (decidedSize + 255) & ~255 : decidedSize;
    if (buffer->desc.size == 0) {
        buffer->desc.size = desc.size;
    }

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Alignment = 0;
    resDesc.Width = buffer->desc.size;
    resDesc.Height = 1; 
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initState,
        nullptr,
        IID_PPV_ARGS(&buffer->resource)
    ))) {
        delete buffer;
        return nullptr;
    }

    if (data) {
        if(config.clipSpace == VerticesClipSpace::VULKAN){
            if (desc.type == BufferType::Vertex) {
                buffer->dataBuffer.resize(desc.size * desc.stride);
                memcpy_s(buffer->dataBuffer.data(), buffer->dataBuffer.size(), data, buffer->dataBuffer.size());

                for (size_t i = 0; i < desc.size; i++) {
                    float* vertex = reinterpret_cast<float*>(buffer->dataBuffer.data() + i * desc.stride);

                    if (desc.stride >= sizeof(float) * 2) {
                        vertex[1] = -vertex[1]; // Flip Y
                    }
                }
            }
        }

        if (desc.dynamic) {
            void* mapped = nullptr;
            D3D12_RANGE range{ 0, buffer->desc.size * buffer->desc.stride };
            buffer->resource->Map(0, &range, &mapped);
            memcpy(mapped, buffer->dataBuffer.empty() ? data : buffer->dataBuffer.data(), buffer->desc.size * buffer->desc.stride);
            buffer->resource->Unmap(0, nullptr);
            buffer->dataBuffer.clear();
            buffer->dataBuffer.shrink_to_fit();
        }
        else {
            // For default heap, you would use a copy command list similar to texture upload
            BufferDesc desc = {};
            desc.type = BufferType::Staging;
            desc.size = buffer->desc.size;
            desc.stride = buffer->desc.stride;
            desc.byteWidth = buffer->desc.byteWidth;
            desc.dynamic = true;

            buffer->upload = (BufferD3D12*)CreateBuffer(desc);

            //// --- 2. Fill upload buffer with data ---
            void* mapped = nullptr;
            D3D12_RANGE range{ 0, 0 }; // we don't intend to read
            buffer->upload->resource->Map(0, &range, &mapped);
            memcpy(mapped, buffer->dataBuffer.empty() ? data : buffer->dataBuffer.data(), buffer->desc.size);
            buffer->upload->resource->Unmap(0, nullptr);

            // --- 3. Copy to default buffer using single-time command list ---
            auto cmd = (CommandBufferD3D12*)BeginSingleTimeCommands(copyPool);

            cmd->list->CopyBufferRegion(
                buffer->resource, // Dest
                0,
                buffer->upload->resource,     // Src
                0,
                buffer->desc.size
            );

            // --- 4. Transition resource into usable state (ex: VERTEX/INDEX/CONSTANT) ---
            D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_GENERIC_READ;
            switch (desc.type) {
            case BufferType::Vertex:   finalState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; break;
            case BufferType::Index:    finalState = D3D12_RESOURCE_STATE_INDEX_BUFFER; break;
            case BufferType::Constant: finalState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; break;
            case BufferType::Storage:  finalState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; break;
            case BufferType::Staging: finalState = D3D12_RESOURCE_STATE_GENERIC_READ;
            }

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                buffer->resource,
                D3D12_RESOURCE_STATE_COPY_DEST,
                finalState
            );
            cmd->list->ResourceBarrier(1, &barrier);

            EndSingleTimeCommands(cmd);
            ReleaseBuffer((IBuffer**)&buffer->upload);
            buffer->dataBuffer.clear();
            buffer->dataBuffer.shrink_to_fit();
        }
    }

    return buffer;
}

IInputLayout* bnGraphicsD3D12::CreateInputLayout(const InputLayoutDesc& desc)
{
    return new InputLayoutD3D12(desc);
}

ISamplerState* bnGraphicsD3D12::CreateSamplerState(const SamplerStateDesc& desc)
{
    auto state = new SamplerStateD3D12();

    state->desc.Filter = ToD3D12Filter(desc.minFilter, desc.magFilter, desc.mipFilter, desc.maxAnisotropy);
    state->desc.AddressU = ToD3D12Address(desc.addressU);
    state->desc.AddressV = ToD3D12Address(desc.addressV);
    state->desc.AddressW = ToD3D12Address(desc.addressW);
    state->desc.MipLODBias = desc.mipLODBias;
    state->desc.MaxAnisotropy = desc.maxAnisotropy;
    state->desc.ComparisonFunc = ToD3D12Comparison(desc.comparisonFunc);
    memcpy(state->desc.BorderColor, desc.borderColor, sizeof(float) * 4);
    state->desc.MinLOD = desc.minLOD;
    state->desc.MaxLOD = desc.maxLOD;

    return state;
}

IViewPort* bnGraphicsD3D12::CreateViewPort(const ViewPortDesc& desc)
{
    return new ViewPortD3D12(desc);
}

IRasterizerState* bnGraphicsD3D12::CreateRasterizerState(const RasterizerDesc& desc)
{
    return new RasterizerStateD3D12(desc);
}

IDepthStencilState* bnGraphicsD3D12::CreateDepthStencilState(const DepthStencilDesc& desc)
{
    return new DepthStencilStateD3D12(desc);
}

IBlendState* bnGraphicsD3D12::CreateBlendState(const BlendStateDesc& desc)
{
    return new BlendStateD3D12(desc);
}

ICommandBuffer* bnGraphicsD3D12::BeginSingleTimeCommands(ICommandPool* pool)
{
    auto poolD3D12 = dynamic_cast<CommandPoolD3D12*>(pool);

    // Create a command list
    ID3D12GraphicsCommandList* commandList = poolD3D12->AllocateCommandList();
    //device->CreateCommandList(
    //    0,
    //    poolD3D12->type,        // Direct / Compute / Copy
    //    allocator,
    //    nullptr,           // Initial PSO if needed
    //    IID_PPV_ARGS(&commandList)
    //);

    //// Close it immediately to reset later (required by DX12)
    //commandList->Close();
    //commandList->Reset(allocator, nullptr);

    return new CommandBufferD3D12(commandList, poolD3D12);
}

void bnGraphicsD3D12::EndSingleTimeCommands(ICommandBuffer* buffer) {
    auto bufferD3D12 = dynamic_cast<CommandBufferD3D12*>(buffer);
    if (!bufferD3D12 || !bufferD3D12->list) return;

    if (frameDone) {

            bufferD3D12->list->Close();
            ID3D12CommandList* listsToExecute = { bufferD3D12->list };
            commandQueue->ExecuteCommandLists(1, &listsToExecute);
            UINT64 waitValue = ++sFenceValue;
            commandQueue->Signal(sFence.Get(), waitValue);
            sFence->SetEventOnCompletion(waitValue, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
           
            bufferD3D12->Release();

               
                delete bufferD3D12;
            
     
    } else SingleTimeCommandBuffers.push_back(bufferD3D12);
}

void bnGraphicsD3D12::CopyToBuffer(IBuffer* buffer, ICommandBuffer* pool, void* data, size_t size)
{
    auto d3dBuffer = static_cast<BufferD3D12*>(buffer);
    auto cmdList = static_cast<CommandBufferD3D12*>(pool)->list;

    if (!d3dBuffer->desc.dynamic && d3dBuffer->desc.type != BufferType::Staging) {
        // Create an upload heap for staging
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC desc = d3dBuffer->resource->GetDesc();

        BufferDesc bdesc = {};
        bdesc.type = BufferType::Staging;
        bdesc.size = buffer->type == BufferType::Constant ? (size + 255) & ~255 : size;
        bdesc.dynamic = true;       // if needed

        d3dBuffer->upload = (BufferD3D12*)CreateBuffer(bdesc);

        // Map and copy
        void* mapped = nullptr;
        d3dBuffer->upload->resource->Map(0, nullptr, &mapped);
        memcpy(mapped, data, size);
        d3dBuffer->upload->resource->Unmap(0, nullptr);

        // Copy from upload buffer to GPU buffer
        cmdList->CopyResource(d3dBuffer->resource, d3dBuffer->upload->resource);

        ReleaseBuffer((IBuffer**)&d3dBuffer->upload);
    }
    else {
        void* mapped = nullptr;
        d3dBuffer->resource->Map(0, nullptr, &mapped);
        memcpy(mapped, data, size);
        d3dBuffer->resource->Unmap(0, nullptr);
    }
}

void bnGraphicsD3D12::MapBufferMemory(IBuffer* buffer, void** dataPtr)
{
    auto d3dBuffer = static_cast<BufferD3D12*>(buffer);
    d3dBuffer->resource->Map(0, nullptr, dataPtr);
}

void bnGraphicsD3D12::UnmapBufferMemory(IBuffer* buffer)
{
    auto d3dBuffer = static_cast<BufferD3D12*>(buffer);
    d3dBuffer->resource->Unmap(0, nullptr);
}


void bnGraphicsD3D12::CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc)
{
    auto cmdList = static_cast<CommandBufferD3D12*>(cBuffer)->list;
    auto d3dBuffer = static_cast<BufferD3D12*>(srcBuffer);
    auto d3dTexture = static_cast<TextureD3D12*>(dstTexture);

    if (desc.bufferRowLength == desc.imageExtent.width) {
        desc.bufferRowLength = desc.imageExtent.width * 4;
    }

    if (desc.bufferImageHeight > desc.imageExtent.height) {
        desc.bufferImageHeight = desc.imageExtent.height;
    }

    // Destination texture
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = d3dTexture->resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = desc.imageSubresource.mipLevel; // use Vulkan-style mip level

    // Source buffer
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = d3dBuffer->resource;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = src.PlacedFootprint;
    footprint.Offset = desc.bufferOffset;
    footprint.Footprint.Format = ToDXGIFormat(d3dTexture->desc.format);
    footprint.Footprint.Width = desc.imageExtent.width;
    footprint.Footprint.Height = desc.imageExtent.height;
    footprint.Footprint.Depth = desc.imageExtent.depth;
    footprint.Footprint.RowPitch = desc.bufferRowLength > 0 ? desc.bufferRowLength : desc.imageExtent.width * 4; // fallback

    // Source box within the buffer
    D3D12_BOX srcBox = {};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = desc.imageExtent.width;
    srcBox.bottom = desc.imageExtent.height;
    srcBox.back = desc.imageExtent.depth;

    // Destination offset
    UINT dstX = desc.imageOffset.x;
    UINT dstY = desc.imageOffset.y;
    UINT dstZ = desc.imageOffset.z;

    // Perform the copy
    cmdList->CopyTextureRegion(&dst, dstX, dstY, dstZ, &src, &srcBox);
}

void bnGraphicsD3D12::CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc)
{
    auto cmdList = static_cast<CommandBufferD3D12*>(cBuffer)->list;
    auto src = static_cast<TextureD3D12*>(srcBuffer);
    auto dst = static_cast<TextureD3D12*>(dstBuffer);

    // Source texture
    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = src->resource;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = desc.srcSubresource.mipLevel; // use mip level

    // Destination texture
    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = dst->resource;
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = desc.dstSubresource.mipLevel; // use mip level

    // Source box
    D3D12_BOX srcBox = {};
    srcBox.left = desc.srcOffset.x;
    srcBox.top = desc.srcOffset.y;
    srcBox.front = desc.srcOffset.z;
    srcBox.right = desc.srcOffset.x + desc.extent.width;
    srcBox.bottom = desc.srcOffset.y + desc.extent.height;
    srcBox.back = desc.srcOffset.z + desc.extent.depth;

    // Perform copy
    cmdList->CopyTextureRegion(
        &dstLoc,
        desc.dstOffset.x,
        desc.dstOffset.y,
        desc.dstOffset.z,
        &srcLoc,
        &srcBox
    );
}

void bnGraphicsD3D12::PushGroup(const char *name, uint32_t color) {
    PIXBeginEvent(color, name);
}

void bnGraphicsD3D12::PopGroup() {
    PIXEndEvent();
}

void bnGraphicsD3D12::SetMarker(const char *name, uint32_t color) {
    PIXSetMarker(color, name);
}

void bnGraphicsD3D12::ClearPendingReleases() {
   
      for (auto* cmdBuf : cbRelease) {
             cmdBuf->Release();
            delete cmdBuf;
        }
        cbRelease.clear();

        for (auto pipeline : pipelineRelease) {
            auto it = std::find_if(pDraws.begin(), pDraws.end(),
                [=](const PendingDrawD3D12& p) { return p.pipeline == pipeline; });

            if (it != pDraws.end()) {
                continue;
            }

            for (auto& ds : pipeline->descriptorSets) {
                if (device && ds.second != nullptr && pipeline->descriptorPool != nullptr) {
                    pipeline->descriptorPool->ClearAllocations();
                    ds.second->Release();
                }

                delete ds.second;
            }
            pipeline->descriptorSets.clear();

            if (pipeline->pso) {
                pipeline->pso->Release();
                pipeline->pso = nullptr;
                
            }
            //if (pipeline->descriptorLayout) {
            //    pipeline->descriptorLayout->Release();
            //    pipeline->descriptorLayout = nullptr;
            //}

            delete pipeline;
        }
        pipelineRelease.clear();
        for (auto bufPtr : bufferRelease) {
            if (bufPtr && *bufPtr) {
                (*bufPtr)->Release();
                delete* bufPtr;
                *bufPtr = nullptr;
            }
        }
        bufferRelease.clear();

        for (auto bufPtr : shaderRelease) {
            if (bufPtr && *bufPtr) {
                (*bufPtr)->Release();
                delete *bufPtr;
                *bufPtr = nullptr;
            }
        }
        shaderRelease.clear();

        for (auto tex : textureRelease) {
            if (tex && *tex) {
                auto texture = (TextureD3D12*)*tex;
                texture->Release();
                delete* tex;
                *tex = nullptr;
            }
        }
    
        textureRelease.clear();

        for (auto cmdPool : poolRelease) {
            if (cmdPool && *cmdPool) {
                (*cmdPool)->Release();
                delete* cmdPool;
                *cmdPool = nullptr;
            }
        }

        poolRelease.clear();

        for (auto cmdBuffer : releaseCommandBuffers) {
            if (cmdBuffer) {
                auto it = std::find_if(pDraws.begin(), pDraws.end(),
                    [=](const PendingDrawD3D12& p) { return p.cmdBuffer == cmdBuffer->list.Get(); });

                if (it != pDraws.end()) {
                    continue;
                }
                cmdBuffer->list.Reset();
                delete cmdBuffer;
                auto iteration = std::find(commandLists.begin(), commandLists.end(), cmdBuffer);
                if (iteration != commandLists.end()) {
                    *iteration = nullptr;
                }
            }
        }

        releaseCommandBuffers.clear();

        for (auto pool : descriptorPoolRelease) {

            if (pool && *pool) {
                auto it = std::find_if(pDraws.begin(), pDraws.end(),
                    [=](const PendingDrawD3D12& p) { return ((PipelineD3D12*)p.pipeline)->descriptorPool == (IDescriptorPool*)pool; });

                if (it != pDraws.end()) {
                    continue;
                }

                (*pool)->Release();
                delete* pool;
                *pool = nullptr;
            }
        }

        descriptorPoolRelease.clear();

        for (auto layout : descriptorSetLayoutRelease) {
            if (layout && *layout) {
                auto it = std::find_if(pDraws.begin(), pDraws.end(),
                    [=](const PendingDrawD3D12& p) { return ((PipelineD3D12*)p.pipeline)->descriptorLayout == (IDescriptorSetLayout*)layout; });

                if (it != pDraws.end()) {
                    continue;
                }
                (*layout)->Release();
                delete* layout;
                *layout = nullptr;
            }
        }

        descriptorSetLayoutRelease.clear();

        for (auto voids : pendVoids) {
            if (voids) {
                delete voids;
                voids = nullptr;
            }
        }

        pendVoids.clear();
}

void bnGraphicsD3D12::WaitTillImFree()
{
    if (commandQueue) {
        for (size_t i = 0; i < fence.size(); i++) {
            const UINT64 value = fenceValues[i] + 1;
            commandQueue->Signal(fence[i].Get(), value);
            fenceValues[i] = value;

            if (fence[i]->GetCompletedValue() < value) {
                fence[i]->SetEventOnCompletion(value, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }
}