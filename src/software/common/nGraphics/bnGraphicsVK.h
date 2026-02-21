#pragma once
#include "nGraphics/ExplicitGraphicsAbstract.h"
#include <vulkan/vulkan.h>
#include <iostream>
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif
#include <stdexcept>
#include <array>
#include "GraphicsUtilities.h"

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
        throw std::runtime_error("Unsupported vertex element type");
    }
}


class InputLayoutVK : public IInputLayout{
public:
    VkPipelineVertexInputStateCreateInfo vkVertexInputInfo{};
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    InputLayoutVK(const InputLayoutDesc& desc)
    {
        this->desc = desc;
        bindings.clear();
        attributes.clear();

        // Single vertex buffer binding (for simplicity)
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = desc.stride; // stride of one vertex
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(binding);

        for (uint8_t i = 0; i < desc.elements.size(); ++i)
        {
            auto& elem = desc.elements[i]; // your vertex elements
            VkVertexInputAttributeDescription attr{};
            attr.location = elem.semanticIndex;
            attr.binding = elem.inputSlot;
            attr.format = TranslateTypeToVkFormat(elem.type);
            attr.offset = elem.offset;
            attributes.push_back(attr);
        }

        vkVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vkVertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vkVertexInputInfo.pVertexBindingDescriptions = bindings.data();
        vkVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vkVertexInputInfo.pVertexAttributeDescriptions = attributes.data();
    }

    void* GetNativeHandle() override {
        return nullptr;
    }

    void Release() {
        delete this;
    }

    VkPipelineVertexInputStateCreateInfo* GetVertexInputState() { return &vkVertexInputInfo; }

};

class BufferVK : public IBuffer
{
public:
    BufferVK(VkDevice device, BufferDesc desc)
        : device(device), desc(desc) {
        type = desc.type;
    }

    ~BufferVK() {
        Release();
    }

    void Release() override
    {
        if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
        if(stagingBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, stagingBuffer, nullptr);
        if (stagingMemory != VK_NULL_HANDLE) vkFreeMemory(device, stagingMemory, nullptr);
        if (mStagingBuffer) mStagingBuffer->Release();
    }

    void* GetNativeHandle() override { return buffer; }

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDevice device ;
    BufferDesc desc;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    IBuffer* mStagingBuffer = VK_NULL_HANDLE;
    std::vector<u8> dataBuffer;
};


class ShaderVK : public IShader {
public:
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkDevice device;

    ShaderVK(VkDevice device, const ShaderDesc& desc)
        : device(device)
    {
        type = desc.type;
        ogData = desc.ogData;
        binary.reserve(desc.bytecodeSize);
        binary.insert(binary.end(),
            desc.bytecode,
            desc.bytecode + desc.bytecodeSize);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = desc.bytecodeSize;
        createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.bytecode);

        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module");
    }

    ~ShaderVK() {
        Release();
    }

    void Release() {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, shaderModule, nullptr);
        }
    }

    void* GetNativeHandle() override { return shaderModule; }

    VkPipelineShaderStageCreateInfo GetStageInfo() const {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // default
        stageInfo.module = shaderModule;
        stageInfo.pName = "main"; // entry point

        // Set stage according to shader type
        switch (type)
        {
        case ShaderDesc::Type::Vertex:
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderDesc::Type::Pixel:  // fragment shader
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case ShaderDesc::Type::Compute:
            stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        case ShaderDesc::Type::Geometry:
            stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        }

        return stageInfo;
    }
};

class TextureVK : public ITexture {
public:
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    //VkWriteDescriptorSet descriptor;
    //VkDescriptorSetLayout layout;
    //VkDescriptorSet descriptorSet;

    ~TextureVK() {
        Release();
    }

    TextureVK(bool ownership = true)  {
        owner = ownership;
    };

    uint32_t width, height, mipLevels;

    void Release() override {
        if (owner) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device, sampler, nullptr);
                sampler = VK_NULL_HANDLE;
            }

            if (image != VK_NULL_HANDLE) {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
            }

            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }

            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }

        desc = {};

    }

    void* GetNativeHandle() override {
        return image;
    }

};

class ViewportVK : public IViewPort
{
public:
    ViewportVK(const ViewPortDesc& desc)
    {
        auto vp = desc.viewport;


        viewport.x = vp->x;
        viewport.y = vp->y;
        viewport.width = static_cast<float>(vp->width);
        viewport.height = static_cast<float>(vp->height);
        viewport.minDepth = vp->minDepth;
        viewport.maxDepth = vp->maxDepth;

        scissor.offset = { vp->x, vp->y };
        scissor.extent = { static_cast<UINT>(vp->width), static_cast<UINT>(vp->height) };
    }

    void* GetNativeHandle() override { return nullptr; }

    VkViewport viewport{};
    VkRect2D   scissor{};
};

class RasterizerVK : public IRasterizerState {
public:
    VkPipelineRasterizationStateCreateInfo vkRaster{};

    RasterizerVK(const RasterizerDesc& desc) {
        vkRaster.lineWidth = 1.0f;
        vkRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        vkRaster.polygonMode = (desc.fillMode == IFillMode::Solid) ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE;
        vkRaster.cullMode = (desc.cullMode == CullMode::Back) ? VK_CULL_MODE_BACK_BIT :
            (desc.cullMode == CullMode::Front) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
        vkRaster.frontFace = desc.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        vkRaster.depthBiasEnable = VK_FALSE;
    }

    void* GetNativeHandle() override { return nullptr; }

    void Release() {
        delete this;
    }
};

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


class DepthStencilStateVK : public IDepthStencilState {
public:
    VkPipelineDepthStencilStateCreateInfo vkDepthStencil{};

    DepthStencilStateVK(const DepthStencilDesc& desc) {
        vkDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        vkDepthStencil.depthTestEnable = desc.depthEnable ? VK_TRUE : VK_FALSE;
        vkDepthStencil.depthWriteEnable = desc.depthWriteMask ? VK_TRUE : VK_FALSE;
        vkDepthStencil.depthCompareOp = TranslateComparisonFunc(desc.depthFunc);
        vkDepthStencil.stencilTestEnable = desc.stencilEnable ? VK_TRUE : VK_FALSE;
        // Fill front/back stencil ops
    }

    void* GetNativeHandle() {
        return nullptr; //VK!
    }

    void Release() {
        delete this;
    }
};

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


class BlendStateVK : public IBlendState {
public:
    VkPipelineColorBlendStateCreateInfo vkBlend{};
    std::vector<VkPipelineColorBlendAttachmentState> attachments;

    BlendStateVK(const BlendStateDesc& desc) {
        attachments.resize(1); // max render targets
        for (int i = 0; i < 1; ++i) {
            auto& a = attachments[i];
            a.blendEnable = desc.renderTarget[i].blendEnable ? VK_TRUE : VK_FALSE;
            a.srcColorBlendFactor = TranslateBlend(desc.renderTarget[i].srcBlend);
            a.dstColorBlendFactor = TranslateBlend(desc.renderTarget[i].destBlend);
            a.colorBlendOp = TranslateBlendOp(desc.renderTarget[i].blendOp);
            a.srcAlphaBlendFactor = TranslateBlend(desc.renderTarget[i].srcBlendAlpha);
            a.dstAlphaBlendFactor = TranslateBlend(desc.renderTarget[i].destBlendAlpha);
            a.alphaBlendOp = TranslateBlendOp(desc.renderTarget[i].blendOpAlpha);
            a.colorWriteMask = desc.renderTarget[i].renderTargetWriteMask;
        }
        vkBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        vkBlend.attachmentCount = static_cast<uint32_t>(attachments.size());
        vkBlend.pAttachments = attachments.data();
    }

    void Release() {
        delete this;
    }

    void* GetNativeHandle() override { return nullptr; }
};

class SamplerVK : public ISamplerState {
public:
    VkDevice device;
    VkSampler sampler = VK_NULL_HANDLE;

    SamplerVK(VkDevice dev, const SamplerStateDesc& desc, sVec<SamplerVK*>* samplerReleasePointer) : device(dev), samplerReleasePointer(samplerReleasePointer) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        // Convert filters
        samplerInfo.minFilter = ToVkFilter(desc.minFilter);
        samplerInfo.magFilter = ToVkFilter(desc.magFilter);
        samplerInfo.mipmapMode = ToVkMipmapMode(desc.mipFilter);

        // Convert address modes
        samplerInfo.addressModeU = ToVkAddressMode(desc.addressU);
        samplerInfo.addressModeV = ToVkAddressMode(desc.addressV);
        samplerInfo.addressModeW = ToVkAddressMode(desc.addressW);

        // LOD
        samplerInfo.mipLodBias = desc.mipLODBias;
        samplerInfo.minLod = desc.minLOD;
        samplerInfo.maxLod = desc.maxLOD;

        // Anisotropy
        samplerInfo.anisotropyEnable = (desc.maxAnisotropy > 1) ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = static_cast<float>(desc.maxAnisotropy);

        // Comparison function
        if (desc.comparisonFunc != ComparisonFunc::Always) {
            samplerInfo.compareEnable = VK_TRUE;
            samplerInfo.compareOp = ToVkCompareOp(desc.comparisonFunc);
        }
        else {
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        }

        // Border color
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; // default
        if (desc.addressU == TextureAddressMode::Border ||
            desc.addressV == TextureAddressMode::Border ||
            desc.addressW == TextureAddressMode::Border) {
            samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
            // Or map your borderColor to closest VK_BORDER_COLOR enum
        }

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan sampler!");
        }
    }

    void Release() override {
        samplerReleasePointer->push_back(this);
    }

    void* GetNativeHandle() override {
        return sampler;
    }

private:
    VkFilter ToVkFilter(TextureFilter filter) {
        switch (filter) {
        case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        case TextureFilter::Linear:  return VK_FILTER_LINEAR;
        case TextureFilter::Anisotropic: return VK_FILTER_LINEAR; // Vulkan uses anisotropyEnable instead
        }
        return VK_FILTER_LINEAR;
    }

    VkSamplerMipmapMode ToVkMipmapMode(TextureFilter filter) {
        switch (filter) {
        case TextureFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case TextureFilter::Linear:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case TextureFilter::Anisotropic: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    VkSamplerAddressMode ToVkAddressMode(TextureAddressMode mode) {
        switch (mode) {
        case TextureAddressMode::Wrap:   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureAddressMode::Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureAddressMode::Clamp:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureAddressMode::Border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    VkCompareOp ToVkCompareOp(ComparisonFunc func) {
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

    sVec<SamplerVK*>* samplerReleasePointer;
};



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

struct PendingDrawVK : public IPendingDraw {
    VkCommandBuffer cmdBuffer;
};

class CommandListVK : public ICommandList {
public:
    VkDevice device;
    VkCommandBuffer cmdBuffer;
    sVec<CommandListVK*>* releases;
    sVec<PendingDrawVK>* pDraws; 
    PendingDrawVK draw;

    CommandListVK(VkDevice dev, VkCommandBuffer buffer, sVec<CommandListVK*>* rel, sVec<PendingDrawVK>* pDraws)
        : device(dev), cmdBuffer(buffer), releases(rel), pDraws(pDraws){
        draw.cmdBuffer = cmdBuffer;
    }

    void Release() {
        if(releases) releases->push_back(this);
    }

    virtual void BindPipeline(IPipeline* pipeline) override;
    virtual void BindDescriptorSet(IDescriptorSet* set, uint32_t index = 0) override;

    virtual void BindViewPort(IViewPort* viewport) override;
    virtual void BindScissor(IViewPort* scissor) override;
    virtual void BindBuffer(IBuffer* buffer) override;

    virtual void Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset = 0) override;
    virtual void DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset = 0) override;

    virtual void CopyToBuffer(IBuffer* buffer, void* data, size_t size) override;

   /* void TransitionImageLayout(ITexture image,
        TextureFormat format,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        VkCommandBuffer buffer = nullptr
    );*/
   
};

class PipelineVK;


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


class CommandBufferVK : public ICommandBuffer {
public:
    VkCommandBuffer buffer;
    VkCommandPool& pool;

    std::vector<VkImageMemoryBarrier> pendingBarriers;

    CommandBufferVK(VkCommandBuffer buffer, VkCommandPool& pool) : buffer(buffer), pool(pool) {}



    VkImageMemoryBarrier BarrierCreator(ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) {
        auto vkTexture = dynamic_cast<TextureVK*>(image);
        if (!vkTexture) return {};


        if (oldLayout != image->explicitLayout) {
            oldLayout = image->explicitLayout;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = ToVkImageLayout(oldLayout);
        barrier.newLayout = ToVkImageLayout(newLayout);
        barrier.srcAccessMask = ToVkAccessFlags(srcAccessMask);
        barrier.dstAccessMask = ToVkAccessFlags(dstAccessMask);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vkTexture->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        return barrier;
    }

    void PipelineBarrierBatched(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override {
        auto vkTexture = dynamic_cast<TextureVK*>(image);
        if (!vkTexture) return;

        auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
        pendingBarriers.push_back(barrier);

        image->explicitLayout = newLayout;
    }

    void FlushBatchedBarriers() override {

        if (!pendingBarriers.empty()) {
            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            vkCmdPipelineBarrier(
                buffer,
                srcStage,    // src stage
                dstStage, // dst stage
                0,                                    // dependency flags
                0, nullptr,                            // memory barriers
                0, nullptr,                            // buffer barriers
                static_cast<uint32_t>(pendingBarriers.size()), pendingBarriers.data() // image barriers
            );

            pendingBarriers.clear();
        }
    }


    void PipelineBarrier(
        ITexture* image,
        ImageLayout oldLayout,
        ImageLayout newLayout,
        ImageAccessLayout srcAccessMask,
        ImageAccessLayout dstAccessMask) override
    {

        auto vkTexture = dynamic_cast<TextureVK*>(image);
        if (!vkTexture) return;


        if (oldLayout != image->explicitLayout) {
            oldLayout = image->explicitLayout;
        }

        VkImageMemoryBarrier barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
      
        //Todo: Determine source/destination stage masks
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkCmdPipelineBarrier(
            this->buffer, // VkCommandBuffer
            srcStage,
            dstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        image->explicitLayout = newLayout;
    }


    void* GetNativeHandle() {
        return buffer;
    }
};

inline VkDescriptorSetLayoutCreateFlags ToVkFlags(DescriptorSetLayoutFlags flags) {
    VkDescriptorSetLayoutCreateFlags vkFlags = 0;
    if (static_cast<uint32_t>(flags) & static_cast<uint32_t>(DescriptorSetLayoutFlags::PUSH_DESCRIPTOR)) {
        vkFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }
    return vkFlags;
}

class DescriptorSetLayoutVK : public IDescriptorSetLayout {
public:
    VkDescriptorSetLayout layout;
    VkDevice device;
    DescriptorSetLayoutDesc desc;

    DescriptorSetLayoutVK(VkDevice device, DescriptorSetLayoutDesc desc) : device(device), desc(desc) {
        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(desc.bindings.size());

        for (auto& b : desc.bindings) {
            vkBindings.push_back({ b.binding, ToVkDescriptorType(b.type), b.count, ToVkShaderStageFlags(b.stageFlags), nullptr });
        }

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.flags = ToVkFlags(desc.flags);
        info.bindingCount = static_cast<uint32_t>(vkBindings.size());
        info.pBindings = vkBindings.data();

        if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void* GetNativeHandle() {
        return layout;
    }

    ~DescriptorSetLayoutVK() {
        Release();
    }

    void Release() {
        if (layout) {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
            layout = nullptr;
        }
    }
};

class DescriptorPoolVK : public IDescriptorPool {
public:
    VkDescriptorPool pool;
    VkDevice device;

    DescriptorPoolVK(VkDevice device, DescriptorPoolDesc desc) : device(device) {
        std::vector<VkDescriptorPoolSize> vkSizes;
        vkSizes.reserve(desc.poolSizes.size());

        for (auto& s : desc.poolSizes) {
            vkSizes.push_back({ ToVkDescriptorType(s.first), s.second });
        }

        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = ToVkDescriptorPoolFlags(desc.flags);
        info.maxSets = desc.maxSets;
        info.poolSizeCount = static_cast<uint32_t>(vkSizes.size());
        info.pPoolSizes = vkSizes.data();

        if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void* GetNativeHandle() {
        return pool;
    }

    void Release() {
        if (pool) {
            vkDestroyDescriptorPool(device, pool, nullptr);
            pool = nullptr;
        }
    }
};

class DescriptorSetVK : public IDescriptorSet {
public:
    VkDevice device;
    VkDescriptorSet descriptorSet;
    PipelineVK* pipeline;

    std::vector<BufferVK*> buffers;
    std::vector<TextureVK*> textures;
    std::vector<SamplerVK*> samplers;

    DescriptorSetVK(VkDevice dev, VkDescriptorSet set, PipelineVK* parentP) : device(dev), descriptorSet(set), pipeline(parentP) {}
    ~DescriptorSetVK();

    virtual void SetBuffer(uint32_t slot, IBuffer* buffer) override;
    virtual void SetTexture(uint32_t slot, ITexture* texture) override;
    virtual void SetSampler(uint32_t slot, ISamplerState* sampler) override;
    virtual void Update() override;
};

class bnGraphicsVK;

class DepthStencilVK : public IDepthStencil {
public:
    ITexture* texture;
    VkImageView depthView;
public:
    void* GetNativeHandle() {
        return depthView;
    }
    void Release() {}
};

class RenderTargetVK : public IRenderTarget {
public:
    std::vector<TextureVK*> textures;
    DepthStencilVK* depth = nullptr;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
public:
    void* GetNativeHandle() {
        return renderPass;
    }
    void Release() {
        for (auto& texture : textures) {
            texture->Release();
        }
    }
};

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
        throw std::runtime_error("Unsupported sample count for Vulkan MSAA");
    }
}


class PipelineVK : public IPipeline {
public:
    IGraphicsDeviceConfig& config;
    VkDevice device;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    DescriptorSetLayoutVK* descriptorSetLayout = VK_NULL_HANDLE;
    sVec<PipelineVK*>* releaseVec;
    DescriptorPoolVK* descriptorPool = VK_NULL_HANDLE;  // <-- new: pool for descriptor sets
    std::map<u32, DescriptorSetVK*> descriptorSets;
    IRenderTarget* renderTarget;
    void* GetNativeHandle() { return pipeline; }

    PipelineVK(
        sVec<PipelineVK*>* releaseVec,
        VkDevice dev,
        DescriptorSetLayoutVK* setLayout,
        DescriptorPoolVK* pool,
        std::vector<ShaderVK*> shaders,
        InputLayoutVK* inputLayout,
        RasterizerVK* rasterizer,
        DepthStencilStateVK* depthStencil,
        BlendStateVK* blendState,
        RenderTargetVK* renderTarget,
        IGraphicsDeviceConfig& config)
        : config(config), device(dev), descriptorSetLayout(setLayout), descriptorPool(pool), releaseVec(releaseVec), renderTarget(renderTarget)
    {
        // 1. Create pipeline layout
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout->layout;
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout!");
        
        // 2. Create graphics pipeline
        CreateGraphicsPipeline(shaders, inputLayout, rasterizer, depthStencil, blendState, renderTarget->renderPass);
    }

    ~PipelineVK() {  }

    void Release() {
        releaseVec->push_back(this);
    }



    // -------------------------------
    // Descriptor set creation (no parameters)
    IDescriptorSet* CreateDescriptorSet(u32 slot = 0) override {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool->pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout->layout;

        VkDescriptorSet vkSetHandle;
        auto result = vkAllocateDescriptorSets(device, &allocInfo, &vkSetHandle);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor set!");

        DescriptorSetVK* ds = new DescriptorSetVK(device, vkSetHandle, this);
        descriptorSets[slot] = ds;
        return ds;
    }

    IDescriptorSet* GetDescriptorSet(u32 slot = 0) {
        auto it = descriptorSets.find(slot);
        if (it != descriptorSets.end()) {
            return it->second;
        }
        else return nullptr;
    }

private:
    void CreateGraphicsPipeline(
        const std::vector<ShaderVK*>& shaders,
        InputLayoutVK* inputLayout,
        RasterizerVK* rasterizer,
        DepthStencilStateVK* depthStencil,
        BlendStateVK* blendState,
        VkRenderPass renderPass)
    {
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        std::vector<VkPipelineShaderStageCreateInfo> vkStages;
        vkStages.reserve(shaders.size());
        for (auto shader : shaders) vkStages.push_back(shader->GetStageInfo());

        pipelineInfo.stageCount = static_cast<uint32_t>(vkStages.size());
        pipelineInfo.pStages = vkStages.data();
        pipelineInfo.pVertexInputState = &inputLayout->vkVertexInputInfo;
        pipelineInfo.pRasterizationState = &rasterizer->vkRaster;
        pipelineInfo.pDepthStencilState = &depthStencil->vkDepthStencil;
        pipelineInfo.pColorBlendState = &blendState->vkBlend;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        pipelineInfo.pInputAssemblyState = &inputAssembly;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicInfo{};
        dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicInfo.pDynamicStates = dynamicStates.data();
        pipelineInfo.pDynamicState = &dynamicInfo;

        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = pipeline;
        pipelineInfo.basePipelineIndex = 0;

        // Viewport + scissor must always be defined, even if dynamic
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1; // Must be >0
        viewportState.pViewports = nullptr; // ignored if dynamic
        viewportState.scissorCount = 1;
        viewportState.pScissors = nullptr;  // ignored if dynamic
        pipelineInfo.pViewportState = &viewportState;

        // Multisampling (even if you don�t use MSAA)
        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = IntToVkSampleCount(config.enableMSAA ? config.msaaSamples : 1);
        multisample.sampleShadingEnable = VK_FALSE;
        pipelineInfo.pMultisampleState = &multisample;

        auto res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        if (res != VK_SUCCESS)
            throw std::runtime_error("Failed to create graphics pipeline!");
    }
};




class PipelineBuilderVK : public IPipelineBuilder {
public:
    PipelineBuilderVK(IGraphicsDeviceConfig& config, sVec<PipelineVK*>* releaseVec, VkDevice device, RenderTargetVK* pass) : config(config), device(device), renderPass(pass), releaseVec(releaseVec) {}

    PipelineBuilderVK& AddShader(IShader* shader) override {
        auto vShader = dynamic_cast<ShaderVK*>(shader);

        if (!vShader) return *this;

        shaders.push_back(vShader);
        return *this;
    }

    PipelineBuilderVK& SetInputLayout(IInputLayout* layout) override {
        auto vLayout = dynamic_cast<InputLayoutVK*>(layout);

        if (!vLayout) return *this;

        inputLayout = vLayout;
        return *this;
    }

    PipelineBuilderVK& SetRasterizer(IRasterizerState* raster) {
        auto vRaster = dynamic_cast<RasterizerVK*>(raster);

        if (!vRaster) return *this;

        rasterizer = vRaster;
        return *this;
    }

    PipelineBuilderVK& SetDepthStencil(IDepthStencilState* depth) {
        auto vDepth = dynamic_cast<DepthStencilStateVK*>(depth);

        if (!vDepth) return *this;

        depthStencil = vDepth;
        return *this;
    }

    PipelineBuilderVK& SetBlendState(IBlendState* blend) {
        auto vBlend = dynamic_cast<BlendStateVK*>(blend);

        if (!vBlend) return *this;

        blendState = vBlend;
        return *this;
    }

    PipelineBuilderVK& SetDescriptorPool(IDescriptorPool* pool) {
        auto vPool = dynamic_cast<DescriptorPoolVK*>(pool);

        if (!vPool) return *this;

        descriptorPool = vPool;
        return *this;
    }

    IPipelineBuilder& SetRenderTarget(IRenderTarget* target) {
        auto vRenderTarget = dynamic_cast<RenderTargetVK*>(target);

        if (!vRenderTarget) return *this;

        renderPass = vRenderTarget;
        return *this;
    };

    PipelineBuilderVK& SetDescriptorSetLayout(IDescriptorSetLayout* layout) {
        auto vLayout = dynamic_cast<DescriptorSetLayoutVK*>(layout);

        if (!vLayout) return *this;

        descriptorSetLayout = vLayout;
        return *this;
    }

    sVec<IShader*>* GetShaders() {

        if (shaders.size() != interfaceShaders.size()) {
            interfaceShaders.clear();
            for (auto* s : shaders) {
                interfaceShaders.push_back(static_cast<IShader*>(s));
            }
    }

        return &interfaceShaders;
    }

    IPipeline* Build() override {
        if (!inputLayout || !rasterizer || !depthStencil || !blendState || !renderPass) {
            delete this;
            return nullptr;
        }

        PipelineVK* pipeline = new PipelineVK( 
            releaseVec,
            device,
            descriptorSetLayout,
            descriptorPool,
            shaders,
            inputLayout,
            rasterizer,
            depthStencil,
            blendState,
            renderPass,
            config
        );

        delete this;
        return pipeline;
    }

public:
    sVec<IShader*> interfaceShaders;
    IGraphicsDeviceConfig& config;
    VkDevice device;
    std::vector<ShaderVK*> shaders;
    InputLayoutVK* inputLayout = nullptr;
    RasterizerVK* rasterizer = nullptr;
    DepthStencilStateVK* depthStencil = nullptr;
    BlendStateVK* blendState = nullptr;
    DescriptorSetLayoutVK* descriptorSetLayout = nullptr;
    DescriptorPoolVK* descriptorPool = nullptr;
    RenderTargetVK* renderPass = VK_NULL_HANDLE;
    sVec<PipelineVK*>* releaseVec;
};

class DeviceVK : public IDevice {
public:
    void* GetNativeHandle() override {
        return device;
    }
    void Release() override {
        if (device) {
            vkDestroyDevice(device, nullptr);
        }
    }

    VkDevice operator->() {
        return device;
    }


    VkDevice Get() {
        return device;
    }

    operator VkDevice() const { return device; }

private:
    VkDevice device;
    friend class bnGraphicsVK;
};

class DeviceContextVK : public IDeviceContext {
public:
    void* GetNativeHandle() override {
        return deviceC;
    }
    void Release() override {
        if (deviceC) {
            vkFreeCommandBuffers(
                device,           // VkDevice
                commandPool,     
                1,               
                &deviceC   
            );
        }
    }

    VkCommandBuffer operator->() {
        return deviceC;
    }

    operator VkCommandBuffer() const { return deviceC; }

    operator VkCommandPool() const { return commandPool; }

private:
    VkDevice device;
    VkCommandBuffer deviceC;
    VkCommandPool commandPool;
    friend class bnGraphicsVK;
};

class CommandPoolVK : public ICommandPool {
public:
    VkCommandPool commandPool;
    VkDevice device;

    CommandPoolVK(VkDevice device, CommandPoolDesc desc) : device(device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = desc.queueFamilyIndex;
        poolInfo.flags = 0;

        if (desc.transient) {
            poolInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        }
        if (desc.resettable) {
            poolInfo.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }

      
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

    }

    void Release() {
        if(commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    }

    void* GetNativeHandle() {
        return commandPool;
    }
};



class bnGraphicsVK : IGraphicsDeviceExplicit
{
public:
    IGraphicsDeviceConfig& config;
    bnGraphicsVK(SysHandle& handle, IGraphicsDeviceConfig& config);
    
    const char* GetAPIName() const {
        return "VULKAN";
    }

    uint32_t GetAPIVersion() const {
        return 130;
    }

    bool IsFeatureSupported(const std::string& feature) const {
        return true;
    }

    bool Init() override;

    ICommandList* GetCommandList() override;
    void DestroyPending();

    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(long width, long height) override;

    void ReleaseShader(IShader**) override;
    void ReleaseBuffer(IBuffer**) override;
    void ReleaseTexture(ITexture**) override;
    void ReleaseCommandPool(ICommandPool** pool) override;
    void ReleaseDescriptorPool(IDescriptorPool** pool) override;
    void ReleaseDescriptorSetLayout(IDescriptorSetLayout** layout) override;
    void ReleaseOnPend(void*) override;
    void Shutdown() override;

    IPipelineBuilder* CreatePipelineBuilder() override;
    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) override;
    IShader* CreateShader(const ShaderDesc& desc) override;
    IBuffer* CreateBuffer(const BufferDesc& desc, const void* data = nullptr) override;
    IInputLayout* CreateInputLayout(const InputLayoutDesc& desc) override;
    ISamplerState* CreateSamplerState(const SamplerStateDesc& desc) override;
    IViewPort* CreateViewPort(const ViewPortDesc& desc) override;
    IRasterizerState* CreateRasterizerState(const RasterizerDesc& desc) override;
    IDepthStencilState* CreateDepthStencilState(const DepthStencilDesc& desc) override;
    IBlendState* CreateBlendState(const BlendStateDesc& desc) override;
    IRenderTarget* CreateRenderTarget(const RenderTargetDesc& desc) override;
    IDepthStencil* CreateDepthStencil(ITexture* texture) override;
    ICommandPool* CreateCommandPool(CommandPoolDesc desc) override;
    IDescriptorPool* CreateDescriptorPool(DescriptorPoolDesc desc) override;
    IDescriptorSetLayout* CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc) override;

    IDeviceContext* getContext() override {
        return &commandBuffer[currentFrame];
    }
    IDevice* getDevice() override {
        return &device;
    }

    void WaitForNewFrame();
    ICommandBuffer* BeginSingleTimeCommands(ICommandPool* pool) override;
    void EndSingleTimeCommands(ICommandBuffer* buffer);
    void CopyToBuffer(IBuffer* buffer, ICommandBuffer* pool, void* data, size_t size) override;
    void MapBufferMemory(IBuffer* buffer, void** dataPtr) override;
    void UnmapBufferMemory(IBuffer* buffer) override;
    void CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc) override;
    void CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc) override;
    ITexture* GetSwapchainImage() override;
    sVec<PipelineVK*> releasePipelines;
    sVec<CommandListVK*> releaseCommandBuffers;
    bool dontClearYet = false;
    std::vector<VkFence> inFlightFences;
    uint32_t imageIndex;   // From vkAcquireNextImageKHR (swapchain image)
    size_t currentFrame;   // From modulo MAX_FRAMES_IN_FLIGHT
    void ClearPendingReleases() override;
    void WaitTillImFree() override;

    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF) override;
    void PopGroup() override;
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF) override;
public:
   
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateFrameBuffers();
    u32 FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    SysHandle& handle;
    std::atomic_bool free;
    VkInstance instance;
    DeviceVK device;
   
    VkQueue presentQueue;
    VkPhysicalDevice physicalDevice = nullptr;
    long width, height;
    VkSurfaceKHR surface = nullptr;
    VkSwapchainKHR swapChain = nullptr;
    std::vector<VkImage> swapChainImages;
    TextureVK* currentSwapChainImage = nullptr;
    VkFormat swapchainImageFormat;
    VkExtent2D swapChainExtent;
    RenderTargetVK* swpchTarget = nullptr;
    ITexture* msaaSWPCHTarget = nullptr;
    ITexture* depthTexture = nullptr;
    IDepthStencil* depthStencil = nullptr;
    std::vector<VkImageView> swapchainImageViews;
    VkImageView depthImageView = nullptr;
    std::vector<VkFramebuffer> swapChainFrameBuffers;
    VkImage depthImage = nullptr;
    VkDeviceMemory depthImageMemory = nullptr;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    //sVec<> commandPool;
    sVec<DeviceContextVK> commandBuffer;
    sVec<CommandListVK*> commandLists;
    //VkDescriptorPool descriptorPool;
    //VkDescriptorSetLayout textureSetLayout;

    // Release bla blas
    sVec<void*> pendVoids;
    sVec<IBuffer**> bufferRelease;
    sVec<ITexture**> textureRelease;
    sVec<IShader**> shaderRelease;
    sVec<ICommandPool**> poolRelease;
    sVec<IDescriptorPool**> descriptorPoolRelease;
    sVec<IDescriptorSetLayout**> descriptorSetLayoutRelease;
    sVec<CommandBufferVK*> cbRelease;
    sVec<SamplerVK*> samplerRelease;
    CommandPoolVK* copyPool = nullptr;
    VkQueue graphicsQueue;
    sVec<PendingDrawVK> pDraws;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkSampleCountFlagBits msaaSamples;
};

