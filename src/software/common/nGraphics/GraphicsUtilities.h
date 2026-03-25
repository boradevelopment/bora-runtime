#pragma once
#include "nGraphics/GraphicsAbstractions.h"
#if  __has_include("GLEW/include/glew.h") && defined(_WIN32)
#include "GLEW/include/glew.h"
#include "GLEW/include/wglew.h"
#define WIN32OGL
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#include "vulkan/vulkan.h"
#include <unordered_set>


inline VkImageLayout ToVkImageLayout(ImageLayout layout)
{
    switch (layout)
    {
    case ImageLayout::Undefined:        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageLayout::RenderTarget:     return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageLayout::DepthStencil:     return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageLayout::ShaderRead:       return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageLayout::Present:          return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ImageLayout::CopySrc:          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageLayout::CopyDst:          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:                            return VK_IMAGE_LAYOUT_GENERAL; // fallback
    }
}

inline VkAccessFlags ToVkAccessFlags(ImageAccessLayout layout)
{
    VkAccessFlags flags = 0;

    if ((layout & ImageAccessLayout::Read) != ImageAccessLayout::None)
        flags |= VK_ACCESS_SHADER_READ_BIT;
    if ((layout & ImageAccessLayout::Write) != ImageAccessLayout::None)
        flags |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if ((layout & ImageAccessLayout::DepthRead) != ImageAccessLayout::None)
        flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if ((layout & ImageAccessLayout::DepthWrite) != ImageAccessLayout::None)
        flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if ((layout & ImageAccessLayout::CopySrc) != ImageAccessLayout::None)
        flags |= VK_ACCESS_TRANSFER_READ_BIT;
    if ((layout & ImageAccessLayout::CopyDst) != ImageAccessLayout::None)
        flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
    if ((layout & ImageAccessLayout::Present) != ImageAccessLayout::None)
        flags |= 0; // VK doesn�t require access flags for presentation

    return flags;
}

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

struct GLTextureFormat {
    GLenum internalFormat;
    GLenum format;
    GLenum type;
};

inline GLTextureFormat ToGLFormat(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::RGBA8_UNorm:         return { GL_RGBA8,    GL_RGBA, GL_UNSIGNED_BYTE };
        case TextureFormat::RGBA8_UNorm_SRGB:   return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE };
        case TextureFormat::BGRA8_UNorm:        return { GL_RGBA8,    GL_BGRA, GL_UNSIGNED_BYTE };
        case TextureFormat::BGRA8_UNorm_SRGB:   return { GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE };
        case TextureFormat::RGBA16_Float:       return { GL_RGBA16F,  GL_RGBA, GL_HALF_FLOAT };
        case TextureFormat::R16_Float:          return { GL_R16F,     GL_RED,  GL_HALF_FLOAT };
        case TextureFormat::R16_UNorm:          return { GL_R16,      GL_RED,  GL_UNSIGNED_SHORT };
        case TextureFormat::R32_Float:          return { GL_R32F,     GL_RED,  GL_FLOAT };
        case TextureFormat::RG32_Float:         return { GL_RG32F,    GL_RG,   GL_FLOAT };
        case TextureFormat::RGBA32_Float:       return { GL_RGBA32F,  GL_RGBA, GL_FLOAT };
        case TextureFormat::D24_UNorm_S8_UInt:  return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 };
        case TextureFormat::D32_Float:          return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
        case TextureFormat::D32_Float_S8X24_UInt:return { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV };
        default: return { 0, 0, 0 };
    }
}

inline GLenum ShaderTypeToGL(ShaderDesc::Type type)
{
    switch (type)
    {
        case ShaderDesc::Type::Vertex:       return GL_VERTEX_SHADER;
        case ShaderDesc::Type::Pixel:        return GL_FRAGMENT_SHADER;
        case ShaderDesc::Type::Geometry:     return GL_GEOMETRY_SHADER;
        case ShaderDesc::Type::TessControl:  return GL_TESS_CONTROL_SHADER;
        case ShaderDesc::Type::TessEval:     return GL_TESS_EVALUATION_SHADER;
#ifndef __APPLE__
        case ShaderDesc::Type::Compute: return GL_COMPUTE_SHADER;
#else
        case ShaderDesc::Type::Compute: return 0;
#endif
        default:
            return 0; // Unsupported in standard OpenGL
    }
}

inline bool IsGLExtensionSupported(const char* extName)
{
    static std::unordered_set<std::string> extensions;

    // first call, populate the set
    if (extensions.empty())
    {
        GLint nExtensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &nExtensions);

        for (GLint i = 0; i < nExtensions; ++i)
        {
            const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (ext)
                extensions.insert(ext);
        }
    }

    return extensions.find(extName) != extensions.end();
}

inline GLenum ToGLBufferType(BufferType type)
{
    switch (type)
    {
        case BufferType::Vertex:   return GL_ARRAY_BUFFER;              // Vertex buffer
        case BufferType::Index:    return GL_ELEMENT_ARRAY_BUFFER;      // Index buffer
        case BufferType::Constant: return GL_UNIFORM_BUFFER;            // Constant/uniform buffer
        case BufferType::Staging:  return GL_COPY_WRITE_BUFFER;         // Temporary CPU->GPU buffer
#ifndef __APPLE__
        case BufferType::Storage:  return GL_SHADER_STORAGE_BUFFER;
#else
        case BufferType::Storage:
            // macOS OGL doesn't support SSBOs, i'll just fallback to UBOS.
            // Seriously though, just use any modern library if you're using bora, its supported.
            return GL_UNIFORM_BUFFER;
#endif
        default: return 0;
    }
}

struct GLAttribFormat {
    GLint size;      // number of components
    GLenum type;     // GL_FLOAT, GL_UNSIGNED_INT, etc
    GLboolean normalized;
};

inline GLAttribFormat ToGLAttrib(VertexAttribType type)
{
    switch (type)
    {
        case VertexAttribType::Float:  return { 1, GL_FLOAT, GL_FALSE };
        case VertexAttribType::Float2: return { 2, GL_FLOAT, GL_FALSE };
        case VertexAttribType::Float3: return { 3, GL_FLOAT, GL_FALSE };
        case VertexAttribType::Float4: return { 4, GL_FLOAT, GL_FALSE };
        case VertexAttribType::UInt:   return { 1, GL_UNSIGNED_INT, GL_FALSE };
        case VertexAttribType::UInt2:  return { 2, GL_UNSIGNED_INT, GL_FALSE };
        case VertexAttribType::UInt3:  return { 3, GL_UNSIGNED_INT, GL_FALSE };
        case VertexAttribType::UInt4:  return { 4, GL_UNSIGNED_INT, GL_FALSE };
        default: return { 0, 0, GL_FALSE };
    }
}

inline GLenum ToGLFilter(TextureFilter filter)
{
    switch (filter)
    {
        case TextureFilter::Nearest:      return GL_NEAREST;
        case TextureFilter::Linear:       return GL_LINEAR;
        case TextureFilter::Anisotropic:  return GL_LINEAR; // OpenGL uses maxAnisotropy separately
        default: return GL_LINEAR;
    }
}

inline GLenum ToGLAddressMode(TextureAddressMode mode)
{
    switch (mode)
    {
        case TextureAddressMode::Wrap:    return GL_REPEAT;
        case TextureAddressMode::Mirror:  return GL_MIRRORED_REPEAT;
        case TextureAddressMode::Clamp:   return GL_CLAMP_TO_EDGE;
        case TextureAddressMode::Border:  return GL_CLAMP_TO_BORDER;
        default: return GL_REPEAT;
    }
}

inline GLenum ToGLCompareFunc(ComparisonFunc func)
{
    switch (func)
    {
        case ComparisonFunc::Never:       return GL_NEVER;
        case ComparisonFunc::Less:        return GL_LESS;
        case ComparisonFunc::Equal:       return GL_EQUAL;
        case ComparisonFunc::LessEqual:   return GL_LEQUAL;
        case ComparisonFunc::Greater:     return GL_GREATER;
        case ComparisonFunc::NotEqual:    return GL_NOTEQUAL;
        case ComparisonFunc::GreaterEqual:return GL_GEQUAL;
        case ComparisonFunc::Always:      return GL_ALWAYS;
        default: return GL_NEVER;
    }
}

inline GLenum ToGLStencilOp(StencilOp op)
{
    switch (op)
    {
        case StencilOp::Keep:    return GL_KEEP;
        case StencilOp::Zero:    return GL_ZERO;
        case StencilOp::Replace: return GL_REPLACE;
        case StencilOp::IncrSat: return GL_INCR;      // OpenGL clamps by default
        case StencilOp::DecrSat: return GL_DECR;      // OpenGL clamps by default
        case StencilOp::Invert:  return GL_INVERT;
        case StencilOp::Incr:    return GL_INCR_WRAP;
        case StencilOp::Decr:    return GL_DECR_WRAP;
        default: return GL_KEEP;
    }
}

inline GLenum ToGLBlend(Blend blend)
{
    switch (blend)
    {
        case Blend::Zero:            return GL_ZERO;
        case Blend::One:             return GL_ONE;
        case Blend::SrcColor:        return GL_SRC_COLOR;
        case Blend::InvSrcColor:     return GL_ONE_MINUS_SRC_COLOR;
        case Blend::SrcAlpha:        return GL_SRC_ALPHA;
        case Blend::InvSrcAlpha:     return GL_ONE_MINUS_SRC_ALPHA;
        case Blend::DestAlpha:       return GL_DST_ALPHA;
        case Blend::InvDestAlpha:    return GL_ONE_MINUS_DST_ALPHA;
        case Blend::DestColor:       return GL_DST_COLOR;
        case Blend::InvDestColor:    return GL_ONE_MINUS_DST_COLOR;
        default: return GL_ONE;
    }
}

inline GLenum ToGLBlendOp(BlendOp op)
{
    switch (op)
    {
        case BlendOp::Add:            return GL_FUNC_ADD;
        case BlendOp::Subtract:       return GL_FUNC_SUBTRACT;
        case BlendOp::RevSubtract:    return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:            return GL_MIN;
        case BlendOp::Max:            return GL_MAX;
        default: return GL_FUNC_ADD;
    }
}

inline GLenum ToGLPrimitiveType(PrimitiveType type) {
    GLenum glMode = GL_TRIANGLES; // default
    switch (type) {
        case PrimitiveType::Points:    glMode = GL_POINTS; break;
        case PrimitiveType::Lines:     glMode = GL_LINES; break;
        case PrimitiveType::LineStrip:glMode = GL_LINE_STRIP; break;
        case PrimitiveType::Triangles: glMode = GL_TRIANGLES; break;
        case PrimitiveType::TriangleStrip: glMode = GL_TRIANGLE_STRIP; break;
    }

    return glMode;
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
        sdd = createShaderSPIRVToD3D(dataD);
#endif
    }
    delete dataD;
    return sdd;
}