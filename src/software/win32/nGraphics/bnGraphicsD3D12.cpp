#include "bnGraphicsD3D12.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#include <dxgidebug.h>
#include <cassert>
#include <iostream>
#include "pix3/pix3.h"

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


