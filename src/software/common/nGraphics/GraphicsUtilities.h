#pragma once
#include "nGraphics/GraphicsAbstractions.h"
#if  __has_include("GLEW/include/glew.h") && defined(_WIN32)
#include "GLEW/include/glew.h"
#include "GLEW/include/wglew.h"
#define WIN32OGL
#endif
#ifdef WIN32
#include "dxgi.h"
#include "nGraphics/D3dx12.h"
#include "d3d11.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
inline DXGI_FORMAT ToDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::Unknown:               return DXGI_FORMAT_UNKNOWN;

        // 8-bit per channel
    case TextureFormat::RGBA8_UNorm:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_UNorm_SRGB:      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::BGRA8_UNorm:           return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8_UNorm_SRGB:      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        // 16-bit float
    case TextureFormat::RGBA16_Float:          return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R16_Float:             return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::R16_UNorm:             return DXGI_FORMAT_R16_UNORM;

        // 32-bit float
    case TextureFormat::R32_Float:             return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::RG32_Float:            return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGBA32_Float:          return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // Depth / Stencil
    case TextureFormat::D24_UNorm_S8_UInt:     return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32_Float:             return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::D32_Float_S8X24_UInt:  return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    default: return DXGI_FORMAT_UNKNOWN;
    }
}


inline D3D12_PRIMITIVE_TOPOLOGY ConvertPrimitive(PrimitiveType type) {
    switch (type) {
    case PrimitiveType::Triangles: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveType::Lines: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveType::Points: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case PrimitiveType::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    default: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

inline ImageLayout ToImageLayout(D3D12_RESOURCE_STATES state)
{
    if (state & D3D12_RESOURCE_STATE_COMMON)         return ImageLayout::GenericRead;
    if (state & D3D12_RESOURCE_STATE_GENERIC_READ)   return ImageLayout::GenericRead;
    if (state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ||
        state & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        return ImageLayout::ShaderRead;
    if (state & D3D12_RESOURCE_STATE_RENDER_TARGET)  return ImageLayout::RenderTarget;
    if (state & D3D12_RESOURCE_STATE_DEPTH_WRITE ||
        state & D3D12_RESOURCE_STATE_DEPTH_READ)    return ImageLayout::DepthStencil;
    if (state & D3D12_RESOURCE_STATE_PRESENT)       return ImageLayout::Present;
    if (state & D3D12_RESOURCE_STATE_COPY_SOURCE)   return ImageLayout::CopySrc;
    if (state & D3D12_RESOURCE_STATE_COPY_DEST)     return ImageLayout::CopyDst;

    return ImageLayout::Undefined; // fallback
}


inline D3D12_FILTER ToD3D12Filter(TextureFilter min, TextureFilter mag, TextureFilter mip, uint32_t maxAniso) {
    if (maxAniso > 1) return D3D12_FILTER_ANISOTROPIC;

    auto minMap = (min == TextureFilter::Nearest) ? D3D12_FILTER_TYPE_POINT : D3D12_FILTER_TYPE_LINEAR;
    auto magMap = (mag == TextureFilter::Nearest) ? D3D12_FILTER_TYPE_POINT : D3D12_FILTER_TYPE_LINEAR;
    auto mipMap = (mip == TextureFilter::Nearest) ? D3D12_FILTER_TYPE_POINT : D3D12_FILTER_TYPE_LINEAR;

    return D3D12_ENCODE_BASIC_FILTER(minMap, magMap, mipMap, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
}

inline D3D12_TEXTURE_ADDRESS_MODE ToD3D12Address(TextureAddressMode mode) {
    switch (mode) {
    case TextureAddressMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case TextureAddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case TextureAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case TextureAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

inline D3D12_COMPARISON_FUNC ToD3D12Comparison(ComparisonFunc func) {
    switch (func) {
    case ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
    case ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
    case ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
    case ComparisonFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
    case ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
    default: return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

inline DXGI_FORMAT ToDXGIFormat(VertexAttribType type) {
    switch (type) {
    case VertexAttribType::Float:   return DXGI_FORMAT_R32_FLOAT;
    case VertexAttribType::Float2:  return DXGI_FORMAT_R32G32_FLOAT;
    case VertexAttribType::Float3:  return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexAttribType::Float4:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexAttribType::UInt:    return DXGI_FORMAT_R32_UINT;
    case VertexAttribType::UInt2:   return DXGI_FORMAT_R32G32_UINT;
    case VertexAttribType::UInt3:   return DXGI_FORMAT_R32G32B32_UINT;
    case VertexAttribType::UInt4:   return DXGI_FORMAT_R32G32B32A32_UINT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

inline TextureFormat FromDXGIFormat(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:       return TextureFormat::RGBA8_UNorm;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  return TextureFormat::RGBA8_UNorm_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:       return TextureFormat::BGRA8_UNorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  return TextureFormat::BGRA8_UNorm_SRGB;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:   return TextureFormat::RGBA16_Float;
    case DXGI_FORMAT_R16_FLOAT:            return TextureFormat::R16_Float;
    case DXGI_FORMAT_R16_UNORM:            return TextureFormat::R16_UNorm;

    case DXGI_FORMAT_R32_FLOAT:            return TextureFormat::R32_Float;
    case DXGI_FORMAT_R32G32_FLOAT:         return TextureFormat::RG32_Float;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:   return TextureFormat::RGBA32_Float;

    case DXGI_FORMAT_D24_UNORM_S8_UINT:    return TextureFormat::D24_UNorm_S8_UInt;
    case DXGI_FORMAT_D32_FLOAT:            return TextureFormat::D32_Float;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return TextureFormat::D32_Float_S8X24_UInt;

    default: return TextureFormat::Unknown;
    }
}

// D3D11

inline D3D11_FILTER ToD3D11Filter(TextureFilter minFilter, TextureFilter magFilter, TextureFilter mipFilter, uint32_t maxAnisotropy)
{
    if (minFilter == TextureFilter::Anisotropic ||
        magFilter == TextureFilter::Anisotropic ||
        mipFilter == TextureFilter::Anisotropic)
    {
        return D3D11_ENCODE_ANISOTROPIC_FILTER(maxAnisotropy);
    }

    // Convert min/mag/mip to D3D11 filter bits
    const bool minLinear = (minFilter == TextureFilter::Linear);
    const bool magLinear = (magFilter == TextureFilter::Linear);
    const bool mipLinear = (mipFilter == TextureFilter::Linear);

    if (!minLinear && !magLinear && !mipLinear) return D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (!minLinear && !magLinear && mipLinear)  return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    if (!minLinear && magLinear && !mipLinear)  return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    if (!minLinear && magLinear && mipLinear)   return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    if (minLinear && !magLinear && !mipLinear)  return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if (minLinear && !magLinear && mipLinear)   return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    if (minLinear && magLinear && !mipLinear)   return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    if (minLinear && magLinear && mipLinear)    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    return D3D11_FILTER_MIN_MAG_MIP_LINEAR; // default fallback
}

inline D3D11_TEXTURE_ADDRESS_MODE ToD3D11Address(TextureAddressMode mode)
{
    switch (mode)
    {
    case TextureAddressMode::Wrap:   return D3D11_TEXTURE_ADDRESS_WRAP;
    case TextureAddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
    case TextureAddressMode::Clamp:  return D3D11_TEXTURE_ADDRESS_CLAMP;
    case TextureAddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
    default:                         return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

inline D3D11_COMPARISON_FUNC ToD3D11Comparison(ComparisonFunc func)
{
    switch (func)
    {
    case ComparisonFunc::Never:        return D3D11_COMPARISON_NEVER;
    case ComparisonFunc::Less:         return D3D11_COMPARISON_LESS;
    case ComparisonFunc::Equal:        return D3D11_COMPARISON_EQUAL;
    case ComparisonFunc::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
    case ComparisonFunc::Greater:      return D3D11_COMPARISON_GREATER;
    case ComparisonFunc::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
    case ComparisonFunc::Always:       return D3D11_COMPARISON_ALWAYS;
    default:                           return D3D11_COMPARISON_ALWAYS;
    }
}


inline D3D12_STENCIL_OP ConvertStencilOp(StencilOp op) {
    switch (op) {
    case StencilOp::Keep:    return D3D12_STENCIL_OP_KEEP;
    case StencilOp::Zero:    return D3D12_STENCIL_OP_ZERO;
    case StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
    case StencilOp::IncrSat: return D3D12_STENCIL_OP_INCR_SAT;
    case StencilOp::DecrSat: return D3D12_STENCIL_OP_DECR_SAT;
    case StencilOp::Invert:  return D3D12_STENCIL_OP_INVERT;
    case StencilOp::Incr:    return D3D12_STENCIL_OP_INCR;
    case StencilOp::Decr:    return D3D12_STENCIL_OP_DECR;
    default: return D3D12_STENCIL_OP_KEEP;
    }
}

inline D3D12_COMPARISON_FUNC ConvertComparisonFunc(ComparisonFunc func) {
    switch (func) {
    case ComparisonFunc::Never:    return D3D12_COMPARISON_FUNC_NEVER;
    case ComparisonFunc::Less:     return D3D12_COMPARISON_FUNC_LESS;
    case ComparisonFunc::Equal:    return D3D12_COMPARISON_FUNC_EQUAL;
    case ComparisonFunc::LessEqual:return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case ComparisonFunc::Greater:  return D3D12_COMPARISON_FUNC_GREATER;
    case ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case ComparisonFunc::Always:   return D3D12_COMPARISON_FUNC_ALWAYS;
    default: return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}


inline D3D12_BLEND ConvertBlend(Blend factor) {
    switch (factor) {
    case Blend::Zero: return D3D12_BLEND_ZERO;
    case Blend::One: return D3D12_BLEND_ONE;
    case Blend::SrcColor: return D3D12_BLEND_SRC_COLOR;
    case Blend::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
    case Blend::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
    case Blend::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
    case Blend::DestAlpha: return D3D12_BLEND_DEST_ALPHA;
    case Blend::InvDestAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
    case Blend::DestColor: return D3D12_BLEND_DEST_COLOR;
    case Blend::InvDestColor: return D3D12_BLEND_INV_DEST_COLOR;
    default: return D3D12_BLEND_ONE;
    }
}

inline D3D12_BLEND_OP ConvertBlendOp(BlendOp op) {
    switch (op) {
    case BlendOp::Add: return D3D12_BLEND_OP_ADD;
    case BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
    case BlendOp::RevSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendOp::Min: return D3D12_BLEND_OP_MIN;
    case BlendOp::Max: return D3D12_BLEND_OP_MAX;
    default: return D3D12_BLEND_OP_ADD;
    }
}


inline D3D12_SHADER_VISIBILITY ConvertShaderStage(IShaderStage stageFlags)
{
    if (stageFlags == IShaderStage::None)
        return D3D12_SHADER_VISIBILITY_ALL;

    // Only handle single-stage visibility for root parameters
    if (stageFlags & IShaderStage::Vertex)
        return D3D12_SHADER_VISIBILITY_VERTEX;
    if (stageFlags & IShaderStage::Fragment)
        return D3D12_SHADER_VISIBILITY_PIXEL;
    if (stageFlags & IShaderStage::Compute)
        return D3D12_SHADER_VISIBILITY_ALL;
    if (stageFlags & IShaderStage::Geometry)
        return D3D12_SHADER_VISIBILITY_ALL;
    if (stageFlags & IShaderStage::TessControl)
        return D3D12_SHADER_VISIBILITY_ALL;
    if (stageFlags & IShaderStage::TessEval)
        return D3D12_SHADER_VISIBILITY_ALL;

    return D3D12_SHADER_VISIBILITY_ALL; // fallback
}


#endif
#include "vulkan/vulkan.h"
#include <unordered_set>

inline D3D12_RESOURCE_STATES ToD3D12ResourceState(ImageLayout layout, ImageAccessLayout accessMask)
{
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    // Layout-based state
    switch (layout)
    {
    case ImageLayout::Undefined:     state = D3D12_RESOURCE_STATE_COMMON; break;
    case ImageLayout::RenderTarget:  state = D3D12_RESOURCE_STATE_RENDER_TARGET; break;
    case ImageLayout::DepthStencil:  state = D3D12_RESOURCE_STATE_DEPTH_WRITE; break;
    case ImageLayout::ShaderRead:    state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; break;
    case ImageLayout::Present:       state = D3D12_RESOURCE_STATE_PRESENT; break;
    case ImageLayout::CopySrc:       state = D3D12_RESOURCE_STATE_COPY_SOURCE; break;
    case ImageLayout::CopyDst:       state = D3D12_RESOURCE_STATE_COPY_DEST; break;
    case ImageLayout::GenericRead:   state = D3D12_RESOURCE_STATE_GENERIC_READ; break;
    }

    // AccessMask overrides (optional)
    if ((accessMask & ImageAccessLayout::Read) != ImageAccessLayout::None)
        state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if ((accessMask & ImageAccessLayout::Write) != ImageAccessLayout::None)
        state |= D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    return state;
}

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


#ifdef WIN32OGL
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
    case ShaderDesc::Type::Compute:      return GL_COMPUTE_SHADER;
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
    case BufferType::Storage:  return GL_SHADER_STORAGE_BUFFER;     // Structured/storage buffer
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

#endif


struct ShaderData {
    sVec<u8> data;
    sVec<u8> ogData;
};
#include "3rdparty/bspirv/spirv_hlsl.hpp"
#include <d3dcompiler.h>
#include "Data.h"

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

        spirv_cross::CompilerHLSL compiler(intVec);
        spirv_cross::CompilerHLSL::Options options;
        options.shader_model = 50;
        compiler.set_hlsl_options(options);
        std::string hlsl_source = compiler.compile();
        intVec.clear();
        auto p = std::string(desc == ShaderDesc::Type::Vertex ? "v" : "p");
        p.append("s_5_0");


        ID3DBlob* blob;

        HRESULT hr = D3DCompile(
            hlsl_source.c_str(), hlsl_source.size(),
            nullptr,                 // optional: source name
            nullptr, nullptr,        // macros, include
            "main",                  // entry point
            p.c_str(),                   // target profile: vs_5_0 or ps_5_0
            D3DCOMPILE_ENABLE_STRICTNESS,
            0,
            &blob,
            0
        );

        if (hr == 0) {
            sdd.ogData = dataD->getData();
            sdd.data = sVec<u8>(blob->GetBufferSize());
            memcpy(sdd.data.data(), blob->GetBufferPointer(), blob->GetBufferSize());
            blob->Release();
        }
    }
    delete dataD;
    return sdd;
}