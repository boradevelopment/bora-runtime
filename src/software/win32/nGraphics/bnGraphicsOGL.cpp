#include "bnGraphicsOGL.h"
#include <iostream>

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    std::cerr << "GL CALLBACK: " << (type == GL_DEBUG_TYPE_ERROR ? "**ERROR**" : "")
        << " type = 0x" << type
        << ", severity = 0x" << severity
        << ", message = " << message << std::endl;
}

bnGraphicsOGL::bnGraphicsOGL(SysHandle& handle, IGraphicsDeviceConfig& config) : windowHandle(handle), config(config) {

}

bool bnGraphicsOGL::Init()
{
    device.SetHandle(windowHandle);
    // Step 1: Get device context
    device.device = GetDC(windowHandle);
    if (!device.device) {
        std::cerr << "Failed to get device context\n";
        return false;
    }

    // Step 2: Set pixel format
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(device.device, &pfd);
    if (!pixelFormat) {
        std::cerr << "Failed to choose pixel format\n";
        return false;
    }

    if (!SetPixelFormat(device.device, pixelFormat, &pfd)) {
        std::cerr << "Failed to set pixel format\n";
        return false;
    }

    // Step 3: Create OpenGL context
    /*context.renderContext*/ HGLRC tempContext = wglCreateContext(device.device);
    if (!tempContext) {
        std::cerr << "Failed to create OpenGL context\n";
        return false;
    }

    if (!wglMakeCurrent(device.device, tempContext)) {
        std::cerr << "Failed to make OpenGL context current\n";
        return false;
    }

    int attribs[] = {
    WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
    WGL_CONTEXT_MINOR_VERSION_ARB, 3,  // 3.2+ = 3.2 or higher
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
    0
    };

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB) {
        return false;
    }


    context.renderContext = wglCreateContextAttribsARB(device.device, 0, attribs);

    // Switch to the new context
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempContext);
    wglMakeCurrent(device.device, context.renderContext);

    // Step 4: Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW init failed: "
            << reinterpret_cast<const char*>(glewGetErrorString(err))
            << std::endl;
        return false;
    }

    std::cout << "GLEW initialized. Version: "
        << GLEW_VERSION_MAJOR << "." << GLEW_VERSION_MINOR << "\n";

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, nullptr);


    return true;
}

void bnGraphicsOGL::BeginFrame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, swpchTarget->fbo);
    // Set clear color (can be stored in your class as a member)
    glClearColor(config.clearColor.r, config.clearColor.g, config.clearColor.b, config.clearColor.a);

    // Clear color and depth buffer (stencil if needed)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    for (auto& currentRepeatedAddresses : currentRepeatMappedAddress) {
        MapBufferMemory(currentRepeatedAddresses.first, currentRepeatedAddresses.second);
    }
    currentRepeatMappedAddress.clear();
}

void bnGraphicsOGL::EndFrame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return;
}

void bnGraphicsOGL::Shutdown()
{
    glUseProgram(0);
    for (auto& [shaderSet, programID] : shaderProgramMap) {
        glDeleteProgram(programID);
    }
    shaderProgramMap.clear();
    if (context.renderContext) {
        context.Release();
    }

    if (device.device) {
        device.Release();
    }
}

void bnGraphicsOGL::Present()
{
    glFinish();

    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(config.vsync ? 1 : 0); // 1 = enable vsync, 0 = disable
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, swpchTarget->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);


    if (device.device) {
        // Swap the back buffer to the front
        SwapBuffers(device.Get());
    }
}

void bnGraphicsOGL::Resize(long width, long height)
{
    this->width = width;
    this->height = height;
    if (msaaSWPCHTarget) {
        msaaSWPCHTarget->Release();
        msaaSWPCHTarget = nullptr;
    }

    if (swpchTarget) {
        swpchTarget->Release();
        swpchTarget = nullptr;
    }

    if (depthTexture) {
        depthTexture->Release();
        depthTexture = nullptr;
    }

    if (depthStencil) {
        depthStencil->Release();
        depthStencil = nullptr;
    }

    TextureDesc depthFormat{};
    depthFormat.width = width;
    depthFormat.height = height;
    depthFormat.samples = config.enableMSAA ? config.msaaSamples : 1;
    depthFormat.format = TextureFormat::D32_Float;
    depthFormat.isDepthStencil = true;
    depthTexture = CreateTexture(depthFormat);
    depthStencil = CreateDepthStencil(depthTexture);
    TextureDesc desc{};
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.format = TextureFormat::RGBA8_UNorm;
    desc.isRenderTarget = true;
    currentSwapChainImage = CreateTexture(desc);

    swpchTarget = (RenderTargetOGL*)CreateRenderTarget({
               .colorTargets = { currentSwapChainImage },
               .depth = depthStencil,
               .makeFramebuffer = false,
               .colorLayout = { ImageLayout::Present }
        });

    //viewports.clear();
    //UpdateViewPorts();
    return; // We don't gotta do nothing, so simple.
}

ITexture* bnGraphicsOGL::GetSwapchainImage()
{
    currentSwapChainImage->owner = false;
    return currentSwapChainImage;
}

ITexture* bnGraphicsOGL::CreateTexture(const TextureDesc& desc, const void* initialData)
{
    GLTextureFormat glFmt = ToGLFormat(desc.format);
    if (glFmt.internalFormat == 0) return nullptr; // unsupported format

    TextureOGL* tex = new TextureOGL();
    tex->desc = desc;
    tex->format = glFmt;
    glGenTextures(1, &tex->textureID);

    GLenum target = GL_TEXTURE_2D;
    glBindTexture(target, tex->textureID);

    if (desc.isDepthStencil || desc.isRenderTarget) {
        // Allocate storage only (no initialData)
        switch (desc.dimension) {
        case TextureDimensions::Dim1:
            glTexStorage1D(target, desc.mipLevels, glFmt.internalFormat, desc.width);
            break;
        case TextureDimensions::Dim2:
            glTexStorage2D(target, desc.mipLevels, glFmt.internalFormat, desc.width, desc.height);
            break;
        case TextureDimensions::Dim3:
            glTexStorage3D(target, desc.mipLevels, glFmt.internalFormat, desc.width, desc.height, desc.depth);
            break;
        }
    }
    else {
        switch (desc.dimension) {
        case TextureDimensions::Dim1:
            glTexImage1D(target, 0, glFmt.internalFormat, desc.width, 0, glFmt.format, glFmt.type, initialData);
            break;
        case TextureDimensions::Dim2:
            glTexImage2D(target, 0, glFmt.internalFormat, desc.width, desc.height, 0, glFmt.format, glFmt.type, initialData);
            break;
        case TextureDimensions::Dim3: 
            glTexImage3D(target, 0, glFmt.internalFormat, desc.width, desc.height, desc.depth, 0, glFmt.format, glFmt.type, initialData);
            break;
        }
    }

    // Set default sampling/wrapping
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, (desc.mipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (desc.mipLevels > 1 && initialData) {
        glGenerateMipmap(target);
    }

    glBindTexture(target, 0);
    return tex;
}

IShader* bnGraphicsOGL::CreateShader(const ShaderDesc& desc)
{
    GLenum glType = ShaderTypeToGL(desc.type);
    if (glType == 0) return nullptr; // unsupported shader type

    ShaderOGL* shader = new ShaderOGL();
    shader->type = desc.type;

    // Create shader object
    shader->shaderID = glCreateShader(glType);

    bool spirv = IsSPIRV(desc.bytecode, desc.bytecodeSize);

    if (spirv && IsGLExtensionSupported("GL_ARB_gl_spirv")) {
        glShaderBinary(1, &shader->shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB,
            desc.bytecode, static_cast<GLsizei>(desc.bytecodeSize));
        glSpecializeShader(shader->shaderID, desc.entryName, 0, nullptr, nullptr);
    }
    else {
        const char* src = reinterpret_cast<const char*>(desc.bytecode);
        GLint length = static_cast<GLint>(desc.bytecodeSize);
        glShaderSource(shader->shaderID, 1, &src, &length);
        glCompileShader(shader->shaderID);

        GLint status = 0;
        glGetShaderiv(shader->shaderID, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint logLength = 0;
            glGetShaderiv(shader->shaderID, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0) {
                std::vector<char> log(logLength);
                glGetShaderInfoLog(shader->shaderID, logLength, nullptr, log.data());
                std::cerr << "Shader compilation failed (" << desc.debugName << "):\n" << log.data() << std::endl;
            }
            shader->Release();
            delete shader;
            return nullptr;
        }
    }

   // cShaders.push_back(shader);

    return shader;
}

IBuffer* bnGraphicsOGL::CreateBuffer(const BufferDesc& desc, const void* data)
{
    GLenum target = ToGLBufferType(desc.type);
    if (target == 0) return nullptr;

    BufferOGL* buffer = new BufferOGL();
    buffer->type = desc.type;
    buffer->slot = desc.slot;
    buffer->dynamic = desc.dynamic;
    buffer->stride = desc.stride;
    glGenBuffers(1, &buffer->bufferID);
    glBindBuffer(target, buffer->bufferID);

    GLenum usage = desc.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

    if (data) {
        if (config.clipSpace == VerticesClipSpace::VULKAN) {
            if (desc.type == BufferType::Vertex) {
                buffer->dataBuffer.resize(desc.size);
                memcpy_s(buffer->dataBuffer.data(), buffer->dataBuffer.size(), data, buffer->dataBuffer.size());

                size_t vertexCount = desc.size / desc.stride;
                for (size_t i = 0; i < vertexCount; i++) {
                    auto* base = buffer->dataBuffer.data() + i * desc.stride;

                    float* x = reinterpret_cast<float*>(base + 0);
                    float* y = reinterpret_cast<float*>(base + sizeof(float));
                    *y = -*y; // flip Y

                    //float* u = reinterpret_cast<float*>(base + 12);
                    //float* v = reinterpret_cast<float*>(base + 16);
                    //*v = 1.0f - *v; // flip V (texture coordinate Y)
                }
            }
        }
    }

    glBufferData(target, desc.size, buffer->dataBuffer.empty() ? data : buffer->dataBuffer.data(), usage);

    glBindBuffer(target, 0);

    return buffer;
}

IInputLayout* bnGraphicsOGL::CreateInputLayout(const InputLayoutDesc& desc)
{
    InputLayoutOGL* layout = new InputLayoutOGL();
    layout->desc = desc;
    glGenVertexArrays(1, &layout->vao);
    glBindVertexArray(layout->vao);

    //if (!desc.vs || !desc.vs->Get()) {
    //    glBindVertexArray(0);
    //    return layout; // cannot bind attributes without shader
    //}



    glBindVertexArray(0);

    cLayoutDraw = layout;
    return layout;
}

ISamplerState* bnGraphicsOGL::CreateSamplerState(const SamplerStateDesc& desc)
{
    SamplerOGL* sampler = new SamplerOGL();
    sampler->slot = desc.slot;
    glGenSamplers(1, &sampler->samplerID);

    // Filters
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_MIN_FILTER, ToGLFilter(desc.minFilter));
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_MAG_FILTER, ToGLFilter(desc.magFilter));
    glSamplerParameterf(sampler->samplerID, GL_TEXTURE_LOD_BIAS, desc.mipLODBias);

    // Anisotropy (if requested)
    if (desc.minFilter == TextureFilter::Anisotropic || desc.magFilter == TextureFilter::Anisotropic) {
        GLfloat maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        glSamplerParameterf(sampler->samplerID, GL_TEXTURE_MAX_ANISOTROPY,
            std::min<float>(desc.maxAnisotropy, maxAniso));
    }

    // Address/wrap modes
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_WRAP_S, ToGLAddressMode(desc.addressU));
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_WRAP_T, ToGLAddressMode(desc.addressV));
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_WRAP_R, ToGLAddressMode(desc.addressW));

    // Comparison function
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_COMPARE_MODE,
        desc.comparisonFunc != ComparisonFunc::Never ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
    glSamplerParameteri(sampler->samplerID, GL_TEXTURE_COMPARE_FUNC, ToGLCompareFunc(desc.comparisonFunc));

    // Border color
    glSamplerParameterfv(sampler->samplerID, GL_TEXTURE_BORDER_COLOR, desc.borderColor);

    // LOD clamp
    glSamplerParameterf(sampler->samplerID, GL_TEXTURE_MIN_LOD, desc.minLOD);
    glSamplerParameterf(sampler->samplerID, GL_TEXTURE_MAX_LOD, desc.maxLOD);

    return sampler;
}

IViewPort* bnGraphicsOGL::CreateViewPort(const ViewPortDesc& desc)
{
    return new ViewPortOGL(desc);
}

IRasterizerState* bnGraphicsOGL::CreateRasterizerState(const RasterizerDesc& desc)
{
    return new RasterizerOGL(desc);
}

IDepthStencilState* bnGraphicsOGL::CreateDepthStencilState(const DepthStencilDesc& desc)
{
    return new DepthStencilStateOGL(desc);
}

IBlendState* bnGraphicsOGL::CreateBlendState(const BlendStateDesc& desc)
{
    return new BlendStateOGL(desc);
}

IRenderTarget* bnGraphicsOGL::CreateRenderTarget(const RenderTargetDesc& desc)
{
    RenderTargetOGL* rt = new RenderTargetOGL(desc);

    if ((desc.colorTargets.empty() && !desc.depth))
        return rt; // nothing to bind

    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Attach color targets
    rt->colorAttachments.resize(desc.colorTargets.size());
    for (size_t i = 0; i < desc.colorTargets.size(); ++i) {
        ITexture* tex = desc.colorTargets[i];
        GLuint texID = static_cast<TextureOGL*>(tex)->Get();
        rt->colorAttachments[i] = texID;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
            GL_TEXTURE_2D, texID, 0);
    }

    // Attach depth target
    if (desc.depth) {
        auto textureDep = (TextureOGL*)desc.depth;
        if (textureDep) {
            GLuint depthID = textureDep->Get();
            rt->depthAttachment = depthID;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                GL_TEXTURE_2D, depthID, 0);
        }
    }

    // Set draw buffers
    std::vector<GLenum> drawBuffers(desc.colorTargets.size());
    for (size_t i = 0; i < desc.colorTargets.size(); ++i)
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);

    if (!drawBuffers.empty())
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    else
        glDrawBuffer(GL_NONE);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return rt;
}

IDepthStencil* bnGraphicsOGL::CreateDepthStencil(ITexture* texture)
{
    if (!texture) return nullptr;
    return new DepthStencilOGL(texture);
}

void bnGraphicsOGL::ReleaseShader(IShader** shader  )
{
    if (!shader || !*shader) return;

    ShaderOGL* oglShader = static_cast<ShaderOGL*>(*shader);
    oglShader->Release();

    *shader = nullptr;
}

void bnGraphicsOGL::ReleaseBuffer(IBuffer** buffer)
{
    if (!buffer || !*buffer) return;

    BufferOGL* oglBuffer = static_cast<BufferOGL*>(*buffer);
    oglBuffer->Release();

    *buffer = nullptr;
}
 
void bnGraphicsOGL::ReleaseTexture(ITexture** texture)
{
    if (!texture || !*texture) return;

    TextureOGL* oglTex = static_cast<TextureOGL*>(*texture);
    if (oglTex != currentSwapChainImage) { // hack but swapchain should never in any circumstances be destroyed unless being shutdown.
        oglTex->Release();
    }

    *texture = nullptr;
}

void bnGraphicsOGL::BindShader(IShader* shader)
{
    if (!shader) {
        glUseProgram(0); // unbind any shader
        return;
    }


    ShaderOGL* oglShader = dynamic_cast<ShaderOGL*>(shader);
    if (!oglShader) return;


    // If it�s a compute shader, or a vertex/pixel shader, the shader program must be created
    // Assume ShaderOGL holds the linked program ID
    cShaders.push_back(oglShader);
}

void bnGraphicsOGL::BindBuffer(IBuffer* buffer)
{
    if (!buffer) return;

    BufferOGL* oglBuffer = static_cast<BufferOGL*>(buffer);

    auto iteration = std::find(mappedPointers.begin(), mappedPointers.end(), oglBuffer);
    bool isARepeatedPointer = false;
    if (iteration != mappedPointers.end()) {
        isARepeatedPointer = true;
        UnmapBufferMemory(oglBuffer);
    }

    switch (oglBuffer->type)
    {
    case BufferType::Vertex: {
        if (!cLayoutDraw) {
            std::cout << "no continue without il";
            break;
        }
        glBindVertexArray(cLayoutDraw->vao);
        glBindBuffer(GL_ARRAY_BUFFER, oglBuffer->Get());

        for (const VertexAttribDesc& attr : cLayoutDraw->desc.elements)
        {
            GLAttribFormat fmt = ToGLAttrib(attr.type);
            if (fmt.size == 0) continue;

            GLuint location = attr.semanticIndex; // or another mapping you define
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(
                location,
                fmt.size,
                fmt.type,
                fmt.normalized,
                static_cast<GLsizei>(cLayoutDraw->desc.stride),
                reinterpret_cast<void*>(attr.offset)
            );
            glVertexAttribDivisor(location, attr.perInstance ? 1 : 0);
        }
        
        ShaderSet sSet;
        for (auto shader : cShaders) {
            sSet.insert(shader->shaderID);
        }

        auto it = shaderProgramMap.find(sSet);
        GLuint programID;
        if (it != shaderProgramMap.end()) {
            programID = it->second;
        }
        else {
            programID = glCreateProgram();
            for (auto shader : sSet)
                glAttachShader(programID, shader);
            glLinkProgram(programID);

            GLint linkStatus = 0;
            glGetProgramiv(programID, GL_LINK_STATUS, &linkStatus);
            if (linkStatus == GL_FALSE) {
                GLint logLength = 0;
                glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
                if (logLength > 0) {
                    std::vector<char> log(logLength);
                    glGetProgramInfoLog(programID, logLength, nullptr, log.data());
                    std::cerr << "Program link failed \n" << log.data() << std::endl;
                }

            }

            shaderProgramMap[sSet] = programID;
            //cShPrograms[oglBuffer->bufferID] = programID;

            for (auto shader : cShaders) { glDetachShader(programID, shader->shaderID); } 
            
        }

        cShaders.clear();
        glUseProgram(programID);

        break;
    }
    case BufferType::Index:
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oglBuffer->Get());
        break;
    case BufferType::Constant:
        glBindBufferBase(GL_UNIFORM_BUFFER, oglBuffer->slot, oglBuffer->Get());
        break;
    case BufferType::Storage:
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, oglBuffer->slot, oglBuffer->Get());
        break;
    case BufferType::Staging:
        // Staging buffers are usually CPU read/write, might not bind
        break;
    }

    if (isARepeatedPointer) {
        auto it = repeatMappedAddress.find(oglBuffer);
        if (it != repeatMappedAddress.end()) {
            //MapBufferMemory(oglBuffer, it->second); // map it right back
            currentRepeatMappedAddress[oglBuffer] = it->second;
        }
    }
}

void bnGraphicsOGL::BindTexture(ITexture* texture, u8 slot)
{
    if (!texture) return;

    TextureOGL* oglTex = dynamic_cast<TextureOGL*>(texture);
    if (!oglTex) return;

        GLenum target = GL_TEXTURE_2D; // adjust for 3D/array/etc
        glActiveTexture(GL_TEXTURE0 + (slot != 0 ? slot : oglTex->slot));
        glBindTexture(target, oglTex->textureID);
}

void bnGraphicsOGL::BindInputLayout(IInputLayout* layout)
{
    if (!layout) return;

    InputLayoutOGL* oglLayout = dynamic_cast<InputLayoutOGL*>(layout);
    if (!oglLayout) return;

    glBindVertexArray(oglLayout->vao);
}

void bnGraphicsOGL::BindSamplerState(ISamplerState* samplerState, u8 slot)
{
    if (!samplerState) return;

    SamplerOGL* oglSampler = dynamic_cast<SamplerOGL*>(samplerState);
    glBindSampler(slot != 0 ? slot : oglSampler->slot, oglSampler->samplerID);
}

void bnGraphicsOGL::BindViewPort(IViewPort* viewPort)
{
    if (!viewPort) return;

    // Cast to your OpenGL-specific implementation
    ViewPortOGL* oglVP = static_cast<ViewPortOGL*>(viewPort);
    const ViewPort& vp = oglVP->Get();

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    GLint invertedY = height - (vp.y + vp.height);

    // Set viewport in OpenGL
    glViewport(
        static_cast<GLint>(vp.x),
        invertedY,
        static_cast<GLsizei>(vp.width),
        static_cast<GLsizei>(vp.height)
    );

    // Set depth range (convert long to double)
    glDepthRange(
        static_cast<GLclampd>(vp.minDepth),
        static_cast<GLclampd>(vp.maxDepth)
    );
}

void bnGraphicsOGL::BindRasterizerState(IRasterizerState* state)
{
    if (!state) return;

    RasterizerOGL* oglState = static_cast<RasterizerOGL*>(state);
    const RasterizerDesc& desc = oglState->GetDesc();

    // Fill mode
    GLenum mode = (desc.fillMode == IFillMode::Solid) ? GL_FILL : GL_LINE;
    glPolygonMode(GL_FRONT_AND_BACK, mode);

    // Culling
    if (desc.cullMode == CullMode::None) {
        glDisable(GL_CULL_FACE);
    }
    else {
        glEnable(GL_CULL_FACE);
        glCullFace((desc.cullMode == CullMode::Back) ? GL_BACK : GL_FRONT);
    }

    // Front face winding
    glFrontFace(desc.frontCounterClockwise ? GL_CCW : GL_CW);

    // Depth bias
    if (desc.depthBias != 0 || desc.slopeScaledDepthBias != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(desc.slopeScaledDepthBias, static_cast<GLfloat>(desc.depthBias));
    }
    else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Depth clip (always on in modern OpenGL; can ignore)
    // glDepthClamp can be used if supported
    if (desc.depthClipEnable) {
        glDisable(GL_DEPTH_CLAMP);
    }
    else {
        glEnable(GL_DEPTH_CLAMP);
    }

    // Scissor
    if (desc.scissorEnable) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);

    // Multisample / AA lines
    if (desc.multisampleEnable) glEnable(GL_MULTISAMPLE);
    else glDisable(GL_MULTISAMPLE);

    if (desc.antialiasedLineEnable) glEnable(GL_LINE_SMOOTH);
    else glDisable(GL_LINE_SMOOTH);
}

void bnGraphicsOGL::BindDepthStencilState(IDepthStencilState* state, UINT stencilRef)
{
    if (!state) return;

    DepthStencilStateOGL* oglState = static_cast<DepthStencilStateOGL*>(state);
    const DepthStencilDesc& desc = oglState->GetDesc();

    // --- Depth ---
    if (desc.depthEnable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    glDepthMask(desc.depthWriteMask ? GL_TRUE : GL_FALSE);
    glDepthFunc(ToGLCompareFunc(desc.depthFunc));

    // --- Stencil ---
    if (desc.stencilEnable) glEnable(GL_STENCIL_TEST);
    else glDisable(GL_STENCIL_TEST);

    // Stencil masks
    glStencilMaskSeparate(GL_FRONT, desc.stencilWriteMask);
    glStencilMaskSeparate(GL_BACK, desc.stencilWriteMask);

    // Front-face stencil
    glStencilOpSeparate(GL_FRONT,
        ToGLStencilOp(desc.frontFace.failOp),
        ToGLStencilOp(desc.frontFace.depthFailOp),
        ToGLStencilOp(desc.frontFace.passOp));
    glStencilFuncSeparate(GL_FRONT,
        ToGLCompareFunc(desc.frontFace.func),
        stencilRef,
        desc.stencilReadMask);

    // Back-face stencil
    glStencilOpSeparate(GL_BACK,
        ToGLStencilOp(desc.backFace.failOp),
        ToGLStencilOp(desc.backFace.depthFailOp),
        ToGLStencilOp(desc.backFace.passOp));
    glStencilFuncSeparate(GL_BACK,
        ToGLCompareFunc(desc.backFace.func),
        stencilRef,
        desc.stencilReadMask);
}

void bnGraphicsOGL::BindBlendState(IBlendState* state, const float blendFactor[4], UINT sampleMask)
{
    if (!state) return;

    BlendStateOGL* oglState = static_cast<BlendStateOGL*>(state);
    if (currentBlendState == oglState) return;
    currentBlendState = oglState;
    const BlendStateDesc& desc = oglState->GetDesc();

    if (blendFactor)
    {
        glBlendColor(blendFactor[0], blendFactor[1], blendFactor[2], blendFactor[3]);
    }
    else
    {
        glBlendColor(0.0f, 0.0f, 0.0f, 0.0f); // default if nullptr
    }

    // Set sample mask (per sample index, only first 1 used if single sample)
    // OpenGL supports 32-bit masks per index
    glSampleMaski(0, sampleMask);

    // Alpha-to-coverage
    if (desc.alphaToCoverageEnable)
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    // Bind per-render-target blending
    for (int i = 0; i < 8; ++i)
    {
        const auto& rt = desc.renderTarget[i];

        if (rt.blendEnable)
            glEnablei(GL_BLEND, i);
        else
            glDisablei(GL_BLEND, i);

        glBlendFuncSeparatei(i,
            ToGLBlend(rt.srcBlend),
            ToGLBlend(rt.destBlend),
            ToGLBlend(rt.srcBlendAlpha),
            ToGLBlend(rt.destBlendAlpha));

        glBlendEquationSeparatei(i,
            ToGLBlendOp(rt.blendOp),
            ToGLBlendOp(rt.blendOpAlpha));

        // Color write mask
        GLboolean r = (rt.renderTargetWriteMask & 1) != 0;
        GLboolean g = (rt.renderTargetWriteMask & 2) != 0;
        GLboolean b = (rt.renderTargetWriteMask & 4) != 0;
        GLboolean a = (rt.renderTargetWriteMask & 8) != 0;
        glColorMaski(i, r, g, b, a);
    }
}

void bnGraphicsOGL::BindRenderTarget(IRenderTarget* rt, IDepthStencil* ds)
{
    GLuint fbo = 0;

    if (rt) {
        RenderTargetOGL* oglRT = static_cast<RenderTargetOGL*>(rt);
        fbo = oglRT->GetFBO();
    }
    else {
        // If no render target is provided, bind the default framebuffer (screen)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // If a separate depth/stencil is provided, attach it
    if (ds) {
        DepthStencilOGL* oglDS = static_cast<DepthStencilOGL*>(ds);
        GLuint depthID = static_cast<TextureOGL*>(oglDS->GetTexture())->Get();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, depthID, 0);
    }

    // Set draw buffers (for MRT)
    RenderTargetOGL* oglRT = static_cast<RenderTargetOGL*>(rt);
    size_t numColor = oglRT->colorAttachments.size();
    if (numColor > 0) {
        std::vector<GLenum> drawBuffers(numColor);
        for (size_t i = 0; i < numColor; ++i)
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);

        glDrawBuffers(static_cast<GLsizei>(numColor), drawBuffers.data());
    }
    else {
        glDrawBuffer(GL_NONE);
    }

    // Optionally: check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer incomplete when binding render target!" << std::endl;
}

void bnGraphicsOGL::ClearRenderTarget(IRenderTarget* target, const float color[4])
{
    if (!target) return;

    RenderTargetOGL* oglRT = static_cast<RenderTargetOGL*>(target);
    GLuint fbo = oglRT->GetFBO();

    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Clear each color attachment
    for (size_t i = 0; i < oglRT->colorAttachments.size(); ++i) {
        glDrawBuffer(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
        glClearColor(color[0], color[1], color[2], color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Restore draw buffers (so MRT works correctly after clear)
    if (!oglRT->colorAttachments.empty()) {
        std::vector<GLenum> drawBuffers(oglRT->colorAttachments.size());
        for (size_t i = 0; i < oglRT->colorAttachments.size(); ++i)
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }

    // Unbind framebuffer if needed
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void bnGraphicsOGL::ClearDepthStencil(IDepthStencil* target, float depth, UINT8 stencil)
{
    if (!target) return;

    DepthStencilOGL* oglDS = static_cast<DepthStencilOGL*>(target);
    GLuint fbo = 0;

    // If DepthStencil is attached to a RenderTarget, we need to find the FBO
    // For simplicity, assume user binds the FBO first
    // Or you can store a pointer to the FBO in DepthStencilOGL if needed

    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // bind FBO manually if you store it

    // Set clear values
    glClearDepth(depth);
    glClearStencil(stencil);

    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Unbind framebuffer if needed
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void bnGraphicsOGL::DispatchCompute(UINT x, UINT y, UINT z)
{
    // Assumes a compute shader is already bound
    glDispatchCompute(x, y, z);

    // Optional: ensure memory writes are visible for subsequent commands
    // For example, if writing to SSBOs or images
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
        GL_SHADER_STORAGE_BARRIER_BIT |
        GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
        GL_ELEMENT_ARRAY_BARRIER_BIT |
        GL_UNIFORM_BARRIER_BIT);
}

void bnGraphicsOGL::CopyToBuffer(IBuffer* buffer, void* data, size_t size)
{
    if (!buffer || !data || size == 0) return;

    BufferOGL* oglBuffer = static_cast<BufferOGL*>(buffer);

    GLenum target = GL_ARRAY_BUFFER;
    switch (oglBuffer->type) {
    case BufferType::Vertex:   target = GL_ARRAY_BUFFER; break;
    case BufferType::Index:    target = GL_ELEMENT_ARRAY_BUFFER; break;
    case BufferType::Constant: target = GL_UNIFORM_BUFFER; break;
    case BufferType::Storage:  target = GL_SHADER_STORAGE_BUFFER; break;
    case BufferType::Staging:  target = GL_COPY_WRITE_BUFFER; break; // optional
    }

    glBindBuffer(target, oglBuffer->Get());

    if (oglBuffer->dynamic) {
        // For dynamic buffers, map and copy
        void* ptr = glMapBufferRange(target, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            memcpy(ptr, data, size);
            glUnmapBuffer(target);
        }
    }
    else {
        // For static buffers, update via glBufferSubData
        glBufferSubData(target, 0, size, data);
    }

    glBindBuffer(target, 0);
}

void bnGraphicsOGL::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset)
{
    // Create program and link
  
    auto cType = ToGLPrimitiveType(type);
    glDrawArrays(cType, static_cast<GLint>(vertexOffset), static_cast<GLsizei>(vertexCount));

    

   
}

void bnGraphicsOGL::DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset)
{
    if (!indexBuffer) return;
    BufferOGL* idx = static_cast<BufferOGL*>(indexBuffer);

    // Determine index type
    GLenum indexType = GL_UNSIGNED_INT; // default
    switch (idx->stride) {
    case 1: indexType = GL_UNSIGNED_BYTE; break;
    case 2: indexType = GL_UNSIGNED_SHORT; break;
    case 4: indexType = GL_UNSIGNED_INT; break;
    default:
        std::cerr << "Unsupported index stride!" << std::endl;
        return;
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx->Get());
    glDrawElements(ToGLPrimitiveType(type), static_cast<GLsizei>(indexCount), indexType,
        reinterpret_cast<void*>(indexOffset * idx->stride));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void bnGraphicsOGL::MapBufferMemory(IBuffer* buffer, void** dataPtr)
{
    if (!buffer || !dataPtr) return;

    BufferOGL* bufOGL = static_cast<BufferOGL*>(buffer);

    GLenum access = bufOGL->dynamic ? GL_WRITE_ONLY : GL_READ_WRITE;

    glBindBuffer(GL_ARRAY_BUFFER, bufOGL->Get());
    *dataPtr = glMapBuffer(GL_ARRAY_BUFFER, access);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (dataPtr != nullptr) {
        mappedPointers.push_back(bufOGL);
        repeatMappedAddress[bufOGL] = dataPtr;
    }
}

void bnGraphicsOGL::UnmapBufferMemory(IBuffer* buffer)
{
    if (!buffer) return;
    BufferOGL* bufOGL = static_cast<BufferOGL*>(buffer);
    glBindBuffer(GL_ARRAY_BUFFER, bufOGL->Get());
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mappedPointers.erase(
        std::remove_if(
            mappedPointers.begin(),
            mappedPointers.end(),
            [=](BufferOGL* b) { return b == (BufferOGL*)buffer; } // condition for removal
        ),
        mappedPointers.end()
    );
}

void bnGraphicsOGL::CopyBufferToImage(IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc)
{
    BufferOGL* bufferOGL = static_cast<BufferOGL*>(srcBuffer);
    TextureOGL* texOGL = static_cast<TextureOGL*>(dstTexture);

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, texOGL->textureID);

    // Bind staging buffer as pixel unpack buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bufferOGL->Get()); // GLuint buffer ID

    if (desc.bufferRowLength > desc.imageExtent.width) {
        desc.bufferRowLength = desc.imageExtent.width;
    }

    if (desc.bufferImageHeight > desc.imageExtent.height) {
        desc.bufferImageHeight = desc.imageExtent.height;
    }

    // Set row length and image height
    glPixelStorei(GL_UNPACK_ROW_LENGTH, desc.bufferRowLength ? desc.bufferRowLength : desc.imageExtent.width);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, desc.bufferImageHeight ? desc.bufferImageHeight : desc.imageExtent.height);

    auto texF = texOGL->format;

    // Copy from buffer (PBO) to texture
    glTexSubImage2D(
        GL_TEXTURE_2D,
        desc.imageSubresource.mipLevel,
        desc.imageOffset.x,
        desc.imageOffset.y,
        desc.imageExtent.width,
        desc.imageExtent.height,
        texF.format,   // e.g., GL_RGBA
        texF.type,     // e.g., GL_UNSIGNED_BYTE
        reinterpret_cast<void*>(desc.bufferOffset) // offset into PBO
    );

    // Reset pixel store
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);

    // Unbind buffer & texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void bnGraphicsOGL::CopyImageToImage(ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc)
{
    TextureOGL* texsrcOGL = static_cast<TextureOGL*>(srcBuffer);
    TextureOGL* texOGL = static_cast<TextureOGL*>(dstBuffer);

    GLuint srcTex = texsrcOGL->textureID;
    GLuint dstTex = texOGL->textureID;

    // Extract offsets from the struct
    GLint srcLevel = desc.srcSubresource.mipLevel;
    GLint dstLevel = desc.dstSubresource.mipLevel;

    GLint srcX = desc.srcOffset.x;
    GLint srcY = desc.srcOffset.y;
    GLint srcZ = desc.srcOffset.z;

    GLint dstX = desc.dstOffset.x;
    GLint dstY = desc.dstOffset.y;
    GLint dstZ = desc.dstOffset.z;

    GLsizei width = desc.extent.width;
    GLsizei height = desc.extent.height;
    GLsizei depth = desc.extent.depth;

    glCopyImageSubData(
        srcTex, GL_TEXTURE_2D, srcLevel,
        srcX, srcY, srcZ,
        dstTex, GL_TEXTURE_2D, dstLevel,
        dstX, dstY, dstZ,
        width, height, depth
    );

}

void bnGraphicsOGL::PushGroup(const char *name, uint32_t color) {
    // 0x9127 is GL_DEBUG_SOURCE_APPLICATION
    // -1 tells OpenGL the string is null-terminated
    glPushDebugGroup(0x9127, 0, -1, name);
}

void bnGraphicsOGL::PopGroup() {
    glPopDebugGroup();
}

void bnGraphicsOGL::SetMarker(const char *name, uint32_t color) {
    // GL_DEBUG_TYPE_MARKER (0x8268) is the correct type for single events
    // Severity is usually GL_DEBUG_SEVERITY_NOTIFICATION (0x826B)
    glDebugMessageInsert(0x9127, 0x8268, 0, 0x826B, -1, name);
}