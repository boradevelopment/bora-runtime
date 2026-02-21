#pragma once
#include "bnWindow.h"
#include "bskia/include/core/SkCanvas.h"
#include "bskia/include/core/SkPaint.h"
#include "bskia/include/core/SkSurface.h"
#include "bskia/include/gpu/ganesh/GrDirectContext.h"
#include "bskia/include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "bskia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "bskia/include/gpu/vk/VulkanBackendContext.h"
#include "bskia/include/gpu/vk/VulkanExtensions.h"
#include "bskia/include/gpu/ganesh/vk/GrVkTypes.h"
#include "bskia/include/gpu/ganesh/GrBackendSurface.h"
#include "software/common/Utils.h"

struct bnMessageBoxConfig : bnWindowConstructorStruct {
    const wchar_t* message = L"";
    const wchar_t* description = L"";

    bnMessageBoxConfig(const wchar_t* message, const wchar_t* description) : message(message), description(description) {
        titleBarConfig = new bnWindowTitlebarConfig();
        titleBarConfig->backgroundColor = { 0,0,0 };
        titleBarConfig->borderColor = { 75,75,75 };
        titleBarConfig->properties = TitleBarProperties::SHOWN;
        titleBarConfig->sysButtons = SysButtonFlags::MINIMIZE | SysButtonFlags::CLOSE;
        logo = new Data(R"(C:\Users\isloe\Downloads\bnError.png)");
        hInstance = nullptr;
        id = sWstring(L"BNM") + title;
        title = sWstring(message);
        clearColor = { 0,0,0,1 };
        

    }
};

struct PCMChunk {
    const void* data;
    size_t size;
    int sampleRate;
    int channels;
    int bitsPerSample;
};

inline PCMChunk GetPCMData(const std::vector<uint8_t>& wavBytes) {
    if (wavBytes.size() < 12) return {}; // too small

    // Verify RIFF + WAVE
    if (std::memcmp(&wavBytes[0], "RIFF", 4) != 0) return {};
    if (std::memcmp(&wavBytes[8], "WAVE", 4) != 0) return {};

    size_t i = 12; // start after RIFF header
    PCMChunk chunk{};

    while (i + 8 <= wavBytes.size()) {
        char chunkId[5] = { 0 };
        std::memcpy(chunkId, &wavBytes[i], 4);

        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, &wavBytes[i + 4], 4);

        size_t next = i + 8 + chunkSize;
        if (next > wavBytes.size()) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
            // Extract WAV format
            const uint8_t* fmtData = &wavBytes[i + 8];
            chunk.channels = *(uint16_t*)(fmtData + 2);
            chunk.sampleRate = *(uint32_t*)(fmtData + 4);
            chunk.bitsPerSample = *(uint16_t*)(fmtData + 14);
        }
        else if (std::strncmp(chunkId, "data", 4) == 0) {
            chunk.data = &wavBytes[i + 8];
            chunk.size = chunkSize;
        }

        i = next + (chunkSize % 2); // ensure even alignment
    }

    // Only return chunk if we found both fmt and data
    if (chunk.data && chunk.sampleRate && chunk.channels && chunk.bitsPerSample)
        return chunk;

    return {};
}

class bnMessageBox : public bnWindow
{
private:
    bnMessageBoxConfig config;
public:
    bnMessageBox(bnMessageBoxConfig&& data) : config(std::move(data)), bnWindow(&config) {
        if (!configuration->titleBarConfig) {
            configuration->titleBarConfig = new bnWindowTitlebarConfig();
        }

        auto titleBarCOnfig = configuration->titleBarConfig;

        titleBar = std::make_unique<bnWindowTitlebar>(*this, *titleBarCOnfig);

        descriptionFont = fontMgr.getFont("Segoe UI", 18);

        pauseRender = true;
        choice = GetBestGraphicsChoice(0, false);

        u16msg = wstring_to_utf16(config.message);

        u16dc = wstring_to_utf16(config.description);
        
        //SkRect bounds;
        SkRect boundsMsg;
        auto msgWidth = (int)ceil(titleBar->titleFont.measureText(u16msg.c_str(), u16msg.size() * sizeof(char16_t), SkTextEncoding::kUTF16, &boundsMsg));

        float maxTextWidth = 400.0f; // wrap width limit
        wrappedLines = wrapText(u16dc, titleBar->titleFont, maxTextWidth);

        // Compute total height
        float lineHeight = titleBar->titleFont.getSpacing();
        float totalHeight = wrappedLines.size() * lineHeight;

        // Compute widest line
        float maxLineWidth = 0;
        SkRect tmp;
        for (auto& line : wrappedLines) {
            float w = titleBar->titleFont.measureText(line.c_str(), line.size() * sizeof(char16_t), SkTextEncoding::kUTF16, &tmp);
            maxLineWidth = std::max(maxLineWidth, w);
        }

        // Then use maxLineWidth and totalHeight for predicted bounds
        int predictedWidth = (int)ceil(maxLineWidth) + boundsMsg.height() + 125;
        int predictedHeight = (int)ceil(totalHeight) + boundsMsg.width() + 45;

        config.title = config.message;

        config.height = predictedHeight;
        config.width = predictedWidth;

        // Get the width and height from the texts themselves [rough estimate]
        // the description is the only one in render next to a logo
        // i'll need to implement the image code, i could reuse the skia image code for the titlebar logo.

        create(this, true);
        if (handles.size() >= 2) {
            isChild = true;
            parentHandle = handles[handles.size() - 2];
            // SetWindowLongPtr(handle, GWLP_HWNDPARENT, (LONG_PTR)parentHandle);
            config.titleBarConfig->sysButtons = SysButtonFlags::CLOSE;
        }
        init();
        ShowWindow(handle, SW_SHOW);

        pauseRender = false;
    }
    void run() override;
    void init() override;
    void preDestroyFunctions() override;
    void shutdown(bool perm = true) override;
public: // Functions & Structs
    WindowCallbackCodes windowCallback(WindowEvent* event) override;
    bool InitGraphicsSystem();
    void RenderFrame();
    void ResizeSwapChain(UINT width, UINT height);
    void UpdateViewports(UINT width, UINT height);
private:
    ResourceHandle<IBuffer> stagingBuffer2;
    ResourceHandle<ICommandPool> copyTexturePool;
    ResourceHandle<IShader> vertexShader;
    ResourceHandle<IShader> pixelShader;
    ResourceHandle<IInputLayout> inputLayout;
    ResourceHandle<ISamplerState> samplerState;
    ResourceHandle<IBlendState> blending;
    ResourceHandle<IRasterizerState> rast;
    ResourceHandle<IDepthStencilState> depth;
    BITMAP titleBarBit;
    bool titleUpdated = false;
    std::mutex boraThreadedLock;
    ViewPort vpTitlebar;
    ViewPort vpMain;
    ResourceHandle<IViewPort> iTitleBar;
    ResourceHandle<IViewPort> iMain;
    std::vector<uint8_t> pixels;
    SkFont descriptionFont;
    std::u16string u16msg;
    std::u16string u16dc;
    ResourceHandle<ITexture> mainTexture;
    ResourceHandle<IBuffer> mainVertice;
    std::vector<std::u16string> wrappedLines;
    ResourceHandle<IDescriptorPool> dPool;
    ResourceHandle<IDescriptorSetLayout> dSetLayout;
    ResourceHandle<IPipeline>* pipeline = nullptr;
    ResourceHandle<IDescriptorSet>* set = nullptr;
    AudioStream* stream = nullptr;
    bool flickering = false;
    SysHandle parentHandle;
    bool isChild = false;
    int flickerFramesRemaining = 0; // how many frames left to flicker
    int flickerFrameCounter = 0;    // frame counter for toggling
    ResourceHandle<void> bufferData;
    bnFontMgr fontMgr;
};

inline sMap<const wchar_t*, std::unique_ptr<bnMessageBox>> currentMessageBoxes;
inline void CreateBlockingMessageBox(const wchar_t* id, const wchar_t* title, const wchar_t* description) {
    auto& box = currentMessageBoxes[id];
    box = std::make_unique<bnMessageBox>(
        bnMessageBoxConfig(title, description)
    );

    box->run();
}


inline void DestroyMessageBox(const wchar_t* id) {
    auto it = currentMessageBoxes.find(id);
    if (it == currentMessageBoxes.end())
        return;

    it->second->close();
    currentMessageBoxes.erase(it);
}

