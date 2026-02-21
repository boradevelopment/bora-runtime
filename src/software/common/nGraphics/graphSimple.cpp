#include "graphSimple.h"
#include "nWindow/bnWindow.h"

GraphicsSimple::GraphicsSimple(CommandVector* vector, const bnWindow& Window, std::vector<IResourceHandle**>* rel) : window(Window), GraphicsSystem(vector, rel), pipe(nullptr)
{
    //if (window.usingExpl) {
        pipe = new GraphicsPipelineData();
    //}
}
void GraphicsSimple::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset) {
    commands->push_back(std::make_unique<DrawCommand>(type, vertexCount, &pipe,&pipes, vertexOffset));
}
void GraphicsSimple::CopyToBuffer(ResourceHandle<IBuffer>* buffer, void* data, size_t size)
{
    commands->push_back(std::make_unique<CopyToBufferCommand>(buffer, data, size, &pipe));
}
void GraphicsSimple::MapBufferMemory(ResourceHandle<IBuffer>* buffer, ResourceHandle<void>* dataPtr)
{
    auto cmd = new MapBufferMemoryCommand(buffer, dataPtr);
    cmd->Execute(window.rootDevice);
    delete cmd;
}
void GraphicsSimple::UnmapBufferMemory(ResourceHandle<IBuffer>* buffer)
{
    auto cmd = new UnmapBufferMemoryCommand(buffer);
    cmd->Execute(window.rootDevice);
    delete cmd;
}
void GraphicsSimple::CopyBufferToImage(ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc)
{
    commands->push_back(std::make_unique<CopyBufferToImageCommand>(srcBuffer, dstTexture, desc, &pipe));
}
void GraphicsSimple::CopyImageToImage(ResourceHandle<ITexture>* srcTexture, ResourceHandle<ITexture>* dstTexture, ImageCopyDesc desc)
{
    commands->push_back(std::make_unique<CopyImageToImageCommand>(srcTexture, dstTexture, desc, &pipe));
}
void GraphicsSimple::BindShader(ResourceHandle<IShader>* shader)
{
    commands->push_back(std::make_unique<BindShaderCommand>(shader, &pipe));
}
void GraphicsSimple::BindBuffer(ResourceHandle<IBuffer>* buffer, u32 slot)
{
    commands->push_back(std::make_unique<BindBufferCommand>(buffer, &pipe));
}
void GraphicsSimple::BindTexture(ResourceHandle<ITexture>* texture, u32 slot)
{
    commands->push_back(std::make_unique<BindTextureCommand>(texture, &pipe));
}
void GraphicsSimple::BindSamplerState(ResourceHandle<ISamplerState>* sampler, u32 slot) {
    commands->push_back(std::make_unique<BindSamplerCommand>(sampler, &pipe, slot));
}
void GraphicsSimple::BindViewPort(ResourceHandle<IViewPort>* vp)
{
    commands->push_back(std::make_unique<BindViewPortCommand>(vp, &pipe));
}
void GraphicsSimple::BindRasterizerState(ResourceHandle<IRasterizerState>* rast)
{
    commands->push_back(std::make_unique<BindRasterizerStateCommand>(rast, &pipe));
}
void GraphicsSimple::BindDepthStencilState(ResourceHandle<IDepthStencilState>* depthState, UINT stencilRef)
{
    commands->push_back(std::make_unique<BindDepthStencilStateCommand>(depthState, stencilRef, &pipe));
}
void GraphicsSimple::BindBlendState(ResourceHandle<IBlendState>* blend, const float blendFactor[4], UINT sampleMask)
{
    commands->push_back(std::make_unique<BindBlendStateCommand>(blend, blendFactor, sampleMask, &pipe));
}
void GraphicsSimple::BindRenderTarget(ResourceHandle<IRenderTarget>* target, ResourceHandle<IDepthStencil>* depth)
{
    commands->push_back(std::make_unique<BindRenderTargetCommand>(target, depth, &pipe));
}
void GraphicsSimple::ClearRenderTarget(ResourceHandle<IRenderTarget>* target, const float color[4])
{
    commands->push_back(std::make_unique<ClearRenderTargetCommand>(target, color, &pipe));
}
void GraphicsSimple::ClearDepthStencil(ResourceHandle<IDepthStencil>* target, float depth, UINT8 stencil)
{
    commands->push_back(std::make_unique<ClearDepthStencilCommand>(target, depth, stencil, &pipe));
}
void GraphicsSimple::BindInputLayout(ResourceHandle<IInputLayout>* inputLayout) {
    commands->push_back(std::make_unique<BindInputLayoutCommand>(inputLayout, &pipe));
}

void GraphicsSimple::Release() {
    if (alrRel) return;

    alrRel = true;

    

    for (auto sep : pipes) {
        //auto sep = *sepAd;

        if (window.rootDevice->GetFlags() & GraphicsDeviceFlags::IS_EXPLICIT) {
            auto graphics = (IGraphicsDeviceExplicit*)(window.rootDevice);

            

            if (sep->pl) sep->pl->Release();
            graphics->ReleaseDescriptorPool(&sep->dPool);
            graphics->ReleaseDescriptorSetLayout(&sep->dSetLayout);
            graphics->ReleaseCommandPool(&sep->cp);
            graphics->ReleaseOnPend((void*)sep);
        }
        else { // Immediate contexts don't use pipelines
            delete sep;
            sep = nullptr;
        }
    }

    //if (pipe) {
    //    delete pipe;
    //}
    
}