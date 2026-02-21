#pragma once
#include "GraphicsSystem.h"
#include "nGraphics/bnShader.h"
#include <iostream>

class PipelineImmediate;

class DescriptorSetImmediate : public IDescriptorSet {
public:
    CommandVector* vector;

    PipelineImmediate* pipeline;

    std::vector<IBuffer*> buffers;
    std::vector<ITexture*> textures;
    std::vector<ISamplerState*> samplers;

    DescriptorSetImmediate(PipelineImmediate* parentP) : pipeline(parentP) {}
    ~DescriptorSetImmediate() {};

    virtual void SetBuffer(uint32_t slot, IBuffer* buffer) override {
        if (!buffer) return;

        if (slot >= buffers.size()) buffers.resize(slot + 1, nullptr);

        buffers[slot] = static_cast<IBuffer*>(buffer);
    }
    virtual void SetTexture(uint32_t slot, ITexture* texture) override {
        if (!texture) return;

        if (slot >= textures.size()) textures.resize(slot + 1, nullptr);

        textures[slot] = static_cast<ITexture*>(texture);
    }
    virtual void SetSampler(uint32_t slot, ISamplerState* sampler) override {
        if (!sampler) return;

        if (slot >= samplers.size()) samplers.resize(slot + 1, nullptr);

        samplers[slot] = static_cast<ISamplerState*>(sampler);
    }
    virtual void Update() override {
        return;
    }
};



struct DescriptorSetSetBuffer : public IGPUCommand
{
    ResourceHandle<IDescriptorSet>* objectHandle = nullptr;
    u32 slot;
    ResourceHandle<IBuffer>* bufferHandle = nullptr;

    DescriptorSetSetBuffer(ResourceHandle<IDescriptorSet>* object, u32 slot, ResourceHandle<IBuffer>* buffer) : objectHandle(object), slot(slot), bufferHandle(buffer) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!objectHandle || !bufferHandle) return;

        auto object = objectHandle->Get();
        if (!object) return;

        object->SetBuffer(slot, bufferHandle->Get());
    }
};

struct DescriptorSetSetTexture : public IGPUCommand
{
    ResourceHandle<IDescriptorSet>* objectHandle = nullptr;
    u32 slot;
    ResourceHandle<ITexture>* textureHandle = nullptr;

    DescriptorSetSetTexture(ResourceHandle<IDescriptorSet>* object, u32 slot, ResourceHandle<ITexture>* texture) : objectHandle(object), slot(slot), textureHandle(texture) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!objectHandle || !textureHandle) return;

        auto object = objectHandle->Get();
        if (!object) return;

        object->SetTexture(slot, textureHandle->Get());
    }
};


struct DescriptorSetSetSampler : public IGPUCommand
{
    ResourceHandle<IDescriptorSet>* objectHandle = nullptr;
    u32 slot;
    ResourceHandle<ISamplerState>* samplerStateHandle = nullptr;

    DescriptorSetSetSampler(ResourceHandle<IDescriptorSet>* object, u32 slot, ResourceHandle<ISamplerState>* samplerState) : objectHandle(object), slot(slot), samplerStateHandle(samplerState) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!objectHandle || !samplerStateHandle) return;

        auto object = objectHandle->Get();
        if (!object) return;

        object->SetSampler(slot, samplerStateHandle->Get());
    }
};



struct UpdateDescriptorSetCommand : public IGPUCommand
{
    ResourceHandle<IDescriptorSet>* objectHandle;

    UpdateDescriptorSetCommand(ResourceHandle<IDescriptorSet>* object) : objectHandle(object) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!objectHandle) return;

        auto object = objectHandle->Get();
        if (!object) return;

        object->Update();
    }
};


class PipelineImmediate : public IPipeline {
public:
    CommandVector* vector;

    std::map<u32, DescriptorSetImmediate*> descriptorSets;
    IRenderTarget* renderTarget;
    std::vector<IShader*> shaders;
    IInputLayout* inputLayout = nullptr;
    IRasterizerState* rasterizer = nullptr;
    IDepthStencilState* depthStencil = nullptr;
    IBlendState* blendState = nullptr;
    IDescriptorSetLayout* descriptorSetLayout = nullptr;
    IDescriptorPool* descriptorPool = nullptr;
    IRenderTarget* renderPass = nullptr;

    void* GetNativeHandle() { return nullptr; }

    PipelineImmediate() {};

    PipelineImmediate(
        IDescriptorSetLayout* setLayout,
        IDescriptorPool* pool,
        std::vector<IShader*> shaders,
        IInputLayout* inputLayout,
        IRasterizerState* rasterizer,
        IDepthStencilState* depthStencil,
        IBlendState* blendState,
        IRenderTarget* renderTarget,
        CommandVector* vector
    )
        : descriptorSetLayout(setLayout), descriptorPool(pool), renderTarget(renderTarget), shaders(shaders), inputLayout(inputLayout), rasterizer(rasterizer), depthStencil(depthStencil), blendState(blendState), renderPass(renderPass), vector(vector)
    {}

~PipelineImmediate() {
    Release();
}

void Release() {
    shaders.clear();
    shaders.shrink_to_fit();
    for (auto& des : descriptorSets) {
        delete des.second;
    }
    descriptorSets.clear();
}

IDescriptorSet* CreateDescriptorSet(u32 slot = 0) override;

IDescriptorSet* GetDescriptorSet(u32 slot = 0) {
    ResourceHandle<IDescriptorSet>* res = new ResourceHandle<IDescriptorSet>();

    auto it = descriptorSets.find(slot);
    if (it != descriptorSets.end()) {
        res->Resolve(it->second);
        return res->Get();
    }
    else return nullptr;
}
};



struct PipelineCreateDescriptorSetCommand : public IGPUCommand
{
    ResourceHandle<IPipeline>* objectHandle = nullptr;
    u32 slot;
    ResourceHandle<IDescriptorSet>* res = nullptr;

    PipelineCreateDescriptorSetCommand(ResourceHandle<IPipeline>* object, u32 slot, ResourceHandle<IDescriptorSet>* res) : objectHandle(object), slot(slot), res(res) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!objectHandle) return;
        auto object = objectHandle->Get();
        if (!object) return;

        res->Resolve(object->CreateDescriptorSet(slot));
    }
};


    class PipelineBuilderImmediate : public IPipelineBuilder {
    public:
        PipelineBuilderImmediate(CommandVector* vector) : vector(vector) {}

        PipelineBuilderImmediate& AddShader(IShader* shader) override;

        PipelineBuilderImmediate& SetInputLayout(IInputLayout* layout) override;

        PipelineBuilderImmediate& SetRasterizer(IRasterizerState* raster) override;

        PipelineBuilderImmediate& SetDepthStencil(IDepthStencilState* depth) override;

        PipelineBuilderImmediate& SetBlendState(IBlendState* blend) override;

        PipelineBuilderImmediate& SetDescriptorPool(IDescriptorPool* pool) override;

        PipelineBuilderImmediate& SetDescriptorSetLayout(IDescriptorSetLayout* layout) override;

        PipelineBuilderImmediate& SetRenderTarget(IRenderTarget* target) {
            //auto vRenderTarget = dynamic_cast<IRenderTarget*>(target);

            //if (!vRenderTarget) return *this;

            //renderPass = vRenderTarget;
            return *this;
        };



    sVec<IShader*>* GetShaders() {
        return &shaders;
    }

    // todo: redesign this to be a resource handle
    IPipeline* Build() override;

public:
    CommandVector* vector;
    std::vector<IShader*> shaders;
    IInputLayout* inputLayout = nullptr;
    IRasterizerState* rasterizer = nullptr;
    IDepthStencilState* depthStencil = nullptr;
    IBlendState* blendState = nullptr;
    IDescriptorSetLayout* descriptorSetLayout = nullptr;
    IDescriptorPool* descriptorPool = nullptr;
    IRenderTarget* renderPass = nullptr;
};



    struct PipelineBuildCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IPipeline>* pipeline = nullptr;

        PipelineBuildCommand(ResourceHandle<IPipelineBuilder>* object, ResourceHandle<IPipeline>* pipeline) : objectHandle(object), pipeline(pipeline) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            auto object = objectHandle->Get();
            if (!object) return;

            pipeline->Resolve(object->Build());
            objectHandle->DestroyHandle();
            objectHandle = nullptr;
        }
    };


    struct PipelineAddShaderCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IShader>* shader = nullptr;

        PipelineAddShaderCommand(ResourceHandle<IShader>* shader, ResourceHandle<IPipelineBuilder>* object) : shader(shader), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!shader || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            object->AddShader(shader->Get());
        }
    };

    struct PipelineSetInputLayoutCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IInputLayout>* layout = nullptr;

        PipelineSetInputLayoutCommand(ResourceHandle<IInputLayout>* layout, ResourceHandle<IPipelineBuilder>* object) : layout(layout), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!layout || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            object->SetInputLayout(layout->Get());
        }
    };

    struct PipelineSetRasterizerCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IRasterizerState>* rast = nullptr;

        PipelineSetRasterizerCommand(ResourceHandle<IRasterizerState>* rast, ResourceHandle<IPipelineBuilder>* object) : rast(rast), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!rast || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            
            object->SetRasterizer(rast->Get());
        }
    };

    struct PipelineSetDepthStencilCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IDepthStencilState>* depth = nullptr;

        PipelineSetDepthStencilCommand(ResourceHandle<IDepthStencilState>* depth, ResourceHandle<IPipelineBuilder>* object) : depth(depth), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!depth || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            object->SetDepthStencil(depth->Get());
        }
    };

    struct PipelineSetBlendStateCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IBlendState>* blend = nullptr;

        PipelineSetBlendStateCommand(ResourceHandle<IBlendState>* blend, ResourceHandle<IPipelineBuilder>* object) : blend(blend), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!blend || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            object->SetBlendState(blend->Get());
        }
    };

    struct PipelineSetDescriptorPoolCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IDescriptorPool>* pool = nullptr;

        PipelineSetDescriptorPoolCommand(ResourceHandle<IDescriptorPool>* pool, ResourceHandle<IPipelineBuilder>* object) : pool(pool), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!pool || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

            object->SetDescriptorPool(pool->Get());
        }
    };

    struct PipelineSetDescriptorSetLayoutCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* objectHandle = nullptr;
        ResourceHandle<IDescriptorSetLayout>* layout = nullptr;

        PipelineSetDescriptorSetLayoutCommand(ResourceHandle<IDescriptorSetLayout>* layout, ResourceHandle<IPipelineBuilder>* object) : layout(layout), objectHandle(object) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!layout || !objectHandle) return;

            auto object = objectHandle->Get();
            if (!object) return;

        
            object->SetDescriptorSetLayout(layout->Get());
        }
    };
    
    struct GetCommandListCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;


        GetCommandListCommand(ResourceHandle<ICommandList>* list) : list(list) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                list->Resolve(graphics->GetCommandList());
            }
        }
    };

    struct BindPipelineCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        ResourceHandle<IPipeline>* pipeline = nullptr;

        BindPipelineCommand(ResourceHandle<ICommandList>* list, ResourceHandle<IPipeline>* pipeline) : list(list), pipeline(pipeline) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                // yeah idk
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                auto pipelineImm = (PipelineImmediate*)pipeline->Get();
                // bind everything automatically here
                graphics->BindInputLayout(pipelineImm->inputLayout);
                for (auto shader : pipelineImm->shaders) {
                    graphics->BindShader(shader);
                }
                graphics->BindBlendState(pipelineImm->blendState, nullptr);
                graphics->BindDepthStencilState(pipelineImm->depthStencil);
                graphics->BindRasterizerState(pipelineImm->rasterizer);
                
                if(pipelineImm->renderTarget) graphics->BindRenderTarget(pipelineImm->renderTarget);
                return;
            }
            else {
                if (!pipeline || !list) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->BindPipeline(pipeline->Get());
                }
            }
        }
    };

    struct BindDescriptorSetCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        ResourceHandle<IDescriptorSet>* set = nullptr;
        u32 index = 0;

        BindDescriptorSetCommand(ResourceHandle<ICommandList>* list, ResourceHandle<IDescriptorSet>* set, uint32_t index = 0) : list(list), set(set), index(index) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                auto setImm = (DescriptorSetImmediate*)set->Get();
           
                for (uint32_t i = 0; i < setImm->buffers.size(); ++i) {
                    auto buffer = setImm->buffers[i];
                    if (!buffer) continue;
                    graphics->BindBuffer(buffer); // todo: add slot?
                }
                 
                for (uint32_t i = 0; i < setImm->textures.size(); ++i) {
                    auto texture = setImm->textures[i];
                    if (!texture) continue;
                    graphics->BindTexture(texture, i);
                }

                for (uint32_t i = 0; i < setImm->samplers.size(); ++i) {
                    auto samplers = setImm->samplers[i];
                    if (!samplers) continue;
                    graphics->BindSamplerState(samplers, i);
                }
                
                return;
            }
            else {

                if (!list || !set) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->BindDescriptorSet(set->Get(), index);
                }
            }
        }
    };

    struct ListBindViewPortCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        ResourceHandle<IViewPort>* port = nullptr;
        
        ListBindViewPortCommand(ResourceHandle<ICommandList>* list, ResourceHandle<IViewPort>* port) : list(list), port(port) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                // todo: immediates should also get the same port design as explicit
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindViewPort(port->Get());
                return;
            }
            else {

                if (!list || !port) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->BindViewPort(port->Get());
                }
            }
        }
    };


    struct ListBindScissorCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        ResourceHandle<IViewPort>* port = nullptr;

        ListBindScissorCommand(ResourceHandle<ICommandList>* list, ResourceHandle<IViewPort>* port) : list(list), port(port) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindViewPort(port->Get());
                return;
            }
            else {

                if (!list || !port) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->BindScissor(port->Get());
                }
            }
        }
    };

    struct ListBindBufferCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        ResourceHandle<IBuffer>* buffer = nullptr;

        ListBindBufferCommand(ResourceHandle<ICommandList>* list, ResourceHandle<IBuffer>* buffer) : list(list), buffer(buffer) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->BindBuffer(buffer->Get());
                return;
            }
            else {

                if (!list || !buffer) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->BindBuffer(buffer->Get());
                }
            }
        }
    };

    struct ListDrawCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        PrimitiveType type;
        size_t vertexCount;
        size_t vertexOffset = 0;

        ListDrawCommand(ResourceHandle<ICommandList>* list, PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) : list(list), type(type), vertexCount(vertexCount), vertexOffset(vertexOffset) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->Draw(type, vertexCount, vertexOffset);
                return;
            }
            else {

                if (!list) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->Draw(type, vertexCount, vertexOffset);
                }
            }
        }
    };

    struct ListDrawIndexedCommand : public IGPUCommand
    {
        ResourceHandle<ICommandList>* list = nullptr;
        PrimitiveType type;
        ResourceHandle<IBuffer>* indexBuffer = nullptr;
        size_t indexCount;
        size_t indexOffset = 0;

        ListDrawIndexedCommand(ResourceHandle<ICommandList>* list, PrimitiveType type, ResourceHandle<IBuffer>* indexBuffer, size_t indexCount, size_t indexOffset = 0) : list(list), type(type), indexBuffer(indexBuffer), indexCount(indexCount), indexOffset(indexOffset) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!indexBuffer) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                graphics->DrawIndexed(type, indexBuffer->Get(), indexCount, indexOffset);
                return;
            }
            else {

                if (!list) return;

                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (list->Exists()) {
                    list->Get()->DrawIndexed(type, indexBuffer->Get(), indexCount, indexOffset);
                }
            }
        }
    };

    struct CreatePipelineBuilderCommand : public IGPUCommand
    {
        ResourceHandle<IPipelineBuilder>* builder;
        CommandVector* vector;
        CreatePipelineBuilderCommand(ResourceHandle<IPipelineBuilder>* builder, CommandVector* vector) : builder(builder), vector(vector) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!builder) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                builder->Resolve(new PipelineBuilderImmediate(vector));
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                builder->Resolve(graphics->CreatePipelineBuilder());
            }
        }
    };

    struct CreateCommandPoolCommand : public IGPUCommand
    {
        ResourceHandle<ICommandPool>* pool = nullptr;
        CommandPoolDesc& desc;

        CreateCommandPoolCommand(ResourceHandle<ICommandPool>* pool, CommandPoolDesc& desc) : pool(pool) , desc(desc){}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!pool) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                // todo: work on a version for immediate devices
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                pool->Resolve(graphics->CreateCommandPool(desc));
            }
        }
    };

    struct CreateDescriptorPoolCommand : public IGPUCommand
    {
        ResourceHandle<IDescriptorPool>* pool = nullptr;
        DescriptorPoolDesc& desc;

        CreateDescriptorPoolCommand(ResourceHandle<IDescriptorPool>* pool, DescriptorPoolDesc& desc) : pool(pool), desc(desc) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!pool) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                // todo: work on a version for immediate devices
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                pool->Resolve(graphics->CreateDescriptorPool(desc));
            }
        }
    };

    struct CreateDescriptorSetLayoutCommand : public IGPUCommand
    {
        ResourceHandle<IDescriptorSetLayout>* layout;
        DescriptorSetLayoutDesc& desc;

        CreateDescriptorSetLayoutCommand(ResourceHandle<IDescriptorSetLayout>* layout, DescriptorSetLayoutDesc& desc) : layout(layout), desc(desc) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!layout) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                // todo: work on a version for immediate devices
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                layout->Resolve(graphics->CreateDescriptorSetLayout(desc));
            }
        }
    };

    class CommandBufferImmediate : public ICommandBuffer {
    public:
        void* GetNativeHandle() override { return nullptr; };
        void PipelineBarrier(
            ITexture* image,
            ImageLayout oldLayout,
            ImageLayout newLayout,
            ImageAccessLayout srcAccessMask,
            ImageAccessLayout dstAccessMask) override { }

        void PipelineBarrierBatched(
            ITexture* image,
            ImageLayout oldLayout,
            ImageLayout newLayout,
            ImageAccessLayout srcAccessMask,
            ImageAccessLayout dstAccessMask) override {
        }

        void FlushBatchedBarriers() override {}

    };

    struct BeginSingleTimeCommandsCommand : public IGPUCommand
    {
        ResourceHandle<ICommandBuffer>* buffer;
        ResourceHandle<ICommandPool>* pool;

        BeginSingleTimeCommandsCommand(ResourceHandle<ICommandBuffer>* buffer, ResourceHandle<ICommandPool>* pool) : buffer(buffer), pool(pool) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!buffer || !pool) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                buffer->Resolve(new CommandBufferImmediate());
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                buffer->Resolve(graphics->BeginSingleTimeCommands(pool->Get()));
            }
        }
    };


    struct EndSingleTimeCommandsCommand : public IGPUCommand
    {
        ResourceHandle<ICommandBuffer>* buffer = nullptr;
    

        EndSingleTimeCommandsCommand(ResourceHandle<ICommandBuffer>* buffer) : buffer(buffer) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!buffer) return;

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                delete buffer;
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                graphics->EndSingleTimeCommands(buffer->Get());
                graphics->ReleaseOnPend(buffer);
            }
        }
    };

    struct CommandBufferPipelineBarrierCommand : public IGPUCommand
    {
        ResourceHandle<ICommandBuffer>* buffer;
        ResourceHandle<ITexture>* image;
        ImageLayout oldLayout;
        ImageLayout newLayout;
        ImageAccessLayout srcAccessMask;
        ImageAccessLayout dstAccessMask;

        CommandBufferPipelineBarrierCommand(ResourceHandle<ICommandBuffer>* buffer, ResourceHandle<ITexture>* image, ImageLayout oldLayout,ImageLayout newLayout, ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) : buffer(buffer), image(image), oldLayout(oldLayout), newLayout(newLayout), srcAccessMask(srcAccessMask), dstAccessMask(dstAccessMask) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!buffer || !image)  return;
            auto bufferObj = buffer->Get();
            if (!bufferObj) return;


            bufferObj->PipelineBarrier(image->Get(), oldLayout, newLayout, srcAccessMask, dstAccessMask);
        }
    };



    struct CopyBufferToImageCommandAdvanced : public IGPUCommand
    {
        ResourceHandle<IBuffer>* srcBuffer;
        ResourceHandle<ITexture>* dstTexture;
        BufferImageCopyDesc desc;
        ResourceHandle<ICommandBuffer>* cBuffer;

        CopyBufferToImageCommandAdvanced(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc) : cBuffer(cBuffer), srcBuffer(srcBuffer), dstTexture(dstTexture), desc(desc) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {

            if (!srcBuffer || !dstTexture || !cBuffer) return;

            if (srcBuffer->Exists() && dstTexture->Exists()) {
                if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                    graphics->CopyBufferToImage(srcBuffer->Get(), dstTexture->Get(), desc);
                }
                else {
                    auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);

                    auto copyCmd = cBuffer->Get();

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
                }
            }
        }
    };


    struct CopyImageToImageCommandAdvanced : public IGPUCommand
    {
        ResourceHandle<ICommandBuffer>* cBuffer;
        ResourceHandle<ITexture>* srcTexture;
        ResourceHandle<ITexture>* dstTexture;
        ImageCopyDesc desc;

        CopyImageToImageCommandAdvanced(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<ITexture>* srcTexture, ResourceHandle<ITexture>* dstTexture, ImageCopyDesc desc) : cBuffer(cBuffer), srcTexture(srcTexture), dstTexture(dstTexture), desc(desc) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!srcTexture || !dstTexture || !cBuffer) return;

            if (srcTexture->Get() && dstTexture->Get()) {
                if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                    graphics->CopyImageToImage(srcTexture->Get(), dstTexture->Get(), desc);
                }
                else {
                    auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);

                    auto copyCmd = cBuffer->Get();

                    auto oldSrcLayout = srcTexture->Get()->explicitLayout;
                    auto oldDstLayout = dstTexture->Get()->explicitLayout;

                    copyCmd->PipelineBarrierBatched(
                        srcTexture->Get(),
                        oldSrcLayout,            // old layout
                        ImageLayout::CopySrc,            // new layout
                        ImageAccessLayout::None,         // src access
                        ImageAccessLayout::CopySrc       // dst access
                    );

                    // Prepare cached texture as copy destination
                    copyCmd->PipelineBarrierBatched(
                        dstTexture->Get(),
                        oldDstLayout,          // old layout
                        ImageLayout::CopyDst,            // new layout
                        ImageAccessLayout::None,
                        ImageAccessLayout::CopyDst
                    );

                    copyCmd->FlushBatchedBarriers();

                    graphics->CopyImageToImage(copyCmd, srcTexture->Get(), dstTexture->Get(), desc);

                    copyCmd->PipelineBarrierBatched(
                        srcTexture->Get(),
                        srcTexture->Get()->explicitLayout,          // old layout
                        oldSrcLayout,            // new layout
                        ImageAccessLayout::CopyDst,         // src access mask (nothing yet)
                        ImageAccessLayout::None       // dst access mask
                    );

                    copyCmd->PipelineBarrierBatched(
                        dstTexture->Get(),
                        dstTexture->Get()->explicitLayout,          // old layout
                        ImageLayout::ShaderRead,            // new layout
                        ImageAccessLayout::CopyDst,         // src access mask (nothing yet)
                        ImageAccessLayout::None       // dst access mask
                    );

                    copyCmd->FlushBatchedBarriers();

                }
            }
        }
    };


    struct CopyToBufferCommandAdvanced : public IGPUCommand
    {
        ResourceHandle<IBuffer>* buffer;
        ResourceHandle<ICommandBuffer>* pool;
        void* data;
        size_t size;

        CopyToBufferCommandAdvanced(ResourceHandle<IBuffer>* buffer, ResourceHandle<ICommandBuffer>* pool, void* data, size_t size) : buffer(buffer), pool(pool), data(data), size(size)  {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {

            if (!buffer || !pool || !data) return;

   
                if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
                    graphics->CopyToBuffer(buffer->Get(), data, size);
                }
                else {
                    auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);

                    graphics->CopyToBuffer(buffer->Get(), pool->Get(), data, size);
                }
            
        }
    };


    struct ReleaseDescriptorPoolCommand : public IGPUCommand
    {
        ResourceHandle<IDescriptorPool>* pool;

        ReleaseDescriptorPoolCommand(ResourceHandle<IDescriptorPool>* pool) : pool(pool) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (!pool) return;

                graphics->ReleaseDescriptorPool(pool->GetAddress());
            }
        }
    };

    struct ReleaseDescriptorSetLayoutCommand : public IGPUCommand
    {
        ResourceHandle<IDescriptorSetLayout>* layout;

        ReleaseDescriptorSetLayoutCommand(ResourceHandle<IDescriptorSetLayout>* layout) : layout(layout) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                if (!layout) return;

                graphics->ReleaseDescriptorSetLayout(layout->GetAddress());
            }
        }
    };


    struct ReleasePipelineCommand : public IGPUCommand
    {
        ResourceHandle<IPipeline>* pipeline;

        ReleasePipelineCommand(ResourceHandle<IPipeline>* pipe) : pipeline(pipe) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!pipeline) return;

            auto pipe = pipeline->Get();

            if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
                ((PipelineImmediate*)pipe)->~PipelineImmediate();
                pipeline->Invalidate();
                pipeline->status = DESTROYED;
                return;
            }
            else {
                auto graphics = (IGraphicsDeviceExplicit*)(graphicsDevice);
                pipe->Release();
                pipeline->MoveAsPrevious();
                pipeline->status = DESTROYED;
            }
        }
    };

    struct ReleaseDescriptorSetCommand : public IGPUCommand
    {
        ResourceHandle<IDescriptorSet>* set;

        ReleaseDescriptorSetCommand(ResourceHandle<IDescriptorSet>* set) : set(set) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!set) return;

            //auto setObj = set->Get();
            //if (setObj) {
            //    std::cout << "WARNING: An object is still active with this descriptor set, it will be ignored\n";
            //    return;
            //}

            set->DestroyHandle();
        }
    };

    class GraphicAdvancedCommandList {
    private:
        CommandVector* vector;
        ResourceHandle<ICommandList> list;
    public:

        GraphicAdvancedCommandList(CommandVector* vector) : vector(vector) {
            vector->push_back(std::make_unique<GetCommandListCommand>(&list));
        }



        ResourceHandle<IPipeline>* CreatePipeline();

        // Pipeline
        void BindPipeline(ResourceHandle<IPipeline>* pipeline) {
            vector->push_back(std::make_unique<BindPipelineCommand>(&list, pipeline));
        }

        // Descriptor sets (buffers, textures, samplers)
        void BindDescriptorSet(ResourceHandle<IDescriptorSet>* set, uint32_t index = 0) {
            vector->push_back(std::make_unique<BindDescriptorSetCommand>(&list, set, index));
        }

        // Viewport / dynamic states
        void BindViewPort(ResourceHandle<IViewPort>* port) {
            vector->push_back(std::make_unique<ListBindViewPortCommand>(&list, port));
        }
        void BindScissor(ResourceHandle<IViewPort>* port) {
            vector->push_back(std::make_unique<ListBindScissorCommand>(&list, port));
        }
        void BindBuffer(ResourceHandle<IBuffer>* buffer) {
            vector->push_back(std::make_unique<ListBindBufferCommand>(&list, buffer));
        }

        // Drawing
        void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) {
            vector->push_back(std::make_unique<ListDrawCommand>(&list, type, vertexCount, vertexOffset));
        }
        void DrawIndexed(PrimitiveType type, ResourceHandle<IBuffer>* indexBuffer, size_t indexCount, size_t indexOffset = 0) {
            vector->push_back(std::make_unique<ListDrawIndexedCommand>(&list, type, indexBuffer, indexCount, indexOffset));
        }
    };


    struct ReleaseCommandListCommand : public IGPUCommand
    {
        GraphicAdvancedCommandList* list;

        ReleaseCommandListCommand(GraphicAdvancedCommandList* list) : list(list) {}

        void Execute(IGraphicsDevice* graphicsDevice) override
        {
            if (!list) return;

            delete list;
        }
    };

    class bnWindow;

    /// <summary>
    /// Advanced is much more reliant on this class's functions than using the actual resource functions for timing, with alot more specific calls and timing into objects
    /// Advanced allows for both immediate and explicit objects, with it being more targetted for explicit graphic devices rather than immediate objects
    /// Immediate objects are supported but unlike simple which translates your immediate designed calls to explicit calls, it creates it's own immediate classes to replicate 
    /// the behaviour of the object.
    /// </summary>
    class GraphicsAdvanced : public GraphicsSystem
    {
    public:
        GraphicsAdvanced(CommandVector* vector, const bnWindow& Window, std::vector<IResourceHandle**>* rel)
            : GraphicsSystem(vector, rel), window(Window)
        {
        }

        ~GraphicsAdvanced() {}
 
        GraphicAdvancedCommandList* GetCommandList();

        ResourceHandle<IPipelineBuilder>* CreatePipelineBuilder(ResourceHandle<IPipelineBuilder>* resource = nullptr);
        ResourceHandle<ICommandPool>* CreateCommandPool(CommandPoolDesc desc, ResourceHandle<ICommandPool>* resource = nullptr);
        ResourceHandle<IDescriptorPool>* CreateDescriptorPool(DescriptorPoolDesc desc, ResourceHandle<IDescriptorPool>* resource = nullptr);
        ResourceHandle<IDescriptorSetLayout>* CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc, ResourceHandle<IDescriptorSetLayout>* resource = nullptr);
        ResourceHandle<IPipeline>* CreatePipeline(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IPipeline>* resource = nullptr);
        ResourceHandle<IDescriptorSet>* CreateDescriptorSet(ResourceHandle<IPipeline>* pipeline = nullptr, u32 slot = 0, ResourceHandle<IDescriptorSet>* resource = nullptr);
       
        void SetDescriptorSetBuffer(ResourceHandle<IDescriptorSet>* set, ResourceHandle<IBuffer>* buffer, u32 slot = 0);
        void SetDescriptorSetTexture(ResourceHandle<IDescriptorSet>* set, ResourceHandle<ITexture>* buffer, u32 slot = 0);
        void SetDescriptorSetSamplerState(ResourceHandle<IDescriptorSet>* set, ResourceHandle<ISamplerState>* buffer, u32 slot = 0);
        void UpdateDescriptorSet(ResourceHandle<IDescriptorSet>* set);

        ResourceHandle<ICommandBuffer>* BeginSingleTimeCommands(ResourceHandle<ICommandPool>* pool, ResourceHandle<ICommandBuffer>* resource = nullptr);
        void EndSingleTimeCommands(ResourceHandle<ICommandBuffer>* buffer);

        void ReleaseCommandPool(ResourceHandle<ICommandPool>* pool);
        void ReleaseDescriptorPool(ResourceHandle<IDescriptorPool>* pool);
        void ReleaseDescriptorSetLayout(ResourceHandle<IDescriptorSetLayout>* layout);
        void ReleasePipeline(ResourceHandle<IPipeline>* pipeline);
        void ReleaseCommandList(GraphicAdvancedCommandList* list);
        void ReleaseDescriptorSet(ResourceHandle<IDescriptorSet>* set);

        void CopyToBuffer(ResourceHandle<IBuffer>* buffer, ResourceHandle<ICommandBuffer>* pool, void* data, size_t size);
        void MapBufferMemory(ResourceHandle<IBuffer>* buffer, ResourceHandle<void>* dataPtr);
        void UnmapBufferMemory(ResourceHandle<IBuffer>* buffer);
        // void MemoryCopy(ResourceHandle<void>* dataPtr, const void* src, size_t srcSize, size_t offset = 0);
        void CopyBufferToImage(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc);
        void CopyImageToImage(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<ITexture>* srcBuffer, ResourceHandle<ITexture>* dstBuffer, ImageCopyDesc desc);


        void BuilderAddShader(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IShader>* shader);
        void BuilderSetInputLayout(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IInputLayout>* layout);
        void BuilderSetRasterizer(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IRasterizerState>* raster);
        void BuilderSetDepthStencil(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDepthStencilState>* depth);
        void BuilderSetBlendState(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IBlendState>* blend);
        void BuilderSetDescriptorPool(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDescriptorPool>* pool);
        void BuilderSetDescriptorSetLayout(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDescriptorSetLayout>* layout);
        void BuilderSetRenderTarget(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IRenderTarget>* target);

        void CommandBufferPipelineBarrier(ResourceHandle<ICommandBuffer>* buffer, ResourceHandle<ITexture>* image, ImageLayout oldLayout, ImageLayout newLayout, ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask);


       private:
        const bnWindow& window;
    };
