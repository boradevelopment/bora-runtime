#include "bnMessageBox.h"
#include "nGraphics/bnGraphicsCreator.h"
#include "nGraphics/bnGraphics.h"
#include "nAudio/bnAudioCreator.h"
#include "Utils.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#include "bskia/include/core/SkGraphics.h"
#include "nCommon/MemoryCommands.h"

bool LoadMP3ToPCM(const std::string& path, std::vector<float>& outPCM, PCMFormat& format)
{
    drmp3 mp3;
    if (!drmp3_init_file(&mp3, path.c_str(), nullptr)) {
        return false;
    }

    format.sampleRate = mp3.sampleRate;
    format.channels = mp3.channels;
    format.bitsPerSample = 32;
    format.type = SampleType::FLOAT32;

    size_t totalFrames = drmp3_get_pcm_frame_count(&mp3);
    outPCM.resize(totalFrames * mp3.channels);

    drmp3_read_pcm_frames_f32(&mp3, totalFrames, outPCM.data());

    drmp3_uninit(&mp3);
    return true;
}

void ResampleFloat(
    const float* src,       // input float buffer
    size_t numFrames,       // input frames
    int srcRate,            // source sample rate
    int dstRate,            // target device rate
    int channels,           // number of channels
    std::vector<float>& output) // output buffer
{
    size_t newFrames = numFrames * dstRate / srcRate;
    output.resize(newFrames * channels);

    double ratio = static_cast<double>(srcRate) / dstRate;

    for (size_t i = 0; i < newFrames; ++i) {
        double idx = i * ratio;
        size_t idxInt = static_cast<size_t>(idx);
        double frac = idx - idxInt;

        for (int c = 0; c < channels; ++c) {
            float s0 = src[(idxInt * channels) + c];
            float s1 = (idxInt + 1 < numFrames) ? src[((idxInt + 1) * channels) + c] : s0;
            output[i * channels + c] = static_cast<float>(s0 + frac * (s1 - s0));
        }
    }
}

ShaderData vD;
ShaderData pD;
inline bool bnMessageBox::InitGraphicsSystem() {
    HRESULT hr;
    int fallback = 0;

    audioConfig = IAudioDeviceConfig();
    auto aResult = CreateAudioDevice(handle, &audioConfig);

    if (aResult.initialized) {
        rootAudioDevice = aResult.device;
    }

    if (choice == NONE) choice = GetBestGraphicsChoice();
    gEConfig = IGraphicsDeviceConfig{};
    gEConfig.vsync = true;
    gEConfig.enableMSAA = false;


    auto result = CreateGraphicsDevice(choice, handle, &gEConfig);
    if (!result.initialized) {
        while (choice != NONE && !result.initialized) {
            fallback++;
            choice = GetBestGraphicsChoice(fallback);
            // setup config...
            result = CreateGraphicsDevice(choice, handle, &gEConfig);
        }

        if (choice == NONE) {
            MessageBox(handle, L"Error! Bora does not support your GPU, please consider updating or upgrading your GPU.", L"BORA is not supported for your GPU!", MB_ICONERROR);
            close();
            return false;
        }
    }

    rootDevice = result.device;
    if (IsGraphicsChoiceExplicit(choice)) {
        usingExpl = true;
        gEngineExpl = (IGraphicsDeviceExplicit*)rootDevice;
    }
    else {
        usingExpl = false;
        gEngineImm = (IGraphicsDeviceImmediate*)rootDevice;
        gEngineImm = (IGraphicsDeviceImmediate*)rootDevice;
    }


    if (graphics) {
        delete graphics;
        graphics = nullptr;
    }
    graphics = new bnGraphics(this);

    auto advanced = graphics->makeAdvanced();
  
    if (usingExpl) {
        advanced->CreateDescriptorPool({
                {{ DescriptorType::CombinedImageSampler, 2}},
                2, DescriptorPoolFlags::FreeDescriptor
                }, &dPool);
        advanced->CreateDescriptorSetLayout({
                    {{0,DescriptorType::CombinedImageSampler, 1, IShaderStage::Fragment }}
                }, &dSetLayout);
        advanced->CreateCommandPool({
                 0, true, true
            }, &copyTexturePool);
    }

    graphics->CreateSamplerState({}, &samplerState);

    BlendStateDesc blendDesc;
    blendDesc.renderTarget[0].blendEnable = true;
    blendDesc.renderTarget[0].srcBlend = Blend::SrcAlpha;
    blendDesc.renderTarget[0].destBlend = Blend::InvSrcAlpha;
    blendDesc.renderTarget[0].blendOp = BlendOp::Add;
    blendDesc.renderTarget[0].renderTargetWriteMask = 0x0F;

    //blending.Resolve(rootDevice->CreateBlendState(blendDesc));
    graphics->CreateBlendState(blendDesc, &blending);

    // Rasterizer
    RasterizerDesc rastDesc{};
    rastDesc.fillMode = IFillMode::Solid;        // Solid fill (Wireframe if debugging)
    rastDesc.cullMode = CullMode::Back;         // Cull back faces
    rastDesc.frontCounterClockwise = false;                  // CCW = front by default
    rastDesc.depthBias = 0;
    rastDesc.depthBiasClamp = 0.0f;
    rastDesc.slopeScaledDepthBias = 0.0f;
    rastDesc.depthClipEnable = true;                   // Enable depth clipping
    rastDesc.scissorEnable = false;                  // Not using scissors
    rastDesc.multisampleEnable = false;  
    rastDesc.antialiasedLineEnable = false;                

    graphics->CreateRasterizerState(rastDesc, &rast);

    // Depth-stencil
    DepthStencilDesc depDesc{};
    depDesc.depthEnable = true;                         // Enable depth testing
    depDesc.depthWriteMask = true;          // Write all depths
    depDesc.depthFunc = ComparisonFunc::LessEqual;    // Pass if <= existing depth

    depDesc.stencilEnable = false;                        // Disable stencil by default
    depDesc.stencilReadMask = 0xFF;
    depDesc.stencilWriteMask = 0xFF;

    // If you want stencil ops (rare for defaults), init like this:
    depDesc.frontFace.failOp = StencilOp::Keep;
    depDesc.frontFace.depthFailOp = StencilOp::Keep;
    depDesc.frontFace.passOp = StencilOp::Keep;
    depDesc.frontFace.func = ComparisonFunc::Always;

    depDesc.backFace = depDesc.frontFace;

    graphics->CreateDepthStencilState(depDesc, &depth);

    vD = createShader(rootDevice, choice, ShaderDesc::Type::Vertex, "./titlebarvert.spv");
    pD = createShader(rootDevice, choice, ShaderDesc::Type::Pixel, "./titlebarfrag.spv");

    {
        ShaderDesc vertexShaderDesc;
        vertexShaderDesc.bytecode = vD.data.data();
        vertexShaderDesc.bytecodeSize = vD.data.size();
        vertexShaderDesc.ogData = vD.ogData;

        vertexShaderDesc.type = ShaderDesc::Type::Vertex;

        graphics->CreateShader(vertexShaderDesc, &vertexShader);
    }


    graphics->CreateInputLayout({ {
    {
     0, VertexAttribType::Float3, 0, offsetof(Vertex, x), false
    }, {
     1, VertexAttribType::Float2, 0, offsetof(Vertex, u), false
    }
}, sizeof(Vertex), &vertexShader }, &inputLayout);


    {
        ShaderDesc pixelShaderDesc;
        pixelShaderDesc.bytecode = pD.data.data();
        pixelShaderDesc.bytecodeSize = pD.data.size();
        pixelShaderDesc.ogData = pD.ogData;
        pixelShaderDesc.type = ShaderDesc::Type::Pixel;

        graphics->CreateShader(pixelShaderDesc, &pixelShader);
    }

    return true;
}

void bnMessageBox::UpdateViewports(UINT width, UINT height)
{

    FLOAT windowWidth = static_cast<FLOAT>(width);
    FLOAT windowHeight = static_cast<FLOAT>(height);

    if (iTitleBar.Exists() && iMain.Exists()) {
        iTitleBar.Release();
        iMain.Release();
    }

    if (titleBar->config.enabled) {
        WindowRect title_bar_rect = titleBar->win32_titlebar_rect();
        UINT dpi = GetDpiForWindow(handle);
        FLOAT titleBarHeight = FLOAT(title_bar_rect.bottom - title_bar_rect.top);
        // Viewport 0: Titlebar (top portion)
        vpTitlebar.x = 0;
        vpTitlebar.y = 0;
        vpTitlebar.width = windowWidth;
        vpTitlebar.height = windowHeight - titleBarHeight;
        vpTitlebar.minDepth = 0.0f;
        vpTitlebar.maxDepth = 1.0f;

        // Viewport 1: Main Application (remaining area)
        vpMain.x = 0;
        vpMain.y = titleBarHeight;
        vpMain.width = windowWidth;
        vpMain.height = windowHeight - titleBarHeight;
        vpMain.minDepth = 0.0f;
        vpMain.maxDepth = 1.0f;



    }
    else {

        vpTitlebar.x = 0;
        vpTitlebar.y = 0;
        vpTitlebar.width = 0;
        vpTitlebar.height = 0;
        vpTitlebar.minDepth = 0.0f;
        vpTitlebar.maxDepth = 1.0f;

        vpMain.x = 0;
        vpMain.y = 0;
        vpMain.width = windowWidth;
        vpMain.height = windowHeight;
        vpMain.minDepth = 0.0f;
        vpMain.maxDepth = 1.0f;
    }

    if (!iTitleBar.Exists() && !iMain.Exists()) {
        iTitleBar.Resolve(rootDevice->CreateViewPort({ &vpTitlebar }));
        iMain.Resolve(rootDevice->CreateViewPort({ &vpMain }));
    }
}


void bnMessageBox::run()
{
    while (running) {
        PollMessages();
        if (!running) break;
        if (pauseRender) continue;
        gDeltaTimes[handle] = GetDeltaTime();
        if (IsIconic(handle)) {
            rootAudioDevice->Pause();
            continue;
        }
        POINT pt;
        GetCursorPos(&pt);

        RECT parentRect;
        GetWindowRect(parentHandle, &parentRect);

        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) { // left button down
            if (PtInRect(&parentRect, pt) && GetForegroundWindow() != handle) {
                // Parent window clicked
                if (!flickering) {
                    flickering = true;
                    int totalFrames = 1.75f * 60; // 3 seconds
                    flickerFramesRemaining = totalFrames;
                    flickerFrameCounter = 0;
                    titleBar->flickerEffect = true; // start flicker
                }
                //else {
                //    flickerFramesRemaining += 1.75f * 60;
                //}
            }
        }

        if (flickering) {
            if (flickerFramesRemaining <= 0) {
                flickering = false;
                titleBar->flickerEffect = false;
            }
            else {
                flickerFrameCounter++;

                // Toggle flickerEffect every 5 frames
                if (flickerFrameCounter >= 5) {
                    updateAll = true;
                    titleBar->flickerEffect = !titleBar->flickerEffect;
                    flickerFrameCounter = 0;
                }

                flickerFramesRemaining--;

                // Stop flicker completely at the end
                if (flickerFramesRemaining == 0) {
                    updateAll = false;
                    titleBar->flickerEffect = false;
                }
            }
        }



        titleBar->Update();
        RenderFrame();
        //?
    }

    canBeDeleted = true;
}


void Resample16BitToFloat(const int16_t* src, size_t numSamples,
    int srcRate, int dstRate, int channels,
    std::vector<float>& output)
{
    size_t newFrames = numSamples * dstRate / srcRate; // number of frames after resampling
    output.resize(newFrames * channels);               // total elements

    double ratio = static_cast<double>(srcRate) / dstRate;

    for (size_t i = 0; i < newFrames; ++i) {
        double idx = i * ratio;
        auto idxInt = static_cast<size_t>(idx);
        double frac = idx - idxInt;

        for (int c = 0; c < channels; ++c) {
            float s0 = src[(idxInt * channels) + c] / 32768.0f;
            float s1 = (idxInt + 1 < numSamples) ? src[((idxInt + 1) * channels) + c] / 32768.0f : s0;
            output[i * channels + c] = static_cast<float>(s0 + frac * (s1 - s0));
        }
    }
}

void ApplyReverb(float* buffer, size_t frames, int channels, float decay = 0.5f, int delaySamples = 4410) {
    // delaySamples = 100ms at 44.1kHz
    for (size_t i = delaySamples; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            size_t idx = i * channels + c;
            size_t delayedIdx = (i - delaySamples) * channels + c;
            buffer[idx] += buffer[delayedIdx] * decay;
        }
    }
}



void bnMessageBox::RenderFrame() {

    //if (updateIt > gEConfig.framesInFlight) {
    /*ShowWindow(handle, SW_SHOW);*/
    //}
    //if (!firstFrame) {
    //    firstFrame = false;

    //}

    if (stream && stream->finished) {
        //rootAudioDevice->QueueStream(stream);
        delete stream;
        stream = nullptr;
    }


    RECT clientRect;
    GetClientRect(handle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;


    //auto simple = graphics->makeSimple();

    auto advanced = graphics->makeAdvanced();
    auto list = advanced->GetCommandList();


    rootDevice->BeginFrame();
    titleBar->RenderFrame(graphics, advanced, list, &inputLayout, &samplerState, &blending, &rast, &depth, &copyTexturePool, &vertexShader, &pixelShader, &iTitleBar, &dPool, &dSetLayout);

    UpdateViewports(width, height);
    //graphics->window = this;
    width = vpMain.width;
    height = vpMain.height;

    SkImageInfo info = SkImageInfo::Make(
        width,
        height,
        kBGRA_8888_SkColorType,   // matches Win32 DIB
        kPremul_SkAlphaType
    );

    size_t rowBytes = info.minRowBytes();
    bool isFirstFrame = false;
    if (pixels.size() < 1) {

        isFirstFrame = true;
        firstFrame = true;
        pixels = std::vector<uint8_t>(rowBytes * height);

        if (rootAudioDevice) {
            // load wav
            auto errorData = new Data("./bError.wav");
            auto chunks = GetPCMData(*errorData->getDataPtr());
            printf("PCM data size: %zu bytes\n", chunks.size);

            auto config = rootAudioDevice->GetMixerFormat();

            // Prepare PCM format
            PCMFormat format{};
            format.sampleRate = config.sampleRate;  // Use device sample rate
            format.channels = config.channels;
            format.bitsPerSample = 32;  // Float32
            format.type = config.type;

            // Calculate input frames
            uint32_t totalInputFrames = chunks.size / (chunks.channels * (chunks.bitsPerSample / 8));

            // Resample & convert to float
            std::vector<float> output;
            output.reserve(totalInputFrames * config.channels); // Reserve enough space
            Resample16BitToFloat(static_cast<const int16_t*>(chunks.data),
                totalInputFrames,
                chunks.sampleRate,
                config.sampleRate,
                config.channels,
                output);

            // Queue the stream
            stream = rootAudioDevice->QueueStream(output.data(), output.size() * sizeof(float), format);
            auto bla = rootAudioDevice->ListPlaybackDevices();
            for (auto& blas : bla) {
                std::wcout << blas << std::endl;
            }

            // Clean up
            delete errorData;

        }

        auto canvas = SkCanvas::MakeRasterDirect(
            info,
            pixels.data(),
            rowBytes
        );

        if (!canvas) return;

        canvas->clear({ config.clearColor.r / 255, config.clearColor.r / 255, config.clearColor.r / 255, config.clearColor.a });

        SkPaint textPaint;
        textPaint.setColor(SkColorSetARGB(
            255,
            255,
            255,
            255
        ));
        textPaint.setAntiAlias(true);  // smooth text


        // Skia uses float for coordinates
        SkRect textRect = SkRect::MakeLTRB(
            0,
            clientRect.top,
            0,
            clientRect.bottom
        );

        float maxTextWidth = width - 50.0f;

        float lineHeight = descriptionFont.getSpacing();
        float totalHeight = wrappedLines.size() * lineHeight;

        // Start vertically centered
        float startY = clientRect.top + (height - totalHeight) / 2.0f;

        for (size_t i = 0; i < wrappedLines.size(); ++i) {
            const std::u16string& line = wrappedLines[i];

            for (char16_t c : line) {
                printf("U+%04X\n", c);
            }

            SkRect lineBounds;
            descriptionFont.measureText(line.data(), line.size() * sizeof(char16_t),
                SkTextEncoding::kUTF16, &lineBounds);

            float lineWidth = lineBounds.width();
            float x = clientRect.left + (width - lineWidth) / 2.0f - lineBounds.left();
            float y = startY + i * lineHeight - lineBounds.top();

            auto blob = SkTextBlob::MakeFromText(
                line.data(),
                line.size() * sizeof(char16_t),
                descriptionFont,
                SkTextEncoding::kUTF16
            );

            canvas->drawTextBlob(blob, x, y, textPaint);
        }
    }


    {
        if (isFirstFrame) {
            graphics->PushGroup("Initalization Canvas");
            TextureDesc tbDesc;
            tbDesc.format = choice != VULKAN ? TextureFormat::BGRA8_UNorm : TextureFormat::BGRA8_UNorm_SRGB;
            tbDesc.width = width;
            tbDesc.height = height;
            tbDesc.widthBytes = rowBytes;
            tbDesc.CpuAccessWrite = true;

            //mainTexture.Resolve(rootDevice->CreateTexture(tbDesc, nullptr));
            graphics->CreateTexture(tbDesc, nullptr, &mainTexture);
            // }

            // if (isFirstFrame) {


            BufferDesc stagingDesc = {};
            stagingDesc.size = rowBytes * height * 4;
            stagingDesc.type = BufferType::Staging;
            //stagingDesc.format = choice == VULKAN ? TextureFormat::BGRA8_UNorm_SRGB : TextureFormat::BGRA8_UNorm;

            graphics->CreateBuffer(stagingDesc, nullptr, &stagingBuffer2);
            advanced->MapBufferMemory(&stagingBuffer2, &bufferData);

            for (int y = 0; y < height; ++y) {
                MemoryCommands::Copy(advanced->getCommands(),
                    &bufferData,
                    pixels.data() + y * rowBytes,
                    rowBytes,
                    y* rowBytes);
            }

            advanced->UnmapBufferMemory(&stagingBuffer2);

            BufferImageCopyDesc region{};
            region.imageSubresource.aspectMask = IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                1
            };
            //region.bufferRowLength = rowBytes;          // pixels per row
            //region.bufferImageHeight = height;      // rows

            auto pool = advanced->BeginSingleTimeCommands(&copyTexturePool);
            advanced->CopyBufferToImage(pool, &stagingBuffer2, &mainTexture, region);
            advanced->EndSingleTimeCommands(pool);


            if (stagingBuffer2) graphics->ReleaseBuffer(&stagingBuffer2);

            // }

            // if (isFirstFrame) {
            std::vector<Vertex> quadVerts = {
                {-1,  -1, 0, 0, 1},
                {-1, 1, 0, 0, 0},
                { 1,  -1, 0, 1, 1},
                { 1, 1, 0, 1, 0},
                  };

            BufferDesc tbBufferDesc;
            tbBufferDesc.type = BufferType::Vertex;
            tbBufferDesc.size = std::size(quadVerts);
            tbBufferDesc.byteWidth = sizeof(Vertex) * std::size(quadVerts);
            tbBufferDesc.stride = sizeof(Vertex);
            tbBufferDesc.dynamic = false;

            graphics->CreateBuffer(tbBufferDesc, quadVerts.data(), &mainVertice);
            graphics->PopGroup();
            // }
        }
    }

    graphics->PushGroup("Main Pipeline Render");

    if (!pipeline || !set) {
        auto builderObj = advanced->CreatePipelineBuilder();
        auto builder = builderObj->Get();

        advanced->BuilderAddShader(builderObj, &pixelShader);
        advanced->BuilderAddShader(builderObj, &vertexShader);
        advanced->BuilderSetBlendState(builderObj, &blending);
        advanced->BuilderSetInputLayout(builderObj, &inputLayout);
        advanced->BuilderSetRasterizer(builderObj, &rast);
        advanced->BuilderSetDepthStencil(builderObj, &depth);
        advanced->BuilderSetDescriptorPool(builderObj, &dPool);
        advanced->BuilderSetDescriptorSetLayout(builderObj, &dSetLayout);

        pipeline = advanced->CreatePipeline(builderObj);
        set = advanced->CreateDescriptorSet(pipeline);
        advanced->SetDescriptorSetTexture(set, &mainTexture);
        advanced->SetDescriptorSetSamplerState(set, &samplerState);
        advanced->UpdateDescriptorSet(set);
    }


       list->BindViewPort(&iMain);
       list->BindPipeline(pipeline);
       list->BindDescriptorSet(set);
       list->BindBuffer(&mainVertice);
       list->Draw(PrimitiveType::TriangleStrip, 4);
       advanced->ReleaseCommandList(list);

       graphics->PopGroup();
       graphics->Submit();



        if (!isFirstFrame) {

            //firstFrame = true;
           /* auto config = rootAudioDevice->GetMixerFormat();
            auto totalInputFrames = (stream->totalBytes / sizeof(float)) / (stream->format.channels * (stream->format.bitsPerSample / 8));
            ApplyReverb(reinterpret_cast<float*>(stream->data), totalInputFrames, stream->format.channels, 0.5f, config.sampleRate);*/

            //auto errorData = new Data(R"(C:\Users\isloe\Downloads\bError.wav)");
            //auto chunks = GetPCMData(errorData->getData());
            //printf("PCM data size: %zu bytes\n", chunks.size);
            //auto config = rootAudioDevice->GetMixerFormat();

            //PCMFormat format;
            //format.sampleRate = chunks.sampleRate;
            //format.channels = chunks.channels;
            //format.bitsPerSample = chunks.bitsPerSample;
            //format.type = config.type;

            //// Queue the stream

            //auto ipfr = config.channels * 2;
            //auto totalInputFrames = chunks.size / (chunks.channels * (chunks.bitsPerSample / 8));

            //std::vector<float> output;
            //Resample16BitToFloat(reinterpret_cast<const int16_t*>(chunks.data), totalInputFrames, chunks.sampleRate, config.sampleRate, config.channels, output);
            //auto stream2 = rootAudioDevice->QueueStream(output.data(), output.size() * sizeof(float), format);
            //delete errorData;
        }


    rootDevice->EndFrame();
    rootDevice->Present();
    // rootAudioDevice->Unpause();
    // rootAudioDevice->UpdateFrame();
    firstFrame = false;
    //while (true) {
    //    Sleep(5500);
    //    break;
    //}
}

void bnMessageBox::ResizeSwapChain(UINT width, UINT height) {
    rootDevice->Resize(width, height);
}


void bnMessageBox::init()
{
    if (!rootDevice) {
        InitGraphicsSystem();
    }

    
    //SetWindowLong(handle, GWL_EXSTYLE,
    //    GetWindowLong(handle, GWL_EXSTYLE) | WS_EX_LAYERED);
    //SetLayeredWindowAttributes(handle, 0, 0, LWA_ALPHA);
    //ShowWindow(handle, SW_SHOW);

    RECT clientRect;
    GetClientRect(handle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;
    ResizeSwapChain(width, height);

    
}

void bnMessageBox::preDestroyFunctions()
{
}

void ApplyVibrato(float* data, size_t numFrames, int channels, float sampleRate,
    float depthHz = 5.0f, float rateHz = 5.0f)
{
    // depthHz = max pitch deviation in Hz
    // rateHz = how fast the pitch oscillates
    std::vector<float> copy(data, data + numFrames * channels);

    for (size_t frame = 0; frame < numFrames; ++frame) {
        // sine-based pitch modulation
        float mod = std::sin(2.0f * 3.14159265359f * rateHz * frame / sampleRate) * depthHz;
        float readPos = frame + mod; // offset frame by modulation

        size_t idxInt = static_cast<size_t>(readPos);
        float frac = readPos - idxInt;

        for (int c = 0; c < channels; ++c) {
            float s0 = (idxInt < numFrames) ? copy[idxInt * channels + c] : 0.0f;
            float s1 = (idxInt + 1 < numFrames) ? copy[(idxInt + 1) * channels + c] : s0;
            data[frame * channels + c] = s0 + frac * (s1 - s0);
        }
    }
}


void bnMessageBox::shutdown(bool perm)
{
    running = false;
    titleBar->ReleaseRenderVars(rootDevice);
    pipeline->Get()->Release();
    set->DestroyHandle();
    pipeline->DestroyHandle();
    rootDevice->ReleaseTexture(mainTexture);
    rootDevice->ReleaseBuffer(mainVertice);
    rootDevice->ReleaseShader(vertexShader);
    rootDevice->ReleaseShader(pixelShader);
    if (usingExpl) {
        gEngineExpl->ReleaseDescriptorPool(dPool);
        gEngineExpl->ReleaseDescriptorSetLayout(dSetLayout);
        gEngineExpl->ReleaseCommandPool(copyTexturePool);
        gEngineExpl->WaitTillImFree();
    }
    if (samplerState) samplerState->Release();
    if (inputLayout) inputLayout->Release();
    if (rootDevice) rootDevice->Shutdown();
    if (iTitleBar || iMain) {
        iTitleBar.Invalidate();
        iMain.Invalidate();
    }
    if (blending) blending.Invalidate();
    if (rast) rast.Invalidate();
    if (depth) depth.Invalidate();
    rootAudioDevice->Shutdown();
    delete stream;
    SkGraphics::PurgeAllCaches();
}

WindowCallbackCodes bnMessageBox::windowCallback(WindowEvent* event)
{
    if (!titleBar) {
        return WindowCallbackCodes::DEFAULT;
    }

    switch (event->type) {
        case WindowEventType::InitalizeDraw: {
            if (firstFrame) {
#ifdef WIN32
                win32Window::paintBasedOnConfig(configuration, handle);
#elif defined(__linux__)
#endif
                return WindowCallbackCodes::OKAY;
            } else return WindowCallbackCodes::DEFAULT;
        }
        case WindowEventType::NonClientAreaCalcSize: {
        if (!event->wordParameter) return WindowCallbackCodes::DEFAULT;

        if (event->wordParameter == TRUE) {
            if (titleBar) titleBar->OnNCCalcResult(event->longParameter);
            return WindowCallbackCodes::OKAY;
        }
        else {
            return WindowCallbackCodes::DEFAULT;
        }
    }
    case WindowEventType::Initalize: {
        PROCESS_POWER_THROTTLING_STATE state = {};
        state.Version = 1;
        state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask = 0; // Disable throttling

        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));


        if (configuration->logo != nullptr && configuration->logo->getState() == 1) {
            hIcon = CreateIconFromLogo(configuration->logo->getData().data(), configuration->logo->getData().size());
            if (hIcon != nullptr) {
                SendMessage(handle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessage(handle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }
            else {
                printf("Unsupported Image Format for your logo!");
            }
        }

        SetWindowPos(handle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

        if (!rootDevice) {
            InitGraphicsSystem();
            // MessageBoxW(handle, L"Failed to initialize DirectX 11", L"Error", MB_ICONERROR);
        }


        break;
    }
        case WindowEventType::NonClientButtonDoubleClick: {
        // Prevent double-click maximize on the title bar
        if (event->wordParameter == HTCAPTION) {
            return WindowCallbackCodes::OKAY;
        }
        return WindowCallbackCodes::DEFAULT;

    }
        case WindowEventType::KeyDown: {
        int key = event->key;
        if (key == 'M') {
            std::vector<float> outp;
            PCMFormat format;
            LoadMP3ToPCM("./ahhsong.mp3", outp, format);
            auto config = rootAudioDevice->GetMixerFormat();
            size_t totalInputFrames = outp.size() / format.channels;


            std::vector<float> output;
            ResampleFloat(outp.data(), totalInputFrames, format.sampleRate, config.sampleRate, config.channels, output);
            format.channels = config.channels; // after resample
            format.sampleRate = config.sampleRate; // after resample
            format.bitsPerSample = config.bitsPerSample;
            format.type = config.type;
            totalInputFrames = output.size() / (format.channels * (format.bitsPerSample / 8));
            outp.clear();
            auto stream2 = rootAudioDevice->QueueStream(output.data(), output.size() * sizeof(float), format);

            uint32_t bytesPerFrame = stream2->format.GetBytesPerFrame();
            uint32_t totalFrames = stream2->totalBytes / bytesPerFrame;
            ApplyReverb((float*)stream2->data, totalFrames, 2, 0, format.sampleRate);

            //size_t bytesPerFrame = stream2->format.GetBytesPerFrame();
            //size_t frameOffset = static_cast<size_t>(202 * stream2->format.sampleRate);
            //size_t byteOffset = frameOffset * bytesPerFrame;
            //if (byteOffset >= stream2->totalBytes) {
            //    stream2->bytesWritten = stream->totalBytes; // end of stream
            //   
            //} else  stream2->bytesWritten = byteOffset;



            std::thread([=]() {
                auto now = std::chrono::high_resolution_clock::now();
                float durationSec = float(stream2->totalBytes) / (stream2->format.channels * (stream2->format.bitsPerSample / 8) * stream2->format.sampleRate);



                //auto startTime = now - std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
                //    std::chrono::duration<float>(secondsWritten)
                //);

                while (!stream2->finished && rootAudioDevice) {

                    if (rootAudioDevice->Paused()) continue;

                   /* now = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> elapsed = now - startTime;*/


                    //auto mixStream = rootAudioDevice->GetMixerBuffer()

                    //double currentTime = elapsed.count();
                    size_t framesWritten = stream2->bytesWritten / bytesPerFrame;
                    auto secondsWritten = static_cast<double>(framesWritten) / stream2->format.sampleRate;
                    double timeLeft = durationSec - secondsWritten;

                    if (timeLeft < 0.0) timeLeft = 0.0;

                    printf("[AudioStream] Current Time: %.3f s / %.3f s\n", secondsWritten, durationSec);

                    //if (currentTime > 3) {
                    //    //ApplyVibrato((float*)stream2->data, totalFrames, 2, config.sampleRate);
                    //    if (currentTime > 4.4 && currentTime < 4.41) {
                    //        
                    //    }
                    //}

                    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                printf("[AudioStream] Finished playing\n");
                delete stream2;
                }).detach();

        }
    }
    case WindowEventType::Activate: {
        // if (!running) return WindowCallbackCodes::DEFAULT;
        titleBar->Update();
        if (rootAudioDevice) rootAudioDevice->Unpause();
        return WindowCallbackCodes::DEFAULT;
    }
        case WindowEventType::NonClientHitTest: {
        i64Pointer hit = GetDefaultProcedure(event->originalMessage, event->wordParameter, event->longParameter);
        switch (hit) {
            case HTNOWHERE:
            case HTRIGHT:
            case HTLEFT:
            case HTTOPLEFT:
            case HTTOP:
            case HTTOPRIGHT:
            case HTBOTTOMRIGHT:
            case HTBOTTOM:
            case HTBOTTOMLEFT: {
                event->customResult = hit;
                return WindowCallbackCodes::CUSTOM_RESULT;
            }
        }

        auto hitRes = titleBar->OnNCHitTest(event->longParameter);
        if (hitRes != HTTRANSPARENT) event->customResult = hitRes;
        else event->customResult = hit;

        return WindowCallbackCodes::CUSTOM_RESULT;
    }
    case WindowEventType::NonClientMouseMove: {
        titleBar->OnNCMouseMove();
        return WindowCallbackCodes::DEFAULT;
    }
    case WindowEventType::MouseMove: {
        // Clear hover if mouse leaves title bar buttons
        titleBar->OnMouseMove();
        return WindowCallbackCodes::DEFAULT;
    }
   
    case WindowEventType::NonClientMouseButtonDown: {
            if (event->button == 0) {
                if (titleBar->OnNCMouseButtonDown() == 0) return WindowCallbackCodes::OKAY;
                rootAudioDevice->Pause();

                if (flickering) {
                    flickerFramesRemaining = 0;
                }

                return WindowCallbackCodes::DEFAULT;
            }
    }
    case WindowEventType::NonClientMouseButtonUp: {
            if (event->button == 0) {
                titleBar->OnNCMouseButtonUp();
                return WindowCallbackCodes::DEFAULT;
            } else if (event->button == 1) {
                if (event->wordParameter == HTCAPTION) {
                    rootAudioDevice->Pause();
                    titleBar->OnNCRMouseButtonUp(event->longParameter);
                }
                return WindowCallbackCodes::DEFAULT;
            }
    }
    case  WindowEventType::SetCursor: {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        break;
    }
    case WindowEventType::Destroy: {

        shutdown();
        delete rootDevice;
        rootDevice = nullptr;
        // PostQuitMessage(0);
        return WindowCallbackCodes::OKAY;
    }
        case WindowEventType::NonClientMouseLeave: {
        titleBar->OnNCMouseLeave();
        break;
    }
        case WindowEventType::GetMinMaxInfo:
    {
        if (titleBar->config.enabled) {
            titleBar->OnGetMaxInfo(event->longParameter);
            return WindowCallbackCodes::OKAY;
        }
        else return WindowCallbackCodes::DEFAULT;
    }
    case WindowEventType::Resize: {
        if (event->wordParameter != SIZE_MINIMIZED) {
            UINT newWidth = LOWORD(event->longParameter);
            UINT newHeight = HIWORD(event->longParameter);

            if (rootDevice) {
                ResizeSwapChain(newWidth, newHeight);
                UpdateViewports(newWidth, newHeight);
                updateAll = true;
                
                if (choice == VULKAN) {
                    gEngineExpl->WaitForNewFrame();
                }
                // titleBar->Update();
                RenderFrame();
                // rootAudioDevice->UpdateFrame();
                ////if (choice == D3D11) {
                //gDeltaTimes[handle] = GetDeltaTime();
                //UpdateUserThread();
                //UpdateTitleBar();
                //RenderFrame();
                ////}
                updateAll = false;
                pauseRender = false;
            }
        }
        else {
            //pauseRender = true;
        }
        return WindowCallbackCodes::OKAY;
    }
        default:
            return WindowCallbackCodes::DEFAULT;
    }
}
