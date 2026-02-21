#pragma once
#include "GraphicsSystem.h"
#include "nGraphics/bnShader.h"

class bnWindow;

struct DestroySEP : public IGPUCommand
{
    //GraphicsPipelineData* sep;
    //void* mPtr; // The GraphicsSystem Object
    std::vector<GraphicsPipelineData*>* pipes;
    DestroySEP(std::vector<GraphicsPipelineData*>* pipes) : pipes(pipes) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        
    }
};

inline void CreateDPS(IGraphicsDeviceExplicit* graphics, GraphicsPipelineData* sep) {
    if (!sep->dPool && !sep->dSetLayout) {
        DescriptorSetLayoutDesc layoutDesc;
        DescriptorPoolDesc poolDesc;
        poolDesc.flags = DescriptorPoolFlags::FreeDescriptor;
        poolDesc.maxSets = 0;
        std::unordered_map<uint32_t, DescriptorSetLayoutBindingDesc> mergedBindings;
        std::unordered_map<DescriptorType, uint32_t> counts;

        auto shaders = sep->plb->GetShaders();
        for (auto shaderD : *shaders) {
            auto shader = bnShader(shaderD);
            auto refl = shader.getReflection();

            for (auto& res : refl.resources) {
                // track max sets
                poolDesc.maxSets = std::max(poolDesc.maxSets, res.set + 1);

                // merge bindings
                auto it = mergedBindings.find(res.binding);
                if (it == mergedBindings.end()) {
                    DescriptorSetLayoutBindingDesc binding{};
                    binding.binding = res.binding;
                    binding.count = res.count;
                    binding.stageFlags = res.stage;
                    binding.type = res.type;
                    mergedBindings[res.binding] = binding;
                }
                else {
                    // merge stage flags
                    it->second.stageFlags |= res.stage;
                    // verify types match
                    if (it->second.type != res.type) {
                        throw std::runtime_error("Descriptor type mismatch between shaders");
                    }
                }

                counts[res.type] += res.count;
            }
        }

        // move merged bindings to layoutDesc
        for (auto& [_, binding] : mergedBindings) {
            layoutDesc.bindings.push_back(binding);
        }

        // pool sizes
        for (auto& [type, count] : counts) {
            poolDesc.poolSizes.push_back({ type, count });
        }

        sep->dPool = graphics->CreateDescriptorPool(poolDesc);
        sep->dSetLayout = graphics->CreateDescriptorSetLayout(layoutDesc);
        sep->plb->SetDescriptorPool(sep->dPool);
        sep->plb->SetDescriptorSetLayout(sep->dSetLayout);
    }
}

struct CopyBufferToImageCommand : public IGPUCommand
{
    ResourceHandle<IBuffer>* srcBuffer;
    ResourceHandle<ITexture>* dstTexture;
    BufferImageCopyDesc desc;
    GraphicsPipelineData** sepAd;

    CopyBufferToImageCommand(ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc, GraphicsPipelineData** sep) : srcBuffer(srcBuffer), dstTexture(dstTexture), desc(desc), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (!srcBuffer || !dstTexture) return;

        if (srcBuffer->Exists() && dstTexture->Exists()) {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->CopyBufferToImage(srcBuffer->Get(), dstTexture->Get(), desc);
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                
                if (!sep->cp) {
                    sep->cp = graphics->CreateCommandPool({
                        0, true, true
                        });
                }

                auto copyCmd = graphics->BeginSingleTimeCommands(sep->cp);
                //auto oldDstLayout = ;
                copyCmd->PipelineBarrier(
                    dstTexture->Get(),
                    dstTexture->Get()->explicitLayout,          // old layout
                    ImageLayout::CopyDst,            // new layout
                    ImageAccessLayout::None,         // src access mask (nothing yet)
                    ImageAccessLayout::CopyDst       // dst access mask
                );

                graphics->CopyBufferToImage(copyCmd, srcBuffer->Get(), dstTexture->Get(), desc);

                copyCmd->PipelineBarrier(
                    dstTexture->Get(),
                    dstTexture->Get()->explicitLayout,          // old layout
                    ImageLayout::ShaderRead,            // new layout
                    ImageAccessLayout::CopyDst,         // src access mask (nothing yet)
                    ImageAccessLayout::Read       // dst access mask
                );


                graphics->EndSingleTimeCommands(copyCmd);
            }
        }
    }
};


struct CopyImageToImageCommand : public IGPUCommand
{
    ResourceHandle<ITexture>* srcTexture;
    ResourceHandle<ITexture>* dstTexture;
    ImageCopyDesc desc;
    GraphicsPipelineData** sepAd;

    CopyImageToImageCommand(ResourceHandle<ITexture>* srcTexture, ResourceHandle<ITexture>* dstTexture, ImageCopyDesc desc, GraphicsPipelineData** sep) : srcTexture(srcTexture), dstTexture(dstTexture), desc(desc), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (!srcTexture || !dstTexture) return;

        if (srcTexture->Exists() && !dstTexture->Exists()) {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->CopyImageToImage(srcTexture->Get(), dstTexture->Get(), desc);
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);

                if (!sep->cp) {
                    sep->cp = graphics->CreateCommandPool({
                        0, true, true
                        });
                }

                auto copyCmd = graphics->BeginSingleTimeCommands(sep->cp);
                auto oldSrcLayout = srcTexture->Get()->explicitLayout;
                auto oldDstLayout = dstTexture->Get()->explicitLayout;
                copyCmd->PipelineBarrier(
                    srcTexture->Get(),
                    oldSrcLayout,            // old layout
                    ImageLayout::CopySrc,            // new layout
                    ImageAccessLayout::None,         // src access
                    ImageAccessLayout::CopySrc       // dst access
                );

                // Prepare cached texture as copy destination
                copyCmd->PipelineBarrier(
                    dstTexture->Get(),
                    oldDstLayout,          // old layout
                    ImageLayout::CopyDst,            // new layout
                    ImageAccessLayout::None,
                    ImageAccessLayout::CopyDst
                );

                graphics->CopyImageToImage(copyCmd, srcTexture->Get(), srcTexture->Get(), desc);

                copyCmd->PipelineBarrier(
                    srcTexture->Get(),
                    srcTexture->Get()->explicitLayout,          // old layout
                    oldDstLayout,            // new layout
                    ImageAccessLayout::CopyDst,         // src access mask (nothing yet)
                    ImageAccessLayout::Read       // dst access mask
                );

                copyCmd->PipelineBarrier(
                    dstTexture->Get(),
                    dstTexture->Get()->explicitLayout,          // old layout
                    oldDstLayout,            // new layout
                    ImageAccessLayout::CopyDst,         // src access mask (nothing yet)
                    ImageAccessLayout::Read       // dst access mask
                );

            }
        }
    }
};



struct BindTextureCommand : public IGPUCommand
{
    ResourceHandle<ITexture>* texture;
    GraphicsPipelineData** sepAd;
    u32 slot;

    BindTextureCommand(ResourceHandle<ITexture>* tex, GraphicsPipelineData** sep, u32 slot = 0) : texture(tex), sepAd(sep), slot(slot) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (!texture) return;
            if (texture->Exists()) graphics->BindTexture(texture->Get(), slot);
        }
        else {
            if (texture->Exists()) {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (!sep->pl) {
                    CreateDPS(graphics, sep);
                    if (sep->plb) sep->pl = sep->plb->Build();
                    else return;
                }

                auto ds = sep->pl->GetDescriptorSet(sep->descriptorSlotID);
             
                if (!ds) {
                    ds = sep->pl->CreateDescriptorSet(sep->descriptorSlotID);
                }
                
                ds->SetTexture(slot, texture->Get());

            }
        }
    }
};

struct BindSamplerCommand : public IGPUCommand
{
    ResourceHandle<ISamplerState>* sampler;
    GraphicsPipelineData** sepAd;
    u32 slot;


    BindSamplerCommand(ResourceHandle<ISamplerState>* sam, GraphicsPipelineData** sep, u32 slot = 0) : sampler(sam), sepAd(sep), slot(slot) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {

        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (!sampler) return;
            if (sampler->Exists()) graphics->BindSamplerState(sampler->Get());
        }
        else {



            if (sampler->Exists()) {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (!sep->pl) {
                    CreateDPS(graphics, sep);
                    if (sep->plb) sep->pl = sep->plb->Build();
                    else return;
                }

                auto ds = sep->pl->GetDescriptorSet(sep->descriptorSlotID);

                if (!ds) {
                    ds = sep->pl->CreateDescriptorSet(sep->descriptorSlotID);
                }

                ds->SetSampler(slot, sampler->Get());

            }
        }
    }
};

struct BindBufferCommand : public IGPUCommand
{
    // IBuffer* buffer;
    ResourceHandle<IBuffer>* handle;
    GraphicsPipelineData** sepAd;
    u32 slot;

    BindBufferCommand(ResourceHandle<IBuffer>* buf, GraphicsPipelineData** sep, u32 slot = 0) : handle(buf), sepAd(sep), slot(slot) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (!handle) return;
            if (handle->Exists()) {
                graphics->BindBuffer(handle->Get()); // TODO: ADD SLOT
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (handle->Exists() && handle->Get()) {
              

                if (handle->Get()->type == BufferType::Constant) {
                    if (!sep->pl && !sep->plb) return;

                    if (!sep->pl) {
                        CreateDPS(graphics, sep);
                        sep->pl = sep->plb->Build();

                        if (!sep->pl) return;
                    }

                 
                    auto ds = sep->pl->GetDescriptorSet(sep->descriptorSlotID);

                    if (!ds) {
                        ds = sep->pl->CreateDescriptorSet(sep->descriptorSlotID);
                    }

                  
                    ds->SetBuffer(slot, handle->Get());
                   

                    
                }
                else {
                    graphics->GetCommandList()->BindBuffer(handle->Get());
                }

        
            }
        }
    }
};

struct BindShaderCommand : public IGPUCommand
{
    //  IShader* shader;
    ResourceHandle<IShader>* handle;
    GraphicsPipelineData** sepAd;

    BindShaderCommand(ResourceHandle<IShader>* shad, GraphicsPipelineData** sep) : handle(shad), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (handle->Exists()) {
                graphics->BindShader(handle->Get());
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (handle->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.
                //sep->shaders.push_back(handle->Get());
                sep->plb->AddShader(handle->Get());
            }
        }
    }
};

struct BindInputLayoutCommand : public IGPUCommand
{
    ResourceHandle<IInputLayout>* inputLayout;
    GraphicsPipelineData** sepAd;

    BindInputLayoutCommand(ResourceHandle<IInputLayout>* input, GraphicsPipelineData** sep) : inputLayout(input), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (inputLayout->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindInputLayout(inputLayout->Get());
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (inputLayout->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.

                sep->plb->SetInputLayout(inputLayout->Get());
            }
        }
    }
};

struct BindViewPortCommand : public IGPUCommand
{
    ResourceHandle<IViewPort>* vp;
    GraphicsPipelineData** sepAd;

    BindViewPortCommand(ResourceHandle<IViewPort>* view, GraphicsPipelineData** sep) : vp(view), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (vp->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindViewPort(vp->Get());
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (vp->Exists()) {
                graphics->GetCommandList()->BindViewPort(vp->Get());
                graphics->GetCommandList()->BindScissor(vp->Get());
            }
        }
    }
};

struct BindRasterizerStateCommand : public IGPUCommand
{
    ResourceHandle<IRasterizerState>* rast;
    GraphicsPipelineData** sepAd;

    BindRasterizerStateCommand(ResourceHandle<IRasterizerState>* rasterizer, GraphicsPipelineData** sep) : rast(rasterizer), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (rast->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindRasterizerState(rast->Get());
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (rast->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.

                sep->plb->SetRasterizer(rast->Get());
            }
        }
    }
};

struct BindDepthStencilStateCommand : public IGPUCommand
{
    ResourceHandle<IDepthStencilState>* depthState;
    GraphicsPipelineData** sepAd;
    u32 stencilRef;
    BindDepthStencilStateCommand(ResourceHandle<IDepthStencilState>* depthStencilState, u32 ref, GraphicsPipelineData** sep) : depthState(depthStencilState), stencilRef(ref), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (depthState->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindDepthStencilState(depthState->Get(), stencilRef);
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (depthState->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.

                // TODO: add stencilRef for explicit devices!
                sep->plb->SetDepthStencil(depthState->Get());
            }
        }
    }
};

struct BindBlendStateCommand : public IGPUCommand
{
    ResourceHandle<IBlendState>* blending;
    GraphicsPipelineData** sepAd;
    const float* blendFactor;
    u32 sampleMask;

    BindBlendStateCommand(ResourceHandle<IBlendState>* blend, const float blendFactors[4], u32 mask, GraphicsPipelineData** sep) : blending(blend), blendFactor(blendFactors), sampleMask(mask), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (blending->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindBlendState(blending->Get(), blendFactor, sampleMask);
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (blending->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.

                // TODO: add masks and blend factors to explicit devices
                sep->plb->SetBlendState(blending->Get());
            }
        }
    }
};

struct BindRenderTargetCommand : public IGPUCommand
{
    ResourceHandle<IRenderTarget>* target;
    GraphicsPipelineData** sepAd;
    ResourceHandle<IDepthStencil>* depth;
    BindRenderTargetCommand(ResourceHandle<IRenderTarget>* renderTarget, ResourceHandle<IDepthStencil>* depthStencil, GraphicsPipelineData** sep) : target(renderTarget), depth(depthStencil), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (target->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindRenderTarget(target->Get(), depth->Get());
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (target->Exists()) {
                if (!sep->plb && !sep->pl) {
                    sep->plb = graphics->CreatePipelineBuilder();
                }
                if (sep->pl) return; // too late.

                // ?
                sep->plb->SetRenderTarget(target->Get());
            }
        }
    }
};

struct ClearRenderTargetCommand : public IGPUCommand
{
    ResourceHandle<IRenderTarget>* target;
    GraphicsPipelineData** sepAd;
    const float* colors;
    ClearRenderTargetCommand(ResourceHandle<IRenderTarget>* renderTarget, const float color[4], GraphicsPipelineData** sep) : target(renderTarget), colors(color), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (target->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->ClearRenderTarget(target->Get(), colors);
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (target->Exists()) {
                //if (!sep->plb && !sep->pl) {
                //    sep->plb = graphics->CreatePipelineBuilder();
                //}
                //if (sep->pl) return; // too late.

                //graphics->
                //// ?
                //sep->plb->SetRenderTarget(target->Get());
            }
        }
    }
};

struct ClearDepthStencilCommand : public IGPUCommand
{
    ResourceHandle<IDepthStencil>* depth;
    GraphicsPipelineData** sepAd;
    float depthF;
    u8 stencil;

    ClearDepthStencilCommand(ResourceHandle<IDepthStencil>* depthStencil, float depthF, u8 stencil, GraphicsPipelineData** sep) : depth(depthStencil), depthF(depthF), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            if (depth->Exists()) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->ClearDepthStencil(depth->Get(), depthF, stencil);
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (depth->Exists()) {
                //if (!sep->plb && !sep->pl) {
                //    sep->plb = graphics->CreatePipelineBuilder();
                //}
                //if (sep->pl) return; // too late.

                //graphics->
                //// ?
                //sep->plb->SetRenderTarget(target->Get());
            }
        }
    }
};

struct CopyToBufferCommand : public IGPUCommand
{
    //IBuffer* buffer;
    ResourceHandle<IBuffer>* buffer;
    void* data;
    size_t size;
    GraphicsPipelineData** sepAd;

    CopyToBufferCommand(ResourceHandle<IBuffer>* buffer, void* data, size_t size, GraphicsPipelineData** sep) : buffer(buffer), data(data), size(size), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        auto sep = *sepAd;

        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (!buffer) return;

            if (buffer->Exists()) {
                graphics->CopyToBuffer(buffer->Get(), data, size);
            }
        }
        else {
            auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
            if (!buffer) return;

            if (!sep->cp) {
                sep->cp = graphics->CreateCommandPool({
                    0, true, true
                });
            }

            if (buffer->Exists()) {
                sep->cb = graphics->BeginSingleTimeCommands(sep->cp);
                graphics->CopyToBuffer(buffer->Get(), sep->cb, data, size);
                graphics->EndSingleTimeCommands(sep->cb);
            }
        }
    }
};

struct ReleaseBufferCommand : public IGPUCommand
{
    ResourceHandle<IBuffer>* buffer;

    ReleaseBufferCommand(ResourceHandle<IBuffer>* buf) : buffer(buf) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        buffer->MoveAsPrevious();
        graphicsDevice->ReleaseBuffer(buffer->GetPreviousResource());
        buffer->status = ResourceStatus::DESTROYED;
        buffer->Invalidate();
    }
};

struct ReleaseTextureCommand : public IGPUCommand
{
    ResourceHandle<ITexture>* texture;

    ReleaseTextureCommand(ResourceHandle<ITexture>* tex) : texture(tex) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        // Since we have only 1 address, we need to make two and move it to another address and release it on that one always.
        texture->MoveAsPrevious();
        graphicsDevice->ReleaseTexture(texture->GetPreviousResource());
        texture->status = ResourceStatus::DESTROYED;
    }
};

struct ReleaseShaderCommand : public IGPUCommand
{
    ResourceHandle<IShader>* buffer;

    ReleaseShaderCommand(ResourceHandle<IShader>* buf) : buffer(buf) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        graphicsDevice->ReleaseShader(buffer->GetAddress());
        //buffer->Invalidate();
    }
};


struct ReleaseInputLayoutCommand : public IGPUCommand
{
    ResourceHandle<IInputLayout>* layout;

    ReleaseInputLayoutCommand(ResourceHandle<IInputLayout>* lay) : layout(lay) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!layout) return;
        //auto layoutPtr = layout->Get();
        //if (!layoutPtr) return;
        layout->Invalidate();
    }
};

struct ReleaseSamplerStateCommand : public IGPUCommand
{
    ResourceHandle<ISamplerState>* samplerState;

    ReleaseSamplerStateCommand(ResourceHandle<ISamplerState>* samp) : samplerState(samp) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!samplerState || !samplerState->Get()) return;
        samplerState->Get()->Release();
    }
};

struct DestroyResourceCommand : public IGPUCommand
{
    IResourceHandle** buffer;
    std::vector<IResourceHandle**>* relVariables;

    DestroyResourceCommand(IResourceHandle** buf, std::vector<IResourceHandle**>* rel) : buffer(buf), relVariables(rel) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            if (!buffer || std::find(relVariables->begin(), relVariables->end(), buffer) != relVariables->end()) {
                return;
            }

            relVariables->push_back(buffer);
        }
    }
};

struct DrawCommand : public IGPUCommand
{
    PrimitiveType type;
    size_t vertexCount;
    size_t vertexOffset = 0;
    GraphicsPipelineData** sepAd;
    std::vector<GraphicsPipelineData*>* pipes;;

    DrawCommand(PrimitiveType type, size_t vertexCount, GraphicsPipelineData** sep, std::vector<GraphicsPipelineData*>* pipes, size_t vertexOffset = 0) : type(type), vertexCount(vertexCount), pipes(pipes), vertexOffset(vertexOffset), sepAd(sep) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {

        if (sepAd) {
            auto sep = *sepAd;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->Draw(type, vertexCount, vertexOffset);
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);

                if (!sep->pl && !sep->plb) {
                    auto lastPipe = pipes->back();
                    if (!lastPipe) return;
                    sep = lastPipe;
                }

                if (!sep->pl) {
                    CreateDPS(graphics, sep);
                    sep->pl = sep->plb->Build();
                }
                auto ds = sep->pl->GetDescriptorSet(sep->descriptorSlotID);

                if (ds) {
                    ds->Update();
                    sep->descriptorSlotID++;

                    graphics->GetCommandList()->BindDescriptorSet(ds);
                }

                graphics->GetCommandList()->BindPipeline(sep->pl);
                graphics->GetCommandList()->Draw(type, vertexCount, vertexOffset);

                auto it = std::find(pipes->begin(), pipes->end(), sep);
                if (it == pipes->end()) {
                    pipes->push_back(sep);
                }

                *sepAd = new GraphicsPipelineData();
            
            }
        }
    }
};


// For immediate
class GraphicsSimple : protected GraphicsSystem {
public:
    GraphicsSimple(CommandVector* vector, const bnWindow& Window, std::vector<IResourceHandle**>* rel);
    ~GraphicsSimple() {}
    void BindShader(ResourceHandle<IShader>* shader);
    void BindBuffer(ResourceHandle<IBuffer>* buffer, u32 slot = 0);
    void BindTexture(ResourceHandle<ITexture>* texture, u32 slot = 0);
    void BindInputLayout(ResourceHandle<IInputLayout>* inputLayout);
    void BindSamplerState(ResourceHandle<ISamplerState>*, u32 = 0);
    void BindViewPort(ResourceHandle<IViewPort>*);
    void BindRasterizerState(ResourceHandle<IRasterizerState>*);
    void BindDepthStencilState(ResourceHandle<IDepthStencilState>*, UINT stencilRef = 0);
    void BindBlendState(ResourceHandle<IBlendState>*, const float blendFactor[4] = nullptr, UINT sampleMask = 0xFFFFFFFF);
    void BindRenderTarget(ResourceHandle<IRenderTarget>*, ResourceHandle<IDepthStencil>* = nullptr);
    void ClearRenderTarget(ResourceHandle<IRenderTarget>* target, const float color[4] = {});
    void ClearDepthStencil(ResourceHandle<IDepthStencil>* target, float depth = 0, UINT8 stencil = 0);
    void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0);
    void CopyToBuffer(ResourceHandle<IBuffer>* buffer, void* data, size_t size);
    void MapBufferMemory(ResourceHandle<IBuffer>* buffer, ResourceHandle<void>* dataPtr);

    void UnmapBufferMemory(ResourceHandle<IBuffer>* buffer);
    void CopyBufferToImage(ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc);
    void CopyImageToImage(ResourceHandle<ITexture>* srcTexture, ResourceHandle<ITexture>* dstTexture, ImageCopyDesc desc);

    
    bool alrRel = false;
    GraphicsPipelineData* pipe;
    std::vector<GraphicsPipelineData*> pipes;
private:
    const bnWindow& window;
protected:
    void Release();
    friend class bnGraphics;
};
