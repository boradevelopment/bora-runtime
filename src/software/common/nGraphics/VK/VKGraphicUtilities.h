// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: VKGraphicUtilities.h
 * Purpose: All graphic related utilities for VULKAN
*/
#pragma once
#ifdef BORA_HAS_VULKAN
#include "vulkan/vulkan.h"
#if defined(Always)
  #pragma message("Always is defined as a macro!")
  // This forces the compiler to fail and print the exact file that defined it:
  #error "Find the leaking header above"
#endif

inline VkDescriptorType ToVkDescriptorType(DescriptorType type) {
    switch (type) {
    case DescriptorType::UniformBuffer:       return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorType::CombinedImageSampler:return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::StorageBuffer:       return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::StorageImage:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::Sampler:             return VK_DESCRIPTOR_TYPE_SAMPLER;
    default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

inline VkDescriptorPoolCreateFlags ToVkDescriptorPoolFlags(DescriptorPoolFlags flags) {
    VkDescriptorPoolCreateFlags result = 0;
    if (flags & DescriptorPoolFlags::FreeDescriptor)
        result |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (flags & DescriptorPoolFlags::UpdateAfterBind)
        result |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    return result;
}

inline VkShaderStageFlags ToVkShaderStageFlags(IShaderStage stages) {
    VkShaderStageFlags flags = 0;
    if (stages & IShaderStage::Vertex)      flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & IShaderStage::Fragment)    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & IShaderStage::Compute)     flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (stages & IShaderStage::Geometry)    flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages & IShaderStage::TessControl) flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages & IShaderStage::TessEval)    flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages & IShaderStage::AllGraphics) flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    if (stages & IShaderStage::All)         flags |= VK_SHADER_STAGE_ALL;
    return flags;
}

inline VkPrimitiveTopology ToVkPrimitiveTopology(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::Points:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveType::Lines:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveType::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveType::Triangles:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveType::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default:                           return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}


inline VkDescriptorSetLayoutCreateFlags ToVkFlags(DescriptorSetLayoutFlags flags) {
    VkDescriptorSetLayoutCreateFlags vkFlags = 0;
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DescriptorSetLayoutFlags::PUSH_DESCRIPTOR)) {
        vkFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }
    return vkFlags;
}


inline VkSampleCountFlagBits IntToVkSampleCount(int samples) {
    switch (samples) {
        case 1:  return VK_SAMPLE_COUNT_1_BIT;
        case 2:  return VK_SAMPLE_COUNT_2_BIT;
        case 4:  return VK_SAMPLE_COUNT_4_BIT;
        case 8:  return VK_SAMPLE_COUNT_8_BIT;
        case 16: return VK_SAMPLE_COUNT_16_BIT;
        case 32: return VK_SAMPLE_COUNT_32_BIT;
        case 64: return VK_SAMPLE_COUNT_64_BIT;
        default:
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

inline VkBlendFactor TranslateBlend(Blend blend)
{
    switch (blend)
    {
        case Blend::Zero:            return VK_BLEND_FACTOR_ZERO;
        case Blend::One:             return VK_BLEND_FACTOR_ONE;
        case Blend::SrcColor:        return VK_BLEND_FACTOR_SRC_COLOR;
        case Blend::InvSrcColor:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case Blend::SrcAlpha:        return VK_BLEND_FACTOR_SRC_ALPHA;
        case Blend::InvSrcAlpha:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case Blend::DestAlpha:       return VK_BLEND_FACTOR_DST_ALPHA;
        case Blend::InvDestAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case Blend::DestColor:       return VK_BLEND_FACTOR_DST_COLOR;
        case Blend::InvDestColor:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case Blend::SrcAlphaSat:     return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case Blend::BlendFactor:     return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case Blend::InvBlendFactor:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case Blend::Src1Color:       return VK_BLEND_FACTOR_SRC1_COLOR;
        case Blend::InvSrc1Color:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case Blend::Src1Alpha:       return VK_BLEND_FACTOR_SRC1_ALPHA;
        case Blend::InvSrc1Alpha:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        default:                     return VK_BLEND_FACTOR_ONE;
    }
}

inline VkBlendOp TranslateBlendOp(BlendOp op)
{
    switch (op)
    {
        case BlendOp::Add:           return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:      return VK_BLEND_OP_SUBTRACT;
        case BlendOp::RevSubtract:   return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:           return VK_BLEND_OP_MIN;
        case BlendOp::Max:           return VK_BLEND_OP_MAX;
        default:                     return VK_BLEND_OP_ADD;
    }
}

inline VkFilter ToVkFilter(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        case TextureFilter::Linear:  return VK_FILTER_LINEAR;
        case TextureFilter::Anisotropic: return VK_FILTER_LINEAR; // Vulkan uses anisotropyEnable instead
    }
    return VK_FILTER_LINEAR;
}

inline VkSamplerMipmapMode ToVkMipmapMode(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case TextureFilter::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case TextureFilter::Anisotropic: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

inline VkSamplerAddressMode ToVkAddressMode(TextureAddressMode mode) {
    switch (mode) {
        case TextureAddressMode::Wrap:   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureAddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureAddressMode::Clamp:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureAddressMode::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

inline VkCompareOp ToVkCompareOp(ComparisonFunc func) {
    switch (func) {
        case ComparisonFunc::Never:        return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:         return VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:        return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:      return VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Always:       return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}

inline VkCompareOp TranslateComparisonFunc(ComparisonFunc func)
{
    switch (func)
    {
        case ComparisonFunc::Never:        return VK_COMPARE_OP_NEVER;
        case ComparisonFunc::Less:         return VK_COMPARE_OP_LESS;
        case ComparisonFunc::Equal:        return VK_COMPARE_OP_EQUAL;
        case ComparisonFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonFunc::Greater:      return VK_COMPARE_OP_GREATER;
        case ComparisonFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonFunc::Always:       return VK_COMPARE_OP_ALWAYS;
        default:                           return VK_COMPARE_OP_ALWAYS;
    }
}


inline VkFormat TranslateTypeToVkFormat(VertexAttribType type)
{
    switch (type)
    {
        case VertexAttribType::Float: return VK_FORMAT_R32_SFLOAT;
        case VertexAttribType::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexAttribType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexAttribType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexAttribType::UInt: return VK_FORMAT_R32_UINT;
            // add more mappings if needed
        default:
            return VK_FORMAT_UNDEFINED;
    }
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


inline VkFormat ToVkFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8_UNorm:        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_UNorm_SRGB:   return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureFormat::BGRA8_UNorm:        return VK_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8_UNorm_SRGB:   return VK_FORMAT_B8G8R8A8_SRGB;

    case TextureFormat::RGBA16_Float:       return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::R16_Float:          return VK_FORMAT_R16_SFLOAT;
    case TextureFormat::R16_UNorm:          return VK_FORMAT_R16_UNORM;

    case TextureFormat::R32_Float:          return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG32_Float:         return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGBA32_Float:       return VK_FORMAT_R32G32B32A32_SFLOAT;

    case TextureFormat::D24_UNorm_S8_UInt:  return VK_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32_Float:          return VK_FORMAT_D32_SFLOAT;
    case TextureFormat::D32_Float_S8X24_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;

    default:
        throw std::runtime_error("Unsupported TextureFormat for Vulkan!");
    }
}

inline TextureFormat FromVkFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:       return TextureFormat::RGBA8_UNorm;
    case VK_FORMAT_R8G8B8A8_SRGB:        return TextureFormat::RGBA8_UNorm_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:       return TextureFormat::BGRA8_UNorm;
    case VK_FORMAT_B8G8R8A8_SRGB:        return TextureFormat::BGRA8_UNorm_SRGB;

    case VK_FORMAT_R16G16B16A16_SFLOAT:  return TextureFormat::RGBA16_Float;
    case VK_FORMAT_R16_SFLOAT:           return TextureFormat::R16_Float;
    case VK_FORMAT_R16_UNORM:            return TextureFormat::R16_UNorm;

    case VK_FORMAT_R32_SFLOAT:           return TextureFormat::R32_Float;
    case VK_FORMAT_R32G32_SFLOAT:        return TextureFormat::RG32_Float;
    case VK_FORMAT_R32G32B32A32_SFLOAT:  return TextureFormat::RGBA32_Float;

    case VK_FORMAT_D24_UNORM_S8_UINT:    return TextureFormat::D24_UNorm_S8_UInt;
    case VK_FORMAT_D32_SFLOAT:           return TextureFormat::D32_Float;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:   return TextureFormat::D32_Float_S8X24_UInt;

    default:
        throw std::runtime_error("Unsupported VkFormat for TextureFormat!");
    }
}

inline bool IsDepthFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

inline VkImageUsageFlags ToVkUsage(ImageUsage usage) {
    VkImageUsageFlags flags = 0;
    if ((usage & ImageUsage::ColorAttachment) != ImageUsage::None) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((usage & ImageUsage::DepthStencil) != ImageUsage::None) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((usage & ImageUsage::Sampled) != ImageUsage::None) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((usage & ImageUsage::Storage) != ImageUsage::None) flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if ((usage & ImageUsage::TransferSrc) != ImageUsage::None) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((usage & ImageUsage::TransferDst) != ImageUsage::None) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((usage & ImageUsage::InputAttachment) != ImageUsage::None) flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return flags;
}

inline VkImageLayout ToVkLayout(ImageLayout layout) {
    switch (layout) {
    case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageLayout::RenderTarget: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageLayout::DepthStencil: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageLayout::ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ImageLayout::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageLayout::CopyDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ImageLayout::GenericRead:
        return VK_IMAGE_LAYOUT_GENERAL;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline VkImageAspectFlags GetAspectFlags(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

#endif
