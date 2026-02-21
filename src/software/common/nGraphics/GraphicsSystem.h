#pragma once
#include "nGraphics/ImmediateGraphicsAbstract.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include <functional>

#include "nCommon/CommandList.h"
#include "nCommon/CommandListContainers.h"
#include "nCommon/CommandRegistry.h"

struct GraphicsPipelineData {
    IPipeline* pl;
    IPipelineBuilder* plb;
    ICommandPool* cp;
    ICommandBuffer* cb;
    IBlendState* blend;
    IRasterizerState* rast;
    IDepthStencilState* depth;
    IInputLayout* inputLayout;
    IDescriptorPool* dPool;
    IDescriptorSetLayout* dSetLayout;
    u32 descriptorSlotID = 0;

    u32 ID() const {
        std::hash<IPipeline*> ptrHash;
        std::hash<IBlendState*> blendHash;
        std::hash<IRasterizerState*> rastHash;
        std::hash<IDepthStencilState*> depthHash;
        std::hash<IInputLayout*> layoutHash;

        u32 hash = 0;
        hash ^= ptrHash(pl) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= blendHash(blend) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= rastHash(rast) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= depthHash(depth) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= layoutHash(inputLayout) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        return hash;
    }

    void Clear() {
        pl = nullptr;
        plb = nullptr;
        cp = nullptr;
        cb = nullptr;
        blend = nullptr;
        rast = nullptr;
        depth = nullptr;
        inputLayout = nullptr;
        dPool = nullptr;
        dSetLayout = nullptr;
        descriptorSlotID = 0;
    }
};

struct IGPUCommand : public ICommand
{
    ~IGPUCommand() override = default;
    virtual void Execute(IGraphicsDevice* device) = 0;
    void ExecuteStatic(void* context) override {
        Execute(static_cast<IGraphicsDevice *>(context));
    }
};

using GPUFunction = std::function<void(IGraphicsDevice*)>;
using CommandVector = MutexableVector<std::unique_ptr<IGPUCommand>>;//std::vector<std::unique_ptr<IGPUCommand>>;
using CommandVectorPlain = MutexableVector<IGPUCommand>; //std::vector<IGPUCommand*>;

class bnGraphics;

class GraphicsSystem : public CommandList<CommandVector> {
public:
    GraphicsSystem(CommandVector* vector, std::vector<IResourceHandle**>* rel) : CommandList(vector), relVariables(rel) {}
protected:
    friend class bnGraphics;
protected:
    std::vector<IResourceHandle**>* relVariables;
};

struct MapBufferMemoryCommand : public IGPUCommand
{
    ResourceHandle<IBuffer>* buffer;
    ResourceHandle<void>* dataPtr;

    MapBufferMemoryCommand(ResourceHandle<IBuffer>* buf, ResourceHandle<void>* data) : buffer(buf), dataPtr(data) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!buffer) return;
        if (!dataPtr) return;

        dataPtr->status = TO_BE_CREATED;
        if (buffer->Exists()) graphicsDevice->MapBufferMemory(buffer->Get(), dataPtr->GetAddress());
        dataPtr->status = CREATED;
    }
};

struct UnmapBufferMemoryCommand : public IGPUCommand
{
    ResourceHandle<IBuffer>* buffer;


    UnmapBufferMemoryCommand(ResourceHandle<IBuffer>* buf) : buffer(buf) {}

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!buffer) return;

        if (buffer->Exists()) graphicsDevice->UnmapBufferMemory(buffer->Get());
    }
};

struct CreateTextureCommand : public IGPUCommand
{
    TextureDesc desc;
    const void* initialData;
    ResourceHandle<ITexture>* handle;
    //ITexture** outTexture;

    CreateTextureCommand(const TextureDesc& d, const void* data, ResourceHandle<ITexture>* out)
        : desc(d), initialData(data), handle(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
       
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);

        //}

        if (!handle) return;
        handle->Resolve(graphicsDevice->CreateTexture(desc, initialData));
        handle->status = ResourceStatus::CREATED;
    }
};

struct CreateShaderCommand : public IGPUCommand
{
    ShaderDesc desc;
    //IShader** outShader;
    ResourceHandle<IShader>* handle;

    CreateShaderCommand(ShaderDesc d, ResourceHandle<IShader>* out)
        : desc(d), handle(out) {

    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (!handle) return;
        handle->Resolve(graphicsDevice->CreateShader(desc));
        handle->status = ResourceStatus::CREATED;
    }
};

struct CreateBufferCommand : public IGPUCommand
{
    BufferDesc desc;
    void* initialData = nullptr;
    ResourceHandle<IBuffer>* handle;

    CreateBufferCommand(const BufferDesc& d, const void* data, ResourceHandle<IBuffer>* out)
        : desc(d), handle(out) {
        // Copy data for the entire buffer lifespan since data parameter can not be trusted.
        if (data) {
            initialData = malloc(desc.size * desc.stride);
            if (initialData) {
                memcpy(initialData, data, desc.size * desc.stride);
            }
        }
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        handle->Resolve(graphicsDevice->CreateBuffer(desc, initialData));
        handle->status = ResourceStatus::CREATED;
        if(initialData != nullptr) free(initialData);
    }
};

struct CreateInputLayoutCommand : public IGPUCommand
{
    InputLayoutDesc desc;
    ResourceHandle<IInputLayout>* outLayout;

    CreateInputLayoutCommand(const InputLayoutDesc& d, ResourceHandle<IInputLayout>* out)
        : desc(d), outLayout(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);

        //}

        outLayout->Resolve(graphicsDevice->CreateInputLayout(desc));
        outLayout->status = CREATED;
    }
};


struct CreateSamplerStateCommand : public IGPUCommand
{
    SamplerStateDesc desc;
    ResourceHandle<ISamplerState>* outSampler;

    CreateSamplerStateCommand(const SamplerStateDesc& d, ResourceHandle<ISamplerState>* out)
        : desc(d), outSampler(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
        //   
        //}
        outSampler->Resolve(graphicsDevice->CreateSamplerState(desc));
        outSampler->status = CREATED;
    }
};

struct CreateViewPortCommand : public IGPUCommand
{
    ViewPortDesc desc;
    IViewPort** outView;

    CreateViewPortCommand(const ViewPortDesc& d, IViewPort** out)
        : desc(d), outView(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
            auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
            *outView = graphics->CreateViewPort(desc);
          
        }
    }
};

struct CreateRasterizerStateCommand : public IGPUCommand
{
    RasterizerDesc desc;
    ResourceHandle<IRasterizerState>* outRast;

    CreateRasterizerStateCommand(const RasterizerDesc& d, ResourceHandle<IRasterizerState>* out)
        : desc(d), outRast(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
        //    
        //}
        outRast->Resolve(graphicsDevice->CreateRasterizerState(desc));
        outRast->status = CREATED;
    }
};


struct CreateDepthStencilStateCommand : public IGPUCommand
{
    DepthStencilDesc desc;
    ResourceHandle<IDepthStencilState>* outDepthState;

    CreateDepthStencilStateCommand(const DepthStencilDesc& d, ResourceHandle<IDepthStencilState>* out)
        : desc(d), outDepthState(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
        //    
        //}
        outDepthState->Resolve(graphicsDevice->CreateDepthStencilState(desc));
        outDepthState->status = CREATED;
    }
};



struct CreateBlendStateCommand : public IGPUCommand
{
    BlendStateDesc desc;
    ResourceHandle<IBlendState>* outBlend;

    CreateBlendStateCommand(const BlendStateDesc& d, ResourceHandle<IBlendState>* out)
        : desc(d), outBlend(out) {
    }

    void Execute(IGraphicsDevice* graphicsDevice) override
    {
        //if (graphicsDevice->GetFlags() & GraphicsDeviceFlags::IS_IMMEDIATE) {
        //    auto graphics = (IGraphicsDeviceImmediate*)(graphicsDevice);
        //    
        //}
        outBlend->Resolve(graphicsDevice->CreateBlendState(desc));
        outBlend->status = CREATED;
    }
};

struct FunctionCommand : public IGPUCommand {
    GPUFunction func;
    FunctionCommand(GPUFunction f) : func(std::move(f)) {}
    void Execute(IGraphicsDevice* graphics) override {
        func(graphics);
    }
};



#pragma pack(push, 1)
struct PushGroupCommandData
{
    const char* name;
    uint32_t color;
};
#pragma pack(pop)

using namespace CommandCategories;

struct PushGroupCommand : public IGPUCommand {
    PushGroupCommandData data;

    PushGroupCommand(const PushGroupCommandData& src)
        : data(src) {}

    void Execute(IGraphicsDevice* graphics) override {
        graphics->PushGroup(data.name, data.color);
    }
};

REGISTER_LIST_COMMAND(NativeGraphics, PushGroupCommand);

struct SetMarkerCommand : public IGPUCommand {
    const char* name;
    uint32_t color;
    SetMarkerCommand(const char* name, uint32_t color = 0xFFFFFFFF) : name(name), color(color) {}
    void Execute(IGraphicsDevice* graphics) override {
        graphics->SetMarker(name, color);
    }
};

struct PopGroupCommand : public IGPUCommand {
    PopGroupCommand() {}
    void Execute(IGraphicsDevice* graphics) override {
        graphics->PopGroup();
    }
};
