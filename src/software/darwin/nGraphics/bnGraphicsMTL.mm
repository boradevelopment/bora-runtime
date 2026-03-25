#include "bnGraphicsMTL.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>
#include "MetalUtils.mm"
#include "3rdparty/bspirv/spirv_msl.hpp"

bnGraphicsMTL::bnGraphicsMTL(SysHandle& handle, IGraphicsDeviceConfig& config) : handle(handle), config(config)
{

}

bool bnGraphicsMTL::Init() {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return false;
    [device retain];
    this->device.device = device;
    mNativeCommandQueue = [device newCommandQueue];
    // Use a captured pointer for the block to avoid scope issues
    void* localHandle = handle;

    // Perform UI-sensitive tasks on the Main Thread
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSWindow* window = (__bridge NSWindow*)localHandle;
        NSView* view = [window contentView];

        [view setWantsLayer:YES];

        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

        // This makes the layer resize automatically with the window
        metalLayer.contentsGravity = kCAGravityResizeAspectFill;
        metalLayer.contentsScale = window.backingScaleFactor;
        [view setLayer:metalLayer];

        // Store the layer in your class so you can call nextDrawable() later
        mNativeSwapchain = (void*)metalLayer;
    });

    return true;
}

void bnGraphicsMTL::BeginFrame() {
    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)mNativeCommandQueue;

    // Create the buffer for this frame's work
    id<MTLCommandBuffer> cmdBuffer = [queue commandBuffer];
    mCurrentFrameBuffer.currentFrame = (void*)cmdBuffer;
}

ICommandList *bnGraphicsMTL::GetCommandList() {
    id<MTLCommandBuffer> cmdBuffer = (__bridge id<MTLCommandBuffer>)mCurrentFrameBuffer.GetNativeHandle();
    return new CommandListMTL(cmdBuffer);
}

void bnGraphicsMTL::Resize(long width, long height) {
    if (mNativeSwapchain) {
        CAMetalLayer* swapchain = (__bridge CAMetalLayer*)mNativeSwapchain;
        NSWindow* window = (__bridge NSWindow*)handle;

        // Multiply width/height by the backingScaleFactor! It's apple
        CGFloat scale = window.backingScaleFactor;
        swapchain.drawableSize = CGSizeMake(width * scale, height * scale);
        swapchain.contentsScale = scale;
    }
}

void bnGraphicsMTL::Present() {
    // We'll just use this to clear CPU variables since it's commited
    if (mCurrentDrawable) {
        CFRelease(mCurrentDrawable);
        mCurrentDrawable = nullptr;
    }
    if (mCurrentFrameBuffer) {
        CFRelease(mCurrentFrameBuffer.currentFrame);
        mCurrentFrameBuffer.currentFrame = nullptr;
    }
}

void bnGraphicsMTL::EndFrame() {
    id<MTLCommandBuffer> cmdBuffer = (id<MTLCommandBuffer>)mCurrentFrameBuffer.GetNativeHandle();

    // Get the drawable we fetched earlier in GetSwapchainImage()
    id<CAMetalDrawable> drawable = (id<CAMetalDrawable>)mCurrentDrawable;

    if (drawable && cmdBuffer) {
        [cmdBuffer presentDrawable:drawable];
        [cmdBuffer commit]; // Send to GPU!
    }
}

void bnGraphicsMTL::DestroyPending() {
// todo!
}

ITexture *bnGraphicsMTL::CreateTexture(const TextureDesc &desc, const void *initialData) {
    auto* textureMtl = new TextureMTL(true);
    textureMtl->desc = desc;
    auto idDevice = (id<MTLDevice>)device.GetNativeHandle();

    MTLTextureDescriptor* mtlDesc = [[MTLTextureDescriptor alloc] init];

    // 1. Basic Dimensions
    mtlDesc.textureType = (desc.dimension == TextureDimensions::Dim3) ? MTLTextureType3D : MTLTextureType2D;
    mtlDesc.width = desc.width;
    mtlDesc.height = desc.height;
    mtlDesc.depth = desc.depth;
    mtlDesc.mipmapLevelCount = desc.mipLevels;
    mtlDesc.sampleCount = desc.samples;
    mtlDesc.pixelFormat = ToMetalFormat(desc.format);

    // 2. Map Usage Flags
    // Most textures need to be read in shaders
    mtlDesc.usage = MTLTextureUsageShaderRead;

    if (desc.isRenderTarget || desc.isDepthStencil) {
        mtlDesc.usage |= MTLTextureUsageRenderTarget;
    }

    // 3. Map Storage Mode (CPU Access)
    // On macOS (Discrete GPU), Managed is usually best for CPU writes.
    // On iOS/Apple Silicon, Shared is the default.
    if (desc.CpuAccessWrite || desc.Dynamic) {
#if TARGET_OS_IPHONE
        mtlDesc.storageMode = MTLStorageModeShared;
#else
        mtlDesc.storageMode = MTLStorageModeManaged;
#endif
    } else {
        mtlDesc.storageMode = MTLStorageModePrivate; // GPU Only (Fastest)
    }

    // 4. Create the Texture
    id<MTLTexture> texture = [idDevice newTextureWithDescriptor:mtlDesc];

    // 5. Apply Debug Name
    if (!desc.debugName.empty()) {
        [texture setLabel:[NSString stringWithUTF8String:desc.debugName.c_str()]];
    }

    // 6. Handle Initial Data
    if (initialData && mtlDesc.storageMode != MTLStorageModePrivate) {
        MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
        NSUInteger bytesPerRow = desc.widthBytes > 0 ? desc.widthBytes : (desc.width * 4); // Default to 4bpp if 0
        [texture replaceRegion:region mipmapLevel:0 withBytes:initialData bytesPerRow:bytesPerRow];
    }

    return textureMtl;
}

IBuffer *bnGraphicsMTL::CreateBuffer(const BufferDesc &desc, const void *data) {
    id<MTLDevice> idDevice = (id<MTLDevice>)device.GetNativeHandle();
    BufferMTL* buffer = new BufferMTL();
    buffer->type = desc.type;
    buffer->slot = desc.slot;

    // 1. Determine Resource Options based on your 'dynamic' flag
    MTLResourceOptions options;
    if (desc.dynamic) {
        options = MTLResourceStorageModeShared;
    } else {
        // High performance GPU-only memory
        options = MTLResourceStorageModePrivate;
    }

    id<MTLBuffer> mtlBuffer = nil;

    if (data && !desc.dynamic) {
        // todo: replace this with inner engine code, this is a placeholder!
        mtlBuffer = [idDevice newBufferWithLength:desc.size
                                                          options:MTLResourceStorageModePrivate];

        id<MTLBuffer> stagingBuffer = [idDevice newBufferWithBytes:data
                                                          length:desc.size
                                                         options:MTLResourceStorageModeShared];

        // todo: this can be expensive, worker thread may need to be implemented.
        id<MTLCommandBuffer> commandBuffer = [(id<MTLCommandQueue>)mNativeCommandQueue commandBuffer];

        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

        [blitEncoder copyFromBuffer:stagingBuffer
                       sourceOffset:0
                           toBuffer:mtlBuffer
                  destinationOffset:0
                               size:desc.size];

        [blitEncoder endEncoding];

        // Release the staging buffer ONLY after the GPU is done!!!
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
            [stagingBuffer release];
        }];

        [commandBuffer commit];
    } else if (data) {
        mtlBuffer = [idDevice newBufferWithBytes:data
                                          length:desc.size
                                         options:options];
    } else {
        mtlBuffer = [idDevice newBufferWithLength:desc.size
                                          options:options];
    }

    buffer->mHandle = mtlBuffer;
    return buffer;
}

id<MTLFunction> CompileStencilFromSource(id<MTLDevice> device, const std::string& source, const char* entry) {
    NSString* nsSource = [NSString stringWithUTF8String:source.c_str()];
    id<MTLLibrary> library = [device newLibraryWithSource:nsSource options:nil error:nil];
    if (!library) return nil;

    return [library newFunctionWithName:[NSString stringWithUTF8String:entry]];
}

IShader *bnGraphicsMTL::CreateShader(const ShaderDesc &desc) {
    ShaderMTL* shaderMtl = new ShaderMTL();
    NSError* error = nil;
    if(IsMetalBinary(desc.bytecode, desc.bytecodeSize))
    {
        dispatch_data_t data = dispatch_data_create(desc.bytecode, desc.bytecodeSize,
                                                    dispatch_get_main_queue(),
                                                    DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        id<MTLLibrary> library = [(id<MTLDevice>)device.device newLibraryWithData:data error:&error];
        if (library) {
            shaderMtl->shaderFunction = [library newFunctionWithName:[NSString stringWithUTF8String:desc.entryName]];
        }
        [library release];

        return shaderMtl;
    }

    std::string sourceString;
    if(IsSPIRV(desc.bytecode, desc.bytecodeSize)){
        size_t word_count = desc.bytecodeSize / sizeof(u32);
        const u32* spirv_data = reinterpret_cast<const u32*>(desc.bytecode);
        spirv_cross::CompilerMSL mlsl(spirv_data, word_count);
        spirv_cross::CompilerMSL::Options options;

        options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 3);
        mlsl.set_msl_options(options);
        sourceString = mlsl.compile();
    }
    std::string source(sourceString.empty() ? reinterpret_cast<const char*>(desc.bytecode) : sourceString.c_str(), sourceString.empty() ? desc.bytecodeSize : sourceString.size());
    shaderMtl->shaderFunction = CompileStencilFromSource((id<MTLDevice>)device.device, source, desc.entryName);

    return shaderMtl;
}

ISamplerState *bnGraphicsMTL::CreateSamplerState(const SamplerStateDesc &desc) {
    SamplerMTL* stateMtl = new SamplerMTL();
    MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];

    // 2. Map Filters
    samplerDesc.minFilter = MapFilter(desc.minFilter);
    samplerDesc.magFilter = MapFilter(desc.magFilter);
    samplerDesc.mipFilter = MapMipFilter(desc.mipFilter);

    // 3. Map Address Modes
    samplerDesc.sAddressMode = MapAddressMode(desc.addressU);
    samplerDesc.tAddressMode = MapAddressMode(desc.addressV);
    samplerDesc.rAddressMode = MapAddressMode(desc.addressW);

    // 4. Anisotropy and LOD
    samplerDesc.maxAnisotropy = desc.maxAnisotropy;
    samplerDesc.lodMinClamp = desc.minLOD;
    samplerDesc.lodMaxClamp = desc.maxLOD;

    if (desc.addressU == TextureAddressMode::Border) {
        // Metal uses a specific enum for border colors (Transparent, White, or Black)
        // It does not support arbitrary float[4] colors like OpenGL/DX11
        // todo: Add to bnLog!!!
        samplerDesc.borderColor = MTLSamplerBorderColorTransparentBlack;
    }

    id<MTLSamplerState> samplerState = [(id<MTLDevice>)device.device newSamplerStateWithDescriptor:samplerDesc];

    // Clean up the temporary descriptor
    [samplerDesc release];

    if (!samplerState) {
        return nullptr;
    }

    stateMtl->sampler = samplerState;
    return stateMtl;
}

IInputLayout *bnGraphicsMTL::CreateInputLayout(const InputLayoutDesc &desc) {
    InputLayoutMTL* inputLayoutMtl = new InputLayoutMTL();
    MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];

    for (uint32_t i = 0; i < desc.elements.size(); ++i) {
        const auto& attr = desc.elements[i];

        // attribute index 'i' must match the [[attribute(i)]] index
        vertexDesc.attributes[i].format = MapVertexFormat(attr.type);
        vertexDesc.attributes[i].offset = attr.offset;
        vertexDesc.attributes[i].bufferIndex = attr.inputSlot;
    }

    // Configure Layouts (The stride and step rate for each buffer slot)
    for (const auto& attr : desc.elements) {
        uint32_t slot = attr.inputSlot;

        // Set the stride (total size of one vertex in this buffer)
        vertexDesc.layouts[slot].stride = desc.stride;

        // Handle Instancing
        if (attr.perInstance) {
            vertexDesc.layouts[slot].stepFunction = MTLVertexStepFunctionPerInstance;
            vertexDesc.layouts[slot].stepRate = 1;
        } else {
            vertexDesc.layouts[slot].stepFunction = MTLVertexStepFunctionPerVertex;
            vertexDesc.layouts[slot].stepRate = 1;
        }
    }

    return inputLayoutMtl;
}

IPipelineBuilder *bnGraphicsMTL::CreatePipelineBuilder() {
    return IGraphicsDeviceExplicit::CreatePipelineBuilder();
}

IDescriptorSetLayout *bnGraphicsMTL::CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc) {
    return new DescriptorSetLayoutMTL(desc);
}

ICommandPool *bnGraphicsMTL::CreateCommandPool(CommandPoolDesc desc) {
    // todo
    return new CommandPoolMTL(mNativeCommandQueue, desc);
}

ICommandBuffer *bnGraphicsMTL::BeginSingleTimeCommands(ICommandPool *pool) {
    CommandPoolMTL* mtlPool = static_cast<CommandPoolMTL*>(pool);
    id<MTLCommandBuffer> mtlBuffer = [(id<MTLCommandQueue>)mtlPool->queue commandBuffer];

    return new CommandBufferMTL(mtlBuffer, mtlPool->desc.type);
}

void bnGraphicsMTL::EndSingleTimeCommands(ICommandBuffer *buffer) {
    CommandBufferMTL* mtlBuffer = static_cast<CommandBufferMTL*>(buffer);
    if (mtlBuffer->renderEncoder)  [(id<MTLRenderCommandEncoder>)mtlBuffer->renderEncoder endEncoding];
    if (mtlBuffer->computeEncoder) [(id<MTLComputeCommandEncoder>)mtlBuffer->computeEncoder endEncoding];
    if (mtlBuffer->blitEncoder)    [(id<MTLBlitCommandEncoder>)mtlBuffer->blitEncoder endEncoding];

    mtlBuffer->renderEncoder = nil;
    mtlBuffer->computeEncoder = nil;
    mtlBuffer->blitEncoder = nil;
}

IDescriptorPool *bnGraphicsMTL::CreateDescriptorPool(DescriptorPoolDesc desc) {
    return new DescriptorPoolMTL(desc);
}

IRenderTarget *bnGraphicsMTL::CreateRenderTarget(const RenderTargetDesc &desc) {
    return new RenderTargetMTL(desc);
}

IRasterizerState *bnGraphicsMTL::CreateRasterizerState(const RasterizerDesc &desc) {
    return new RasterizerMTL(desc);
}

IDepthStencilState *bnGraphicsMTL::CreateDepthStencilState(const DepthStencilDesc &desc) {
    return new DepthStencilStateMTL(device.device, desc);
}

IBlendState *bnGraphicsMTL::CreateBlendState(const BlendStateDesc &desc) {
    return new BlendStateMTL(desc);
}

IDepthStencil *bnGraphicsMTL::CreateDepthStencil(ITexture *texture) {
    return new DepthStencilMTL();
}

void CommandListMTL::EnsureEncoder() {
    if (!m_encoder) {
        // Note: In a real engine, you'd get the MTLRenderPassDescriptor
        // from your Framebuffer/Swapchain object here.
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        m_encoder = [(id<MTLCommandBuffer>)m_cmdBuffer renderCommandEncoderWithDescriptor:pass];
    }
}

CommandListMTL::~CommandListMTL() {
    if (m_encoder) {
        [(id<MTLRenderCommandEncoder>)m_encoder endEncoding];
    }
}

void CommandListMTL::BindPipeline(IPipeline *pipeline) {
    EnsureEncoder();
    auto* mtlPipeline = static_cast<PipelineMTL*>(pipeline);
    [(id<MTLRenderCommandEncoder>)m_encoder setRenderPipelineState:(id<MTLRenderPipelineState>) mtlPipeline->pipelineState];
}

void CommandListMTL::BindBuffer(IBuffer *buffer) {
    EnsureEncoder();
    auto* mtlBuffer = static_cast<BufferMTL*>(buffer);
    // Metal requires an index. You might need to extend your interface
    // to pass the shader 'atIndex' argument.
    [(id<MTLRenderCommandEncoder>)m_encoder setVertexBuffer:(id<MTLBuffer>)mtlBuffer->GetNativeHandle() offset:mtlBuffer->slot atIndex:0];
}


void CommandListMTL::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset) {
    EnsureEncoder();
    [(id<MTLRenderCommandEncoder>)m_encoder drawPrimitives:MTLPrimitiveTypeTriangle // Map 'type' here
                  vertexStart:vertexOffset
                  vertexCount:vertexCount];
}


void CommandListMTL::Release() {
    if (m_encoder) {
        [(id<MTLRenderCommandEncoder>)m_encoder endEncoding];
        m_encoder = nil;
    }
}

void DeviceMTL::Release() {
    if (device) {
        [(id<MTLDevice>)device release];
        device = nil;
    }
}

void DeviceContextMTL::Release() {
    if (currentFrame) {
        CFRelease(currentFrame);
    }
}

BufferMTL::~BufferMTL() {
    if (mHandle) {
        [(id)mHandle release];
        mHandle = nil;
    }
}

void ShaderMTL::Release() {
    if (shaderFunction) {
        [(id<MTLFunction>)shaderFunction release];
        shaderFunction = nil;
    }
}

void SamplerMTL::Release() {
    if (sampler) {
        [(id<MTLSamplerState>)sampler release];
        sampler = nil;
    }
}

void InputLayoutMTL::Release() {
    if (inputLayout) {
        [(MTLVertexDescriptor*)inputLayout release];
        inputLayout = nil;
    }
}

void CommandBufferMTL::EndCurrentEncoder() {
    if (renderEncoder) {
        [(id<MTLRenderCommandEncoder>)renderEncoder endEncoding];
        renderEncoder = nil;
    }
    if (computeEncoder) {
        [(id<MTLComputeCommandEncoder>)computeEncoder endEncoding];
        computeEncoder = nil;
    }
    if (blitEncoder) {
        [(id<MTLBlitCommandEncoder>)blitEncoder endEncoding];
        blitEncoder = nil;
    }
}

void DescriptorSetMTL::SetBuffer(uint32_t slot, IBuffer *buffer) {
    auto* mtlBuf = static_cast<BufferMTL*>(buffer);
    bindings[slot] = { slot, (id<MTLResource>)mtlBuf->GetNativeHandle(), nil, DescriptorType::UniformBuffer };
}

void DescriptorSetMTL::SetTexture(uint32_t slot, ITexture *texture) {
    auto* mtlTex = static_cast<TextureMTL*>(texture);
    bindings[slot] = { slot, (id<MTLResource>)mtlTex->GetNativeHandle(), nil, DescriptorType::CombinedImageSampler };
}

void DescriptorSetMTL::SetSampler(uint32_t slot, ISamplerState *sampler) {
    auto* mtlSamp = static_cast<SamplerMTL*>(sampler);
    // Note: Samplers are separate from Textures in Metal's argument slots
    bindings[slot].sampler = mtlSamp->GetNativeHandle();
}

RasterizerMTL::RasterizerMTL(const RasterizerDesc &d) : desc(d) {
    // 1. Map Culling
    if (desc.cullMode == CullMode::None) cullMode = MTLCullModeNone;
    else if (desc.cullMode == CullMode::Back) cullMode = MTLCullModeBack;
    else if (desc.cullMode == CullMode::Front) cullMode = MTLCullModeFront;

    // 2. Map Winding (Front Face)
    winding = desc.frontCounterClockwise ? MTLWindingCounterClockwise : MTLWindingClockwise;

    // 3. Map Fill Mode (Solid vs Wireframe)
    fillMode = (desc.fillMode == IFillMode::Solid) ? MTLTriangleFillModeFill : MTLTriangleFillModeLines;
}

DepthStencilStateMTL::DepthStencilStateMTL(void *device, const DepthStencilDesc &desc) {
    MTLDepthStencilDescriptor *dsDesc = [[MTLDepthStencilDescriptor alloc] init];
    // 1. Depth Settings
    dsDesc.depthCompareFunction = MapCompareFunc(desc.depthFunc);
    dsDesc.depthWriteEnabled = desc.depthWriteMask; // In your engine, likely a bool

    // 2. Stencil Settings
    if (desc.stencilEnable) {
        // Front Face
        MTLStencilDescriptor *front = [[MTLStencilDescriptor alloc] init];
        front.stencilCompareFunction = MapCompareFunc(desc.frontFace.func);
        front.stencilFailureOperation = MapStencilOp(desc.frontFace.failOp);
        front.depthFailureOperation = MapStencilOp(desc.frontFace.depthFailOp);
        front.depthStencilPassOperation = MapStencilOp(desc.frontFace.passOp);
        dsDesc.frontFaceStencil = front;
        [front release];

        // Back Face
        MTLStencilDescriptor *back = [[MTLStencilDescriptor alloc] init];
        back.stencilCompareFunction = MapCompareFunc(desc.backFace.func);
        back.stencilFailureOperation = MapStencilOp(desc.backFace.failOp);
        back.depthFailureOperation = MapStencilOp(desc.backFace.depthFailOp);
        back.depthStencilPassOperation = MapStencilOp(desc.backFace.passOp);
        dsDesc.backFaceStencil = back;
        [back release];
    }

    // 3. Create the immutable state object
    handle = [(id<MTLDevice>)device newDepthStencilStateWithDescriptor:dsDesc];
    [dsDesc release];
}

DepthStencilStateMTL::~DepthStencilStateMTL() {
    if (handle) {
        [(id<MTLDepthStencilState>)handle release];
    }
}

void BlendStateMTL::ApplyToDescriptor(void *pipelineDesc) {
    for (int i = 0; i < 8; ++i) {
        auto& rt = desc.renderTarget[i];
        MTLRenderPipelineColorAttachmentDescriptor* attachment = ((MTLRenderPipelineDescriptor*)pipelineDesc).colorAttachments[i];

        attachment.blendingEnabled = rt.blendEnable;
        attachment.writeMask = rt.renderTargetWriteMask; // e.g., MTLColorWriteMaskAll

        if (rt.blendEnable) {
            attachment.sourceRGBBlendFactor = MapBlendFactor(rt.srcBlend);
            attachment.destinationRGBBlendFactor = MapBlendFactor(rt.destBlend);
            attachment.rgbBlendOperation = MapBlendOp(rt.blendOp);

            attachment.sourceAlphaBlendFactor = MapBlendFactor(rt.srcBlendAlpha);
            attachment.destinationAlphaBlendFactor = MapBlendFactor(rt.destBlendAlpha);
            attachment.alphaBlendOperation = MapBlendOp(rt.blendOpAlpha);
        }
    }
}

void RenderTargetMTL::Release() {
    for (auto attachments : colorAttachments) [(id<MTLTexture>)attachments.texture release];
    if (depthTexture) [(id<MTLTexture>)depthTexture release];
}

RenderTargetMTL::RenderTargetMTL(const RenderTargetDesc &desc) {
    // 1. Wrap Color Targets
    for (size_t i = 0; i < desc.colorTargets.size(); ++i) {
        TextureMTL* tex = static_cast<TextureMTL*>(desc.colorTargets[i]);
        Attachment a;
        a.texture = tex->GetNativeHandle();

        // Map rgba to MTLClearColor
        a.clearColor = desc.clearColors.size() > i ? desc.clearColors[i] : rgba{0,0,0,1};

        // Default to Clear/Store for now, or derive from your engine logic
        a.loadAction = MTLLoadActionClear;
        a.storeAction = MTLStoreActionStore;

        colorAttachments.push_back(a);
    }

    // 2. Wrap Depth/Stencil
    if (desc.depth) {
        // Note: Your IDepthStencil likely wraps a TextureMTL
        depthTexture = (id<MTLTexture>)desc.depth->GetNativeHandle();
        depthClearValue = desc.depthClear;
        stencilClearValue = desc.stencilClear;
    }
}

void PipelineMTL::CreateGraphicsPipeline(const std::vector<ShaderMTL *> &shaders, InputLayoutMTL *inputLayout,
                                         RasterizerMTL *rasterizer, DepthStencilStateMTL *depthStencil,
                                         BlendStateMTL *blendState) {
    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];

    // 1. Shaders
    for (auto* shader : shaders) {
        if (shader->type == ShaderDesc::Type::Vertex)
            pipelineDesc.vertexFunction = (id<MTLFunction>)shader->shaderFunction;
        else if (shader->type == ShaderDesc::Type::Pixel)
            pipelineDesc.fragmentFunction = (id<MTLFunction>)shader->shaderFunction;
    }

    // 2. Vertex Input (Input Layout)
    if (inputLayout) {
        pipelineDesc.vertexDescriptor = (MTLVertexDescriptor*)inputLayout->inputLayout;
    }

    blendState->ApplyToDescriptor(pipelineDesc);
    for (size_t i = 0; i < renderTarget->colorAttachments.size(); ++i) {
        // You'll need a helper to get the MTLPixelFormat from your texture
        pipelineDesc.colorAttachments[i].pixelFormat = [(id<MTLTexture>)renderTarget->colorAttachments[i].texture pixelFormat];
    }

    if (renderTarget->depthTexture) {
        pipelineDesc.depthAttachmentPixelFormat = [(id<MTLTexture>)renderTarget->depthTexture pixelFormat];
        // If format supports stencil (e.g. Depth32Float_Stencil8)
        pipelineDesc.stencilAttachmentPixelFormat = [(id<MTLTexture>)renderTarget->depthTexture pixelFormat];
    }

    // 5. Multisampling
    pipelineDesc.rasterSampleCount = config.enableMSAA ? config.msaaSamples : 1;

    // 6. Alpha to Coverage (from Rasterizer)
    pipelineDesc.alphaToCoverageEnabled = rasterizer->desc.frontCounterClockwise;

    // 7. Bake the PSO
    NSError* error = nil;
    pipelineState = [(id<MTLDevice>)device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];

    if (!pipelineState) {
        std::string err = [[error localizedDescription] UTF8String];
        [pipelineDesc release];
        throw std::runtime_error("Metal Pipeline Creation Failed: " + err);
    }

    [pipelineDesc release];
}
