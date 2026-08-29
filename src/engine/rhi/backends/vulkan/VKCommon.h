#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstring>
#include "engine/rhi/RHITypes.h"
#include "engine/rhi/RHIDescriptorSet.h"
#include "engine/core/Log.h"

namespace engine::rhi {

// Check a VkResult and log on failure. Use via VK_CHECK macro.
inline bool vk_check_result(VkResult result, const char* expr, const char* file, int line) {
    if (result != VK_SUCCESS) {
        ENGINE_ERR("Vulkan error %d at %s:%d: %s", static_cast<int>(result), file, line, expr);
        return false;
    }
    return true;
}

#define VK_CHECK(expr) ::engine::rhi::vk_check_result((expr), #expr, __FILE__, __LINE__)

// ---- RHI enum → Vulkan format conversions ----

inline VkFormat to_vk_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:              return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:             return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:            return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::RGBA8:           return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R8UI:            return VK_FORMAT_R8_UINT;
        case TextureFormat::RG8UI:           return VK_FORMAT_R8G8_UINT;
        case TextureFormat::RGB8UI:          return VK_FORMAT_R8G8B8_UINT;
        case TextureFormat::RGBA8UI:         return VK_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::R16F:            return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16F:           return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGB16F:          return VK_FORMAT_R16G16B16_SFLOAT;
        case TextureFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32F:            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32F:          return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth16:         return VK_FORMAT_D16_UNORM;
        case TextureFormat::Depth24:         return VK_FORMAT_X8_D24_UNORM_PACK32;
        case TextureFormat::Depth32F:        return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::Depth32FStencil8:return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:                             return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

inline VkFormat to_vk_vertex_format(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:      return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::Float2:     return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3:     return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float4:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::Int:        return VK_FORMAT_R32_SINT;
        case VertexFormat::Int2:       return VK_FORMAT_R32G32_SINT;
        case VertexFormat::Int3:       return VK_FORMAT_R32G32B32_SINT;
        case VertexFormat::Int4:       return VK_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::UInt:       return VK_FORMAT_R32_UINT;
        case VertexFormat::UInt2:      return VK_FORMAT_R32G32_UINT;
        case VertexFormat::UInt3:      return VK_FORMAT_R32G32B32_UINT;
        case VertexFormat::UInt4:      return VK_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::Byte4Norm:  return VK_FORMAT_R8G8B8A8_SNORM;
        case VertexFormat::UByte4Norm: return VK_FORMAT_R8G8B8A8_UNORM;
        default:                       return VK_FORMAT_R32G32B32_SFLOAT;
    }
}

inline VkPrimitiveTopology to_vk_topology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::Points:        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::Lines:         return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::Triangles:     return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::TriangleFan:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:                               return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

inline VkCullModeFlags to_vk_cull_mode(CullMode mode) {
    switch (mode) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default:              return VK_CULL_MODE_BACK_BIT;
    }
}

inline VkFrontFace to_vk_front_face(FrontFace face) {
    switch (face) {
        case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
        default:                          return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

inline VkBlendFactor to_vk_blend_factor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:             return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:              return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        default:                            return VK_BLEND_FACTOR_ONE;
    }
}

inline VkBlendOp to_vk_blend_op(BlendOp op) {
    switch (op) {
        case BlendOp::Add:             return VK_BLEND_OP_ADD;
        case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min:             return VK_BLEND_OP_MIN;
        case BlendOp::Max:             return VK_BLEND_OP_MAX;
        default:                       return VK_BLEND_OP_ADD;
    }
}

inline VkCompareOp to_vk_compare_op(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return VK_COMPARE_OP_NEVER;
        case CompareFunc::Less:         return VK_COMPARE_OP_LESS;
        case CompareFunc::Equal:        return VK_COMPARE_OP_EQUAL;
        case CompareFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunc::Greater:      return VK_COMPARE_OP_GREATER;
        case CompareFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareFunc::Always:       return VK_COMPARE_OP_ALWAYS;
        default:                        return VK_COMPARE_OP_LESS;
    }
}

inline VkFilter to_vk_filter(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        case TextureFilter::Linear:  return VK_FILTER_LINEAR;
        default:                     return VK_FILTER_NEAREST;
    }
}

inline VkSamplerAddressMode to_vk_address_mode(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Clamp:         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrap::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrap::Mirror:        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureWrap::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:                         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

inline VkIndexType to_vk_index_type(IndexType type) {
    switch (type) {
        case IndexType::UInt16: return VK_INDEX_TYPE_UINT16;
        case IndexType::UInt32: return VK_INDEX_TYPE_UINT32;
        default:                return VK_INDEX_TYPE_UINT32;
    }
}

inline VkImageType to_vk_image_type(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::Tex1D:   return VK_IMAGE_TYPE_1D;
        case TextureDimension::Tex2D:
        case TextureDimension::TexCube: return VK_IMAGE_TYPE_2D;
        case TextureDimension::Tex3D:   return VK_IMAGE_TYPE_3D;
        default:                        return VK_IMAGE_TYPE_2D;
    }
}

inline VkImageViewType to_vk_image_view_type(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::Tex1D:   return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::Tex2D:   return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::Tex3D:   return VK_IMAGE_VIEW_TYPE_3D;
        case TextureDimension::TexCube: return VK_IMAGE_VIEW_TYPE_CUBE;
        default:                        return VK_IMAGE_VIEW_TYPE_2D;
    }
}

inline VkImageAspectFlags vk_aspect_from_format(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

inline VkDescriptorType to_vk_descriptor_type(DescriptorType type) {
    switch (type) {
        case DescriptorType::UniformBuffer:       return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:       return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::Texture:             return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::StorageImage:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::Sampler:             return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::CombinedImageSampler:return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        default:                                  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

inline TextureFormat from_vk_depth_format(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:           return TextureFormat::Depth16;
        case VK_FORMAT_X8_D24_UNORM_PACK32: return TextureFormat::Depth24;
        case VK_FORMAT_D32_SFLOAT:          return TextureFormat::Depth32F;
        case VK_FORMAT_D24_UNORM_S8_UINT:   return TextureFormat::Depth24Stencil8;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:  return TextureFormat::Depth32FStencil8;
        default:                            return TextureFormat::Depth32F;
    }
}

// ---- Shared barrier construction from RHI BarrierFlags ----
// Used by both VKContext and VKCommandBuffer to avoid duplicated logic.

struct VkBarrierInfo {
    VkAccessFlags src_access = 0;
    VkAccessFlags dst_access = 0;
    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;
};

inline VkBarrierInfo build_barrier_from_flags(BarrierFlags flags) {
    VkBarrierInfo info = {};

    if (has_flag(flags, BarrierFlags::StorageBuffer) || has_flag(flags, BarrierFlags::ImageAccess)) {
        info.src_access |= VK_ACCESS_SHADER_WRITE_BIT;
        info.dst_access |= VK_ACCESS_SHADER_READ_BIT;
        info.src_stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (has_flag(flags, BarrierFlags::VertexBuffer)) {
        info.dst_access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if (has_flag(flags, BarrierFlags::IndexBuffer)) {
        info.dst_access |= VK_ACCESS_INDEX_READ_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if (has_flag(flags, BarrierFlags::UniformBuffer)) {
        info.dst_access |= VK_ACCESS_UNIFORM_READ_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (has_flag(flags, BarrierFlags::TextureRead)) {
        info.dst_access |= VK_ACCESS_SHADER_READ_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (has_flag(flags, BarrierFlags::TextureWrite)) {
        info.src_access |= VK_ACCESS_SHADER_WRITE_BIT;
        info.src_stage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if (has_flag(flags, BarrierFlags::Framebuffer)) {
        info.src_access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        info.dst_access |= VK_ACCESS_SHADER_READ_BIT;
        info.src_stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        info.dst_stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    if (info.src_stage == 0) info.src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (info.dst_stage == 0) info.dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    return info;
}

// ---- Shared descriptor binding state ----
// Used by both VKContext and VKCommandBuffer to avoid duplicated flush logic.

class VKDevice;
class VKShader;
class VKTexture;

struct VKDescriptorState {
    static constexpr uint32_t MAX_SLOTS = 16;

    VkBuffer ubos[MAX_SLOTS] = {};
    VkDeviceSize ubo_sizes[MAX_SLOTS] = {};
    VkBuffer ssbos[MAX_SLOTS] = {};
    VkDeviceSize ssbo_sizes[MAX_SLOTS] = {};
    VkImageView images[MAX_SLOTS] = {};
    const VKTexture* samplers[MAX_SLOTS] = {};
    bool dirty = false;

    // Pre-allocated write buffers (reused each flush — avoids per-draw heap allocation)
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkDescriptorImageInfo> image_infos;

    void clear() {
        memset(ubos, 0, sizeof(ubos));
        memset(ubo_sizes, 0, sizeof(ubo_sizes));
        memset(ssbos, 0, sizeof(ssbos));
        memset(ssbo_sizes, 0, sizeof(ssbo_sizes));
        memset(images, 0, sizeof(images));
        memset(samplers, 0, sizeof(samplers));
        dirty = false;
    }

    // Build descriptor writes, allocate a frame descriptor set, update and bind it.
    // Uses reflected binding numbers (not sequential indices) for correct matching.
    // Returns true if descriptors were written and bound.
    bool flush(VKDevice* device, VkCommandBuffer cmd,
               VKShader* shader, VkPipelineLayout layout, bool is_compute);
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
