#include "dxgi.h"
#include "nGraphics/D3dx12.h"
#include "d3d11.h"
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include <d3dcompiler.h>
#include "Data.h"
#include "3rdparty/bspirv/spirv_hlsl.hpp"

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

inline ShaderData createShaderSPIRVToD3D(Data* dataD){
    ShaderData sdd{};
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
        sdd.valid = true;
        sdd.ogData = dataD->getData();
        sdd.data = sVec<u8>(blob->GetBufferSize());
        memcpy(sdd.data.data(), blob->GetBufferPointer(), blob->GetBufferSize());
        blob->Release();
    }

    return sdd;
}