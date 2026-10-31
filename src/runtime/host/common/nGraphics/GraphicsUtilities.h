#pragma once
#include "nGraphics/GraphicsAbstractions.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
#if  __has_include("GLEW/include/glew.h") && defined(_WIN32)
#include "GLEW/include/glew.h"
#include "GLEW/include/wglew.h"
#define WIN32OGL
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif
#include "VK/VKGraphicUtilities.h"

inline bool IsSPIRV(const void* data, size_t size)
{
    if (!data || size < 4) return false;
    const uint32_t* header = reinterpret_cast<const uint32_t*>(data);
    return *header == 0x07230203;
}

inline bool IsMetalBinary(const void* data, size_t size) {
    if (size < 4) return false;
    const char* bytes = reinterpret_cast<const char*>(data);
    // Metal Library Magic Number is 'M' 'T' 'L' 'B'
    return bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'L' && bytes[3] == 'B';
}

#include "3rdparty/bspirv/spirv_hlsl.hpp"
#include "Data.h"
#ifdef WIN32
#include "nGraphics/D3DUtilities.h"
#endif

inline ShaderData createShader(IGraphicsDevice* rootDevice, GraphicsChoice choice, ShaderDesc::Type desc, std::string path) {
    ShaderData sdd{};
    auto dataD = new Data(path);
    bool test = rootDevice->IsFeatureSupported("SPIRV");
    if (choice == VULKAN || choice == OPENGL && test) { // SPIR-V ALREADY
        sdd.data = dataD->getData();
        sdd.ogData = dataD->getData();
    }
    else if (choice == OPENGL) {
        auto data = dataD->getData();
        size_t intCount = data.size() / 4;

        std::vector<uint32_t> intVec(intCount);

        for (size_t i = 0; i < intCount; ++i) {
            intVec[i] = static_cast<uint32_t>(data[i * 4]) |
                (static_cast<uint32_t>(data[i * 4 + 1]) << 8) |
                (static_cast<uint32_t>(data[i * 4 + 2]) << 16) |
                (static_cast<uint32_t>(data[i * 4 + 3]) << 24);
        }
        data.clear();

        spirv_cross::CompilerGLSL compiler(intVec);
        spirv_cross::CompilerGLSL::Options options;
        std::string hlsl_source = compiler.compile();
        intVec.clear();
        sdd.ogData = dataD->getData();
        sdd.data = sVec<u8>(hlsl_source.size());
        memcpy(sdd.data.data(), hlsl_source.c_str(), hlsl_source.size());
    }
    else if (choice == D3D11 || choice == D3D12) {
#ifdef WIN32
        sdd = createShaderSPIRVToD3D(dataD, desc);
#endif
    }
    delete dataD;
    return sdd;
}

