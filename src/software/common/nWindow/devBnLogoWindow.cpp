#include "devBnLogoWindow.h"
#include "nGraphics/bnGraphics.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "nCommon/MemoryCommands.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "include/core/SkStream.h"
#include "nGUI/skia/interfaces/IGUISize.h"
#include "nGUI/skia/interfaces/svg/IGUISVGDOM.h"

ResourceHandle<IGUISVGDOM> load_svg(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return nullptr; // Or handle error accordingly
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        return nullptr;
    }
    vec8 buffer(static_cast<size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return nullptr;
    }
    // todo: add font/image providers
    return IGUISVGDOM::CreateFromBinary(buffer.data(), buffer.size());
}

devBnLogoWindow::devBnLogoWindow()
{
    auto titleBarConfig = bnWindowTitlebarConfig();
    titleBarConfig.borderColor = { 0, 0, 0 };
    titleBarConfig.sysButtons = 0;
    titleBarConfig.enabled = true;
    titleBarConfig.properties = TitleBarProperties::HIDEUNFOCUS;
    windowHandle.Resolve(new bnUserWindow({ 1920, 1080, nullptr, L"BoraError1", L"Bora", -1, new Data(R"(./borat.png)"),
        {

            0, 0, 0,1,

        }, {0,0,0}, 8, true, &titleBarConfig, staticUpdateFunction, this }));
    windowHandle->run();
    windowHandle.Release();
}

void devBnLogoWindow::staticUpdateFunction(bnUserWindow* window, void* userObject, unsigned int msg, uintptr_t wParam, intptr_t lParam)
{
    if (userObject) {
        devBnLogoWindow* interpWindow = static_cast<devBnLogoWindow*>(userObject);
        if (!interpWindow) return;

        interpWindow->updateFunction(window, msg, wParam, lParam);
    }
}

void devBnLogoWindow::updateFunction(bnUserWindow* window, unsigned int msg, uintptr_t wParam, intptr_t lParam)
{
    std::chrono::steady_clock::time_point updateStart;
    updateStart = std::chrono::steady_clock::now();

    switch (msg) {
    case ON_CHANGE_GRAPHICS: {
        auto advanced = window->userGraphics->makeAdvanced();
        window->userGraphics->ReleaseShader(&fragmentShader);
        window->userGraphics->ReleaseShader(&vertexShader);
        window->userGraphics->ReleaseTexture(&texture);
        advanced->UnmapBufferMemory(&uboBuffer);
        window->userGraphics->ReleaseBuffer(&uboBuffer);
        window->userGraphics->ReleaseBuffer(&vertexBuffer);
        advanced->ReleaseDescriptorSetLayout(&dSetLayout);
        advanced->ReleaseDescriptorPool(&dPool);
        advanced->ReleasePipeline(&pipeline);
        set.Reset();
        window->userGraphics->ReleaseInputLayout(&inputLayout);
        samplerState.Invalidate();
        break;
    }
    case ON_RELEASE: {
        // release everything
        auto advanced = window->userGraphics->makeAdvanced();
        window->userGraphics->ReleaseShader(&fragmentShader);
        window->userGraphics->ReleaseShader(&vertexShader);
        window->userGraphics->ReleaseTexture(&texture);
        advanced->UnmapBufferMemory(&uboBuffer);
        window->userGraphics->ReleaseBuffer(&uboBuffer);
        window->userGraphics->ReleaseBuffer(&vertexBuffer);
        advanced->ReleaseDescriptorSetLayout(&dSetLayout);
        advanced->ReleaseDescriptorPool(&dPool);
        advanced->ReleasePipeline(&pipeline);
        set.Reset();
        window->userGraphics->ReleaseInputLayout(&inputLayout);
        window->userGraphics->ReleaseSamplerState(&samplerState);
        // samplerState.Invalidate();
        break;
    }
    case ON_UPDATE: {
        if (!fragmentShader && !vertexShader) {
            vD = createShader(window->rootDevice, window->choice, ShaderDesc::Type::Vertex, "./logoshader.vert.spv");
            pD = createShader(window->rootDevice, window->choice, ShaderDesc::Type::Pixel, "./logoshader.frag.spv");

            {
                ShaderDesc vertexShaderDesc;
                vertexShaderDesc.bytecode = vD.data.data();
                vertexShaderDesc.bytecodeSize = vD.data.size();
                vertexShaderDesc.ogData = vD.ogData;

                vertexShaderDesc.type = ShaderDesc::Type::Vertex;

                window->userGraphics->CreateShader(vertexShaderDesc, &vertexShader);
            }

            {
                ShaderDesc pixelShaderDesc;
                pixelShaderDesc.bytecode = pD.data.data();
                pixelShaderDesc.bytecodeSize = pD.data.size();
                pixelShaderDesc.ogData = pD.ogData;
                pixelShaderDesc.type = ShaderDesc::Type::Pixel;

                window->userGraphics->CreateShader(pixelShaderDesc, &fragmentShader);
            }

        }

        if (!inputLayout.Exists()) {
            window->userGraphics->CreateInputLayout({ {

                {  0, VertexAttribType::Float3, 0, offsetof(Vertex, x), false},
                    { 1, VertexAttribType::Float2, 0, offsetof(Vertex, u), false },
                }, sizeof(Vertex), &vertexShader }, &inputLayout);
        }

        auto graphs = window->userGraphics->makeAdvanced();

        if (!texture.Exists()) {

            window->userGraphics->CreateSamplerState({
            0, TextureFilter::Linear,TextureFilter::Linear,TextureFilter::Linear
                }, &samplerState);


            TextureDesc tbDesc;
            tbDesc.format = window->choice == VULKAN ? TextureFormat::RGBA8_UNorm_SRGB : TextureFormat::RGBA8_UNorm;
            tbDesc.width = window->getWidth();
            tbDesc.height = window->getHeight();
            tbDesc.widthBytes = tbDesc.width * 4;
            tbDesc.CpuAccessWrite = true;
            tbDesc.isRenderTarget = true;
            window->userGraphics->CreateTexture(tbDesc, nullptr, &texture);
            // in-engine gui system incomplete.
            window->userGraphics->CallFunctionInSubmission([=](IGraphicsDevice* device) {
                auto surface = window->defaultGUIRenderer->createFrameSurface(texture);
                auto svg = load_svg("blogo_anim.svg");
                IGUISize containerSize = IGUISize::Make(surface->GetWidth(), surface->GetHeight());
                svg->setContainerSize(containerSize);
                svg->render(surface->GetCanvas());
            });

        }

      

        if (!dPool) {
            graphs->CreateDescriptorPool({
            {{ DescriptorType::UniformBuffer, 1}, {DescriptorType::CombinedImageSampler, 1}},
            1, DescriptorPoolFlags::FreeDescriptor
                }, &dPool);

            graphs->CreateDescriptorSetLayout({
                        {{0,DescriptorType::UniformBuffer, 1, IShaderStage::Vertex },
                        {1, DescriptorType::CombinedImageSampler, 1, IShaderStage::Fragment }}
                }, &dSetLayout);
        }


        if (!uboBuffer.Exists()) {
            BufferDesc cBufferDesc;
            cBufferDesc.type = BufferType::Constant;
            cBufferDesc.byteWidth = sizeof(devBnLogoWindow::UBOData);
            cBufferDesc.size = sizeof(devBnLogoWindow::UBOData);
            cBufferDesc.dynamic = true;

            window->userGraphics->CreateBuffer(cBufferDesc, nullptr, &uboBuffer);
        }

        if (!mappedData) {
            graphs->MapBufferMemory(&uboBuffer, &mappedData);
        }

        uboBufferData.uFillColor[0] = 0;
        uboBufferData.uFillColor[1] = 0;
        uboBufferData.uFillColor[2] = 0;
        //uboBufferData.uFillColor[3] = 0.75f;

        uboBufferData.uStrokeColor[0] = 1.0f;
        uboBufferData.uStrokeColor[1] = 1.0f;
        uboBufferData.uStrokeColor[2] = 1.0f;
        uboBufferData.uStrokeColor[3] = 0.55f;

        uboBufferData.uTime += gDeltaTimes[window->handle] * 2;

        uboBufferData.uSpotDir[0] = 0.3f;
        uboBufferData.uSpotDir[1] = -0.8f;

        uboBufferData.uSpotRadius = 0.5f;
        uboBufferData.uSpotSoftness = 0.1f;

        MemoryCommands::Copy(graphs->getCommands(), &mappedData, &uboBufferData, sizeof(devBnLogoWindow::UBOData));
        // graphs->MemoryCopy(&mappedData, &uboBufferData, sizeof(devBnLogoWindow::UBOData));

        if (!pipeline || !set) {
            auto builderObj = graphs->CreatePipelineBuilder();
            graphs->BuilderAddShader(builderObj, &fragmentShader);
            graphs->BuilderAddShader(builderObj, &vertexShader);
            graphs->BuilderSetBlendState(builderObj, &window->blending);
            graphs->BuilderSetInputLayout(builderObj, &inputLayout);
            graphs->BuilderSetRasterizer(builderObj, &window->rast);
            graphs->BuilderSetDepthStencil(builderObj, &window->depth);
            graphs->BuilderSetDescriptorPool(builderObj, &dPool);
            graphs->BuilderSetDescriptorSetLayout(builderObj, &dSetLayout);
            graphs->CreatePipeline(builderObj, &pipeline);
            graphs->ReleaseBuilder(builderObj);
            graphs->CreateDescriptorSet(&pipeline, 0, &set);

            graphs->SetDescriptorSetBuffer(&set, &uboBuffer);
            graphs->SetDescriptorSetTexture(&set, &texture, 1);
            graphs->SetDescriptorSetSamplerState(&set, &samplerState, 1);
            graphs->UpdateDescriptorSet(&set);
        }

        if (!vertexBuffer) {
            float texWidth = 500.0f;
            float texHeight = 500.0f;
            float winWidth = window->getWidth();
            float winHeight = window->getHeight();
            float halfWidthNDC = (texWidth / 2.0f) / winWidth;  // horizontal scale
            float halfHeightNDC = (texHeight / 2.0f) / winHeight; // vertical scale

            Vertex vertices[] = {
    { -halfWidthNDC, -halfHeightNDC, 0.0f, 0.0f, 0.0f }, // bottom-left
    {  halfWidthNDC, -halfHeightNDC, 0.0f, 1.0f, 0.0f }, // bottom-right
    { -halfWidthNDC,  halfHeightNDC, 0.0f, 0.0f, 1.0f }, // top-left
    {  halfWidthNDC,  halfHeightNDC, 0.0f, 1.0f, 1.0f }, // top-right
            };

            BufferDesc tbBufferDesc;
            tbBufferDesc.type = BufferType::Vertex;
            tbBufferDesc.size = std::size(vertices);
            tbBufferDesc.stride = sizeof(Vertex);
            tbBufferDesc.dynamic = false;

            window->userGraphics->CreateBuffer(tbBufferDesc, &vertices, &vertexBuffer);
        }

        auto list = graphs->GetCommandList();
        list->BindPipeline(&pipeline);
        list->BindDescriptorSet(&set);
        list->BindBuffer(&vertexBuffer);
        list->Draw(PrimitiveType::TriangleStrip, 4);

        // std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    }
    }
}