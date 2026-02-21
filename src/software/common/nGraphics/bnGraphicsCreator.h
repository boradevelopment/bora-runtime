#pragma once
#include "nGraphics/ImmediateGraphicsAbstract.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include "nGraphics/GraphicsAbstractions.h"
#ifdef WIN32
#include "nGraphics/bnGraphicsD3D11.h"
#include "nGraphics/bnGraphicsD3D12.h"
#endif
#include "bnGraphicsVK.h"
#include "bnGraphicsOGL.h"

struct GraphicsDeviceResult {
    IGraphicsDevice* device = nullptr;
    bool initialized = false;
};

inline GraphicsDeviceResult CreateGraphicsDevice(GraphicsChoice choice, SysHandle& window, IGraphicsDeviceConfig* config)
{
    GraphicsDeviceResult result;

    switch (choice)
    {
#ifdef _WIN32
    case GraphicsChoice::D3D12:
        result.device = (IGraphicsDevice*)new bnGraphicsD3D12(window, *config);
        break;
    case GraphicsChoice::D3D11:
        result.device = (IGraphicsDevice*)new bnGraphicsD3D11(window, *config);
        break;
    case GraphicsChoice::VULKAN:
        result.device = (IGraphicsDevice*)new bnGraphicsVK(window, *config);
        break;
    case GraphicsChoice::OPENGL:
        result.device = (IGraphicsDevice*)new bnGraphicsOGL(window, *config);
        break;
#endif

#ifdef __APPLE__
    case GraphicsChoice::METAL:
        result.device = new bnGraphicsMetal(window, *(bnGraphicsConfigMetal*)config);
        break;
    case GraphicsChoice::VULKAN:
        result.device = new bnGraphicsVK(window, *(bnGraphicsConfigVK*)config); // MoltenVK
        break;
    case GraphicsChoice::OPENGL:
        result.device = new bnGraphicsOpenGL(window, *(bnGraphicsConfigGL*)config);
        break;
#endif

#ifdef __ANDROID__
    case GraphicsChoice::OPENGL:
        result.device = new bnGraphicsOpenGL(window, *(bnGraphicsConfigGL*)config); // GLES
        break;
    case GraphicsChoice::VULKAN:
        result.device = new bnGraphicsVK(window, *(bnGraphicsConfigVK*)config);
        break;
#endif

#ifdef __linux__
    case GraphicsChoice::VULKAN:
        result.device = new bnGraphicsVK(window, *(bnGraphicsConfigVK*)config);
        break;
    case GraphicsChoice::OPENGL:
        result.device = new bnGraphicsOpenGL(window, *(bnGraphicsConfigGL*)config);
        break;
#endif

    default:
        result.device = nullptr;
        break;
    }

    if (result.device)
        result.initialized = result.device->Init();

    if (!result.initialized) {
        std::cout << choice << " failed\n";
    }

    return result;
}
