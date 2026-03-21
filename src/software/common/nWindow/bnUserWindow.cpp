#include "bnUserWindow.h"
#include "nGraphics/bnGraphics.h"
#include "Utils.h"
#include "bnWindowTitlebar.h"
#include "nGraphics/bnGraphicsCreator.h"
#include "bnMessageBox.h"
#include "Data.h"
#include "../../../../../global/cpp/contribs/nGUI/skia/GPUContextManager.h"
#include "nGUI/skia/win32/SkiaD3D12Renderer.h"
#include "software/common/Utils.h"

std::string GetLastErrorAsString() {
    DWORD errorCode = GetLastError();
    if (errorCode == 0)
        return "No error";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}


bool iffail(HRESULT res) {
    if (FAILED(res)) {
        std::string errorDetails = GetLastErrorAsString();
        std::cerr << "HRESULT failed: 0x" << std::hex << res
            << " | Win32 Error: " << errorDetails << std::endl;
        std::cout << "You should check call stack\n";
        __debugbreak(); // or std::abort(); to halt the program
        return true;
    }
    else return false;
}

Data* pData = nullptr;
Data* vData = nullptr;

//bnUserWindow::bnUserWindow(bnUserWindowConfig data) : configuration(data)
//{
//    if (!configuration.titleBarConfig) {
//        configuration.titleBarConfig = new bnWindowTitlebarConfig();
//    }
//
//    titleBar = std::make_unique<bnWindowTitlebar>(*this, *configuration.titleBarConfig);
//    const wchar_t* window_class_name = data.id.c_str();
//    WNDCLASSEXW window_class = { 0 };
//    window_class.cbSize = sizeof(window_class);
//    window_class.lpszClassName = window_class_name;
//    window_class.lpfnWndProc = bnUserWindow::winCallStatic;
//    window_class.style = CS_HREDRAW | CS_VREDRAW;
//    RegisterClassExW(&window_class);
//
//    CreateWindowExW(
//        WS_EX_APPWINDOW,
//        window_class_name,
//        data.title.c_str(),
//        WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
//        CW_USEDEFAULT,
//        CW_USEDEFAULT,
//        800,
//        600,
//        0,
//        0,
//        configuration.hInstance ? configuration.hInstance : 0,
//        this
//    );
//
//  
//}
//
//bnUserWindow::~bnUserWindow()
//{
//
//    FinishUpRenderThread();
//    delete graphics;
//}

void bnUserWindow::run()
{
    if (!running) return;
    while (running) {
        gDeltaTimes[handle] = GetDeltaTime();
        PollMessages();
        if (!rootDevice) continue; // wait till it exists.
        if (frameSkipped) {
            frameSkipped = false;
            continue;
        }
        if (!running) break;
        if (pauseRender) {
            if (!predraw) continue;
        }
        //UpdateUserThread();
        if (hasRecentlySwitched) {
            updateAll = true;
        }
        UpdateTitleBar();
        RenderFrame();
        if (configuration->frameLimit > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1000 / configuration->frameLimit));
    }

    rendering = false;
    canBeDeleted = true;
}


void bnUserWindow::init()
{
    if (!rootDevice) {
        InitGraphicsSystem();
    }

    guiRenderer = new SkiaD3D12Renderer(GPUContextManager::getGaneshContextForWindow(this), (bnGraphicsD3D12*)(gEngineExpl));
    StartUserThread();
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;
    ResizeSwapChain(width, height);
}


WindowCallbackCodes bnUserWindow::windowCallback(WindowEvent* event)
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
            return WindowCallbackCodes::OKAY; // means we processed it
        }
        else {
            return WindowCallbackCodes::DEFAULT;
        }
    }
        case WindowEventType::Initalize: {
#ifdef WIN32
        PROCESS_POWER_THROTTLING_STATE state = {};
        state.Version = 1;
        state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask = 0; // Disable throttling

        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));
#endif

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

        return WindowCallbackCodes::OKAY;
    }
        case WindowEventType::Activate: {
        titleBar->Update();
        return WindowCallbackCodes::DEFAULT;
    }
        case WindowEventType::NonClientHitTest: {
        i64 hit = GetDefaultProcedure(event->originalMessage, event->wordParameter, event->longParameter);
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
        int x = event->x;
        int y = event->y;

        currentInputState.mouseOffset.x = x;
        currentInputState.mouseOffset.y = y;

        UpdateInputThread();
        titleBar->OnMouseMove();
        return WindowCallbackCodes::DEFAULT;
    }
    case WindowEventType::MouseButtonDown: {
        if (event->button == 0) {
            if (event->wordParameter != HTCAPTION) {
                int button = 2; // middle mouse button index

                currentInputState.mousePressed.insert(button);
                currentInputState.mouseReleased.erase(button); // in case it was released

                POINT pt;
                pt.x = event->x;
                pt.y = event->y;
                currentInputState.mouseOffset.x = pt.x;
                currentInputState.mouseOffset.y = pt.y;

                UpdateInputThread();
            }
        }

        return WindowCallbackCodes::OKAY;
    }
        case WindowEventType::MouseButtonUp: {
        if (event->button == 0) {
        if (event->wordParameter != HTCAPTION) {

            if (frozenProcess) {
                // its frozen alright
                if (invertAnimation && !messageBoxActive) {
                    if (frozenMessageBoxThread.joinable()) {
                        frozenMessageBoxThread.join();
                    }
                    frozenMessageBoxThread = std::thread([&]() {
                        pauseRender = true;
                        messageBoxActive = true;
                        CreateBlockingMessageBox(L"bnFrozenProcessBox", L"This application has frozen", L"Close this window to continue waiting or press the close button to turn off BORA!");
                        messageBoxActive = false;
                        pauseRender = false;
                        });

                }

                invertAnimation = true;

            }

            int button = 0; // middle mouse button index

            currentInputState.mousePressed.insert(button);
            currentInputState.mouseReleased.erase(button); // in case it was released

            POINT pt;
            pt.x = event->x;
            pt.y = event->y;
            currentInputState.mouseOffset.x = pt.x;
            currentInputState.mouseOffset.y = pt.y;

            UpdateInputThread();
            }
        }

        return WindowCallbackCodes::OKAY;
    }

    case WindowEventType::NonClientMouseButtonDown: {
        if (event->button == 0 ) {
            if (titleBar->OnNCMouseButtonDown() == 0) return WindowCallbackCodes::OKAY;
            return WindowCallbackCodes::DEFAULT;
        }
    }
    case WindowEventType::NonClientMouseButtonUp: {
        if (event->button == 0 ) {
            titleBar->OnNCMouseButtonUp();
            return WindowCallbackCodes::DEFAULT;
        } else if (event->button == 1) {
            if (event->wordParameter == HTCAPTION) {
                titleBar->OnNCRMouseButtonUp(event->longParameter);
            }
            return WindowCallbackCodes::DEFAULT;
        }
    }
    case WindowEventType::SetCursor: {
        if (LOWORD(event->wordParameter) == HTCLIENT)
        {
            if (frozenProcess) {
                HCURSOR hCursor = LoadCursor(nullptr, IDC_WAIT);
                SetCursor(hCursor);
            }
            else {
                HCURSOR hCursor = LoadCursor(nullptr, IDC_ARROW);
                SetCursor(hCursor);
            }
            return WindowCallbackCodes::OKAY;
        }
        else {
            return WindowCallbackCodes::OKAY;
        }
    }
    case WindowEventType::Destroy: {
        shutdown();
        delete rootDevice;
        rootDevice = nullptr;
        PostQuitMessage(0);
        return WindowCallbackCodes::OKAY;
    }
    case WindowEventType::NonClientMouseLeave: {
        if (frozenProcess) {
            HCURSOR hCursor = LoadCursor(nullptr, IDC_WAIT);
            SetCursor(hCursor);
        }
        titleBar->OnNCMouseLeave();
        return WindowCallbackCodes::OKAY;
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
        if (event->longParameter != SIZE_MINIMIZED) {
            UINT newWidth = event->width;
            UINT newHeight = event->height;

            if (rootDevice) {
                ResizeSwapChain(newWidth, newHeight);
                UpdateViewports(newWidth, newHeight);
                updateAll = true;
                if (choice == VULKAN) {
                    gEngineExpl->WaitForNewFrame();
                }
                gDeltaTimes[handle] = GetDeltaTime();
                UpdateTitleBar();
                RenderFrame();
                updateAll = false;
                pauseRender = false;
            }
        }

        return WindowCallbackCodes::OKAY;
    }
    case WindowEventType::KeyDown: {
        int key = static_cast<int>(event->wordParameter);
        this->currentInputState.keysPressed.insert(key);
        UpdateInputThread();

        if (event->wordParameter == 'C') {
            switchGraphics(GetBestGraphicsChoice(GetFallbackChoice(choice) + 1));
        }

        if (event->wordParameter == 'D') {
            static SysButtonFlags combos[] = {
       SysButtonFlags::ALL,
       SysButtonFlags::CLOSE,
       SysButtonFlags::MINIMIZE,
       SysButtonFlags::MAXIMIZE,
       (SysButtonFlags)(SysButtonFlags::CLOSE | SysButtonFlags::MINIMIZE),
       (SysButtonFlags)(SysButtonFlags::CLOSE | SysButtonFlags::MAXIMIZE),
       (SysButtonFlags)(SysButtonFlags::MINIMIZE | SysButtonFlags::MAXIMIZE),
       (SysButtonFlags)0 // none
            };

            static int idx = 0;

            idx = (idx + 1) % std::size(combos);
            titleBar->config.sysButtons = combos[idx];

        }
        return WindowCallbackCodes::OKAY;
    }
    case WindowEventType::KeyUp: {
        int key = static_cast<int>(event->wordParameter);
        this->currentInputState.keysPressed.erase(key);
        this->currentInputState.keysReleased.insert(key);
        UpdateInputThread();
    }
    default:
        return WindowCallbackCodes::DEFAULT;
    }


}

#pragma pack(push, 1)
struct BasicVertex {
    float pos[3];
    float color[3];
};
#pragma pack(pop)

void bnUserWindow::shutdown(bool perm)
{
    if (perm && dontAffectAnything) {
        dontAffectAnything = false;
        return;
    }
    std::lock_guard<std::mutex> lock(boraThreadedLock);
    //while (updateInProgress) {
    //    if (frozenProcess) break;
    //}
    StopThreads();

    //  if (!frozenProcess) {
    userGraphics->commands.clear();
    graphics->commands.clear();
    if (!perm) {
        pauseRender = true;
        configUser.update(this, configUser.userObject, WindowUpdateMessages::ON_CHANGE_GRAPHICS, 0, 0);
        graphics->Submit();
        userGraphics->Submit();
    }
    else {
        configUser.update(this, configUser.userObject, WindowUpdateMessages::ON_RELEASE, 0, 0);
        graphics->Submit();
        userGraphics->Submit();
        running = false;
    }
    //   } else running = false;


    titleBar->ReleaseRenderVars(rootDevice);
    if (pipeline) {
        auto adv = graphics->makeAdvanced();
        adv->ReleasePipeline(pipeline);
        //pipeline->Get()->Release();
        //set->DestroyHandle();
        //pipeline->Reset();
        //pipeline->DestroyHandle();
        //pipeline = nullptr;
        //set = nullptr;
    }
    rootDevice->ReleaseBuffer(utCBO.GetAddress());
    rootDevice->ReleaseBuffer(utVB.GetAddress());
    rootDevice->ReleaseTexture(cachedUserRender);
    rootDevice->ReleaseShader(vertexShader.GetAddress());
    rootDevice->ReleaseShader(pixelShader.GetAddress());
    rootDevice->ReleaseShader(utVertexShader.GetAddress());
    rootDevice->ReleaseShader(utPixelShader.GetAddress());

    if (usingExpl) {

        (gEngineExpl)->ReleaseDescriptorPool(dPool);
        (gEngineExpl)->ReleaseDescriptorSetLayout(dSetLayout);
        (gEngineExpl)->ReleaseCommandPool(copyTexturePool);
        gEngineExpl->ReleaseDescriptorSetLayout(dSetLayoutUT);
        gEngineExpl->ReleaseDescriptorPool(dPoolUT);
        gEngineExpl->WaitTillImFree();
        gEngineExpl->ClearPendingReleases();
        //(gEngineExpl)->ClearPendingReleases();
    }
    else {
        //rootDevice->ReleaseTexture
    }

    if (!perm) {

    }

    //if (RainbowPSBlob) RainbowPSBlob->Release();
    if (perm) {
        if (configuration->logo) {
            if (configuration->logo->getState() == 1) configuration->logo->Release();
        }
        if (titleBar) titleBar->Release();
    }
    //rainbowPixelShader->Release();
    //RainbowVertexShader->Release();

    // I need to move to resourcehandles
    if (samplerState) samplerState->Release();
    samplerState.Reset();
    if (inputLayout) inputLayout->Release();
    inputLayout.Reset();
    if (blending) blending->Release();
    blending.Reset();
    if (rast) rast->Release();
    rast.Reset();
    if (depth) depth->Release();
    depth.Reset();
    GPUContextManager::shutdownGaneshContextForWindow(this);
    if (rootDevice) rootDevice->Shutdown();
    if (iTitleBar || iMain) {
        iTitleBar->Release();
        iMain->Release();
        iMain = nullptr;
        iTitleBar = nullptr;
    }

    frozenProcess = false;
    updateInProgress = false;
    frozenProcess = false;
    renderReady = false;
    // if(rootDevice) rootDevice->Shutdown();
}

inline bool bnUserWindow::InitGraphicsSystem() {
    HRESULT hr;
    int fallback = 0;
    if (choice == NONE) choice = GetBestGraphicsChoice();


    gEConfig = IGraphicsDeviceConfig{};
    gEConfig.vsync = false;
    gEConfig.enableMSAA = true;
    gEConfig.msaaSamples = 8;


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
    if (userGraphics) {
        delete userGraphics;
        userGraphics = nullptr;
    }
    graphics = new bnGraphics(this);
    userGraphics = new bnGraphics(this);


    auto advanced = graphics->makeAdvanced();

    if (usingExpl) {
        advanced->CreateDescriptorPool({
                {{ DescriptorType::CombinedImageSampler, 2}},
                2, DescriptorPoolFlags::FreeDescriptor
            }, &dPool);
        advanced->CreateDescriptorSetLayout({
                    {{0,DescriptorType::CombinedImageSampler, 1, IShaderStage::Fragment }}
            }, &dSetLayout);

        advanced->CreateDescriptorPool({
        {{ DescriptorType::CombinedImageSampler, 1}, { DescriptorType::UniformBuffer, 1}},
        1, DescriptorPoolFlags::FreeDescriptor
            }, &dPoolUT);
        advanced->CreateDescriptorSetLayout({
                    {{1, DescriptorType::CombinedImageSampler, 1, IShaderStage::Fragment }, {0, DescriptorType::UniformBuffer, 1, IShaderStage::Vertex }}
            }, &dSetLayoutUT);
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
    rastDesc.scissorEnable = false;                  // Disable unless using scissors
    rastDesc.multisampleEnable = false;                  // Enable if MSAA on
    rastDesc.antialiasedLineEnable = false;                  // Only relevant if wireframe lines

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

    auto vD = createShader(rootDevice, choice, ShaderDesc::Type::Vertex, "./titlebarvert.spv");
    auto pD = createShader(rootDevice, choice, ShaderDesc::Type::Pixel, "./titlebarfrag.spv");

    auto vDUT = createShader(rootDevice, choice, ShaderDesc::Type::Vertex, "./utshadervert.spv");
    auto pDUT = createShader(rootDevice, choice, ShaderDesc::Type::Pixel, "./utshaderfrag.spv");

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

    {
        ShaderDesc pixelShaderDesc;
        pixelShaderDesc.bytecode = pDUT.data.data();
        pixelShaderDesc.bytecodeSize = pDUT.data.size();
        pixelShaderDesc.ogData = pDUT.ogData;
        pixelShaderDesc.type = ShaderDesc::Type::Pixel;

        graphics->CreateShader(pixelShaderDesc, &utPixelShader);
    }

    {
        ShaderDesc pixelShaderDesc;
        pixelShaderDesc.bytecode = vDUT.data.data();
        pixelShaderDesc.bytecodeSize = vDUT.data.size();
        pixelShaderDesc.ogData = vDUT.ogData;
        pixelShaderDesc.type = ShaderDesc::Type::Vertex;

        graphics->CreateShader(pixelShaderDesc, &utVertexShader);
    }

    graphics->Submit();

    return true;
}

inline void bnUserWindow::ResizeSwapChain(UINT width, UINT height) {
    if (!rootDevice) return;
    // resize
    guiRenderer->PrepareForResize();
    UpdateViewports(width, height);
    rootDevice->Resize(width, height);
}

BYTE* memTitleBar;
void bnUserWindow::UpdateViewports(UINT width, UINT height)
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

int frames = 0;
IBuffer* stagingBuffer;
struct alignas(16) AlphaUB { float alpha; };
AlphaUB utAlpha;
bool newUT = true;
inline void bnUserWindow::RenderFrame() {
    if (!rootDevice) return;
    if (rendering) return;
    rendering = true;
    updateIt++;


    bool isRenderable = !IsIconic(handle);
    int userGraphicsSize = 0;
    if (!isRenderable) {
        rendering = false;
        predraw = false;
        updateInProgress = false;
        workAvailable = true;
        updateCv.notify_one();
        return;
    }

    // Get the full window size
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    POINT p;
    GetCursorPos(&p);

    bool inside = PtInRect(&clientRect, p);

    rootDevice->BeginFrame();
    auto advanced = graphics->makeAdvanced();
    auto list = advanced->GetCommandList();
    titleBar->RenderFrame(graphics, advanced, list, &inputLayout, &samplerState, &blending, &rast, &depth, &copyTexturePool, &vertexShader, &pixelShader, &iTitleBar, &dPool, &dSetLayout);
    if (hasRecentlySwitched) {
        updateAll = false;
        hasRecentlySwitched = false;
    }
    
    if (isRenderable) {

       


        renderReady = true;
        size_t grapSize = 0;
        bool tp = false;
        if (updateInProgress) {
            if (updateItNum < updateIt && !predraw) {
                frozenProcess = true; // too slow
            }
        }

        //if (updateThread.joinable()) {
        //    std::cout << "PREDRAW!\n";

      
        if (!predraw) {
            // std::cout << "not ready\n";
            if (cachedUserRender.Exists()) {
                // std::cout << "FROZEN | RENDER\n";
                if (!invertAnimation) {
                    utAlpha.alpha = 0;
                }
                else {
                    float targetAlpha = 1.0f;
                    float duration = 0.25f; // seconds
                    float deltaTime = gDeltaTimes[handle]; // or your frame time

                    // Calculate how much to increase per second
                    float alphaStep = (targetAlpha - utAlpha.alpha) * (deltaTime / duration);

                    // Update alpha
                    utAlpha.alpha += alphaStep;

                    // Clamp to target
                    if ((targetAlpha > 0.0f && utAlpha.alpha > targetAlpha) ||
                        (targetAlpha < 0.0f && utAlpha.alpha < targetAlpha))
                    {
                        utAlpha.alpha = targetAlpha;
                    }
                }

                if (!utCBO.Exists()) {
                    BufferDesc cBufferDesc;
                    cBufferDesc.type = BufferType::Constant;
                    cBufferDesc.size = sizeof(AlphaUB);
                    cBufferDesc.dynamic = true;

                    graphics->CreateBuffer(cBufferDesc, nullptr, &utCBO);
                }
                auto copybuf = advanced->BeginSingleTimeCommands(&copyTexturePool);
                advanced->CopyToBuffer(&utCBO, copybuf, &utAlpha, sizeof(AlphaUB));
                advanced->EndSingleTimeCommands(copybuf);


                if (!utVB.Exists()) {
                    Vertex quadVerts[] = {
            {-1,  1, 0, 0, 1},
            {-1, -1, 0, 0, 0},
            { 1,  1, 0, 1, 1},
            { 1, -1, 0, 1, 0},
                    };

                    BufferDesc tbBufferDesc;
                    tbBufferDesc.type = BufferType::Vertex;
                    tbBufferDesc.size = sizeof(quadVerts);
                    tbBufferDesc.stride = sizeof(Vertex);
                    tbBufferDesc.dynamic = false;



                    graphics->CreateBuffer(tbBufferDesc, quadVerts, &utVB);
                }

                if (!pipeline || !pipeline->Exists() || !set->Exists()) {
                    auto builderObj = advanced->CreatePipelineBuilder();

                    advanced->BuilderAddShader(builderObj, &utPixelShader);
                    advanced->BuilderAddShader(builderObj, &utVertexShader);
                    advanced->BuilderSetBlendState(builderObj, &blending);
                    advanced->BuilderSetInputLayout(builderObj, &inputLayout);
                    advanced->BuilderSetRasterizer(builderObj, &rast);
                    advanced->BuilderSetDepthStencil(builderObj, &depth);
                    advanced->BuilderSetDescriptorPool(builderObj, &dPoolUT);
                    advanced->BuilderSetDescriptorSetLayout(builderObj, &dSetLayoutUT);

                    pipeline = advanced->CreatePipeline(builderObj);
                    set = advanced->CreateDescriptorSet(pipeline);

                }

                advanced->SetDescriptorSetTexture(set, &cachedUserRender, 1);
                advanced->SetDescriptorSetSamplerState(set, &samplerState, 1);
                advanced->SetDescriptorSetBuffer(set, &utCBO);
                advanced->UpdateDescriptorSet(set);
                if(newUT){
                    newUT = false;
                }

                list->BindViewPort(&iMain);
                list->BindScissor(&iMain);
                list->BindPipeline(pipeline);
                list->BindDescriptorSet(set);
                list->BindBuffer(&utVB);
                list->Draw(PrimitiveType::TriangleStrip, 4);
                advanced->ReleaseCommandList(list);
                //graphics->Submit();
            }

            
        }
        else {
            tp = true;
            updateStart = std::chrono::steady_clock::now();
            DestroyMessageBox(L"bnFrozenProcessBox");
            frozenProcess = false;
            updateInProgress = false;
            frozenProcess = false;
            renderReady = false;
            newUT = true;
            //advanced->ReleaseCommandList(list);
            renderReady = false;
            userGraphicsSize = userGraphics->commands.size();
            list->BindViewPort(&iMain);
            list->BindScissor(&iMain);
            advanced->ReleaseCommandList(list);
            graphics->Submit();
            userGraphics->Submit();
        }
    }
    else {
        updateStart = std::chrono::steady_clock::now();
        //predraw = false;
        frozenProcess = false;
        updateInProgress = false;
        frozenProcess = false;
        renderReady = false;
        // justRendered = true;
        utAlpha.alpha = 0;
    }


    graphics->Submit();
    rootDevice->EndFrame();
    guiRenderer->beginFrame(1920, 1080);
    guiRenderer->Clear();
    guiRenderer->endFrame();

    if (predraw) {
        workAvailable = true;
        predraw = false;
        updateInProgress = false;
        invertAnimation = false;
        utAlpha.alpha = 0;
        swpText.Resolve(rootDevice->GetSwapchainImage());

        advanced = graphics->makeAdvanced();

        if (cachedUserRender.Exists()) {
            cachedUserRender.MoveAsPrevious();
            rootDevice->ReleaseTexture(cachedUserRender.GetPreviousResource()); // pushes to GC, old gets nulled
        }
        scrready = false;

        if (userGraphicsSize > 0) {
            int signedHeight = vpMain.height - vpMain.y;
            TextureDesc tbDesc;
            tbDesc.format = swpText->desc.format;
            tbDesc.width = vpMain.width;
            tbDesc.height = signedHeight < 0 ? vpMain.height : signedHeight;
            tbDesc.widthBytes = 0;
            tbDesc.CpuAccessWrite = true;
            tbDesc.Dynamic = true;

            graphics->CreateTexture(tbDesc, nullptr, &cachedUserRender);
            ImageCopyDesc ccDesc{};
            ccDesc.srcSubresource.aspectMask = ImageAspectFlagBits::IMAGE_ASPECT_COLOR_BIT;
            ccDesc.srcSubresource.layerCount = 1;
            ccDesc.dstSubresource.aspectMask = ImageAspectFlagBits::IMAGE_ASPECT_COLOR_BIT;
            ccDesc.dstSubresource.layerCount = 1;
            ccDesc.srcOffset.y = vpMain.y;
            ccDesc.extent.width = vpMain.width;
            ccDesc.extent.height = vpMain.height - vpMain.y;
            ccDesc.extent.depth = 1;

            auto copyCmd = advanced->BeginSingleTimeCommands(&copyTexturePool);
            advanced->CopyImageToImage(copyCmd, &swpText, &cachedUserRender, ccDesc);
            advanced->EndSingleTimeCommands(copyCmd);
            graphics->ReleaseTexture(&swpText);
            graphics->Submit();
        }
        updateCv.notify_one();
    }
    rootDevice->Present();
    guiRenderer->DestroyFrameResources();


    //if (!updateInProgress && !frozenProcess) {
    //    if (choice == VULKAN) {
    //        auto vkGraph = (bnGraphicsVK*)(gEngineExpl);
    //        vkGraph->dontClearYet = false;
    //    }
    //} 

    rendering = false;
    firstFrame = false;



    //if (updateIt < gEConfig.framesInFlight) {
    //    firstFrames[updateIt] = true;
    //}
}




//void bnUserWindow::FinishUpRenderThread()
//{
// 
//}

void bnUserWindow::UpdateTitleBar()
{
    titleBar->Update();
}

//void bnUserWindow::UpdateUserThread()
//{
//    bool isRenderable = !IsIconic(handle);
//    bool e = updateThread.joinable();
//
//
//    if (!updateInProgress && !predraw) {
//        userGraphics->commands.clear();
//
//        updateStart = std::chrono::steady_clock::now();
//        updateItNum = updateIt;
//        if (updateThread.joinable()) {
//            updateThread.join();
//        }
//
//
//        auto advanced = userGraphics->makeAdvanced();
//        auto list = advanced->GetCommandList();
//
//        list->BindViewPort(&iMain);
//        list->BindScissor(&iMain);
//        advanced->ReleaseCommandList(list);
//
//        updateThread = std::thread([=]() {
//            configUser.update(this, WindowUpdateMessages::ON_UPDATE, isRenderable, 0);
//            predraw = true;
//            frozenProcess = false;
//         });
//
//        updateInProgress = true;
//
//        std::wstring thrName = L"BORA User Update Thread " + std::to_wstring(updateIt);
//
//        SetThreadDescription(updateThread.native_handle(), thrName.c_str());
//    }
//}

void bnUserWindow::preDestroyFunctions()
{
    StopThreads();
}

void bnUserWindow::UpdateInputThread()
{
    configUser.update(this, configUser.userObject, ON_INPUT_CHANGED, 0, reinterpret_cast<intptr_t>(&currentInputState));
}

void bnUserWindow::StartUserThread()
{
    stopThread = false;
    updateThread = std::thread([this]() { UserThreadLoop(); });
    SetThreadDescription(updateThread.native_handle(), L"BORA User Update Thread");
}

void bnUserWindow::StopThreads()
{
    {
        std::lock_guard<std::mutex> lock(updateMutex);
        stopThread = true;
        workAvailable = true; // wake the thread so it exits
    }
    updateCv.notify_one();

    while (updateInProgress) {
        if (predraw) break;
        if (frozenProcess) {

#ifdef WIN32
            TerminateThread(updateThread.native_handle(), 1);
#elif defined(__linux__)
            pthread_cancel(updateThread.native_handle());
#endif
            break;
        }

        if (!updateThread.joinable()) {
            break;
        }
    }

    if (messageBoxActive) {
#ifdef WIN32
        TerminateThread(frozenMessageBoxThread.native_handle(), 1);
#elif defined(__linux__)
        pthread_cancel(frozenMessageBoxThread.native_handle());
#endif
    }

    if (frozenMessageBoxThread.joinable()) {
        frozenMessageBoxThread.join();
    }

    if (updateThread.joinable()) {
        updateThread.join();
    }

    updateInProgress = false;

    if (updateThread.joinable())
        updateThread.join();
}

void bnUserWindow::UserThreadLoop()
{
    while (true) {
        std::unique_lock<std::mutex> lock(updateMutex);
        updateCv.wait(lock, [this]() { return workAvailable || stopThread; });

        if (stopThread)
            break;

        workAvailable = false;
        bool isRenderable = !IsIconic(handle);
        lock.unlock(); // unlock before doing work

        // -------------------
        // Begin user update
        // -------------------
        userGraphics->commands.clear();

        updateStart = std::chrono::steady_clock::now();
        updateItNum = updateIt;

        updateInProgress = true;
        predraw = false;
        if (isWASMInitalized) {
            if (!wasmUpdate) continue;
        WasmTools::CallByIndex<void>(WasmTools::getLastExecutionEnvironment(), (u32)wasmUpdate, WasmTools::toWASM(WasmTools::getLastExecutionEnvironment(), (void*)wasmID), (u64)nullptr, WasmTools::toWASM(WasmTools::getLastExecutionEnvironment(), (void*)WindowUpdateMessages::ON_UPDATE), WasmTools::toWASM(WasmTools::getLastExecutionEnvironment(), (void*)isRenderable), (u64)0);
        } else configUser.update(this, configUser.userObject, WindowUpdateMessages::ON_UPDATE, isRenderable, 0);
        predraw = true;
        frozenProcess = false;
        updateInProgress = false;
    }
}
