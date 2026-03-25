#include "nGraphics/GraphicsAbstractions.h"
#import <Metal/Metal.h>

MTLPixelFormat ToMetalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8_UNorm:  return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::BGRA8_UNorm:  return MTLPixelFormatBGRA8Unorm;
        case TextureFormat::RGBA16_Float: return MTLPixelFormatRGBA16Float;
        case TextureFormat::R32_Float:    return MTLPixelFormatR32Float;

            // Depth/Stencil
        case TextureFormat::D32_Float:           return MTLPixelFormatDepth32Float;
        case TextureFormat::D24_UNorm_S8_UInt:   return MTLPixelFormatDepth24Unorm_Stencil8;
        case TextureFormat::D32_Float_S8X24_UInt:   return MTLPixelFormatDepth32Float_Stencil8;

        default:
            // Log a warning here: "Unsupported BoraFormat for Metal"
            return MTLPixelFormatInvalid;
    }
}

// You will also likely need this for Buffer/Vertex layouts
MTLVertexFormat ToMetalVertexFormat(VertexAttribType format) {
    switch (format) {
        case VertexAttribType::Float3: return MTLVertexFormatFloat3;
        case VertexAttribType::Float4: return MTLVertexFormatFloat4;
        case VertexAttribType::UInt4: return MTLVertexFormatUChar4;
        default: return MTLVertexFormatInvalid;
    }
}

MTLSamplerMinMagFilter MapFilter(TextureFilter filter) {
    return (filter == TextureFilter::Linear) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
}

MTLSamplerMipFilter MapMipFilter(TextureFilter filter) {
    return (filter == TextureFilter::Linear) ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
}

MTLSamplerAddressMode MapAddressMode(TextureAddressMode mode) {
    switch (mode) {
        case TextureAddressMode::Wrap:   return MTLSamplerAddressModeRepeat;
        case TextureAddressMode::Mirror: return MTLSamplerAddressModeMirrorRepeat;
        case TextureAddressMode::Clamp:  return MTLSamplerAddressModeClampToEdge;
        case TextureAddressMode::Border: return MTLSamplerAddressModeClampToBorderColor;
        default:                         return MTLSamplerAddressModeRepeat;
    }
}

MTLVertexFormat MapVertexFormat(VertexAttribType type) {
    switch (type) {
        case VertexAttribType::Float:  return MTLVertexFormatFloat;
        case VertexAttribType::Float2: return MTLVertexFormatFloat2;
        case VertexAttribType::Float3: return MTLVertexFormatFloat3;
        case VertexAttribType::Float4: return MTLVertexFormatFloat4;
        case VertexAttribType::UInt:   return MTLVertexFormatUInt;
        case VertexAttribType::UInt2:  return MTLVertexFormatUInt2;
        case VertexAttribType::UInt3:  return MTLVertexFormatUInt3;
        case VertexAttribType::UInt4:  return MTLVertexFormatUInt4;
        default: return MTLVertexFormatInvalid;
    }
}

MTLCompareFunction MapCompareFunc(ComparisonFunc func) {
    switch (func) {
        case ComparisonFunc::Never:        return MTLCompareFunctionNever;
        case ComparisonFunc::Less:         return MTLCompareFunctionLess;
        case ComparisonFunc::Equal:        return MTLCompareFunctionEqual;
        case ComparisonFunc::LessEqual:    return MTLCompareFunctionLessEqual;
        case ComparisonFunc::Greater:      return MTLCompareFunctionGreater;
        case ComparisonFunc::NotEqual:     return MTLCompareFunctionNotEqual;
        case ComparisonFunc::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case ComparisonFunc::Always:       return MTLCompareFunctionAlways;
    }
}

MTLStencilOperation MapStencilOp(StencilOp op) {
    switch (op) {
        case StencilOp::Keep:           return MTLStencilOperationKeep;
        case StencilOp::Zero:           return MTLStencilOperationZero;
        case StencilOp::Replace:        return MTLStencilOperationReplace;
        case StencilOp::IncrSat: return MTLStencilOperationIncrementClamp;
        case StencilOp::DecrSat: return MTLStencilOperationDecrementClamp;
        case StencilOp::Invert:         return MTLStencilOperationInvert;
        case StencilOp::Incr:  return MTLStencilOperationIncrementWrap;
        case StencilOp::Decr:  return MTLStencilOperationDecrementWrap;
    }
}

MTLBlendFactor MapBlendFactor(Blend factor) {
    switch (factor) {
        case Blend::Zero:           return MTLBlendFactorZero;
        case Blend::One:            return MTLBlendFactorOne;
        case Blend::SrcColor:       return MTLBlendFactorSourceColor;
        case Blend::InvSrcColor:    return MTLBlendFactorOneMinusSourceColor;
        case Blend::SrcAlpha:       return MTLBlendFactorSourceAlpha;
        case Blend::InvSrcAlpha:    return MTLBlendFactorOneMinusSourceAlpha;
        case Blend::DestAlpha:      return MTLBlendFactorDestinationAlpha;
        case Blend::InvDestAlpha:   return MTLBlendFactorOneMinusDestinationAlpha;
        case Blend::DestColor:      return MTLBlendFactorDestinationColor;
        case Blend::InvDestColor:   return MTLBlendFactorOneMinusDestinationColor;
        case Blend::SrcAlphaSat:    return MTLBlendFactorSourceAlphaSaturated;
        case Blend::BlendFactor:    return MTLBlendFactorBlendColor;
        case Blend::InvBlendFactor: return MTLBlendFactorOneMinusBlendColor;
            // Dual-source blending (requires specific pipeline settings)
        case Blend::Src1Color:      return MTLBlendFactorSource1Color;
        case Blend::InvSrc1Color:   return MTLBlendFactorOneMinusSource1Color;
        case Blend::Src1Alpha:      return MTLBlendFactorSource1Alpha;
        case Blend::InvSrc1Alpha:   return MTLBlendFactorOneMinusSource1Alpha;
        default:                    return MTLBlendFactorZero;
    }
}

MTLBlendOperation MapBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add:             return MTLBlendOperationAdd;
        case BlendOp::Subtract:        return MTLBlendOperationSubtract;
        case BlendOp::RevSubtract:     return MTLBlendOperationReverseSubtract;
        case BlendOp::Min:             return MTLBlendOperationMin;
        case BlendOp::Max:             return MTLBlendOperationMax;
    }
}