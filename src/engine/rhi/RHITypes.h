#pragma once

#include <cstdint>
#include <cstddef>

namespace engine::rhi {

enum class Backend {
    OpenGL,
    Vulkan,
    D3D12,
    Metal,
};

enum class BufferType {
    Vertex,
    Index,
    Uniform,
    Storage,
    PixelPack,
};

enum class BufferUsage {
    Static,
    Dynamic,
    Stream,
};

struct BufferDesc {
    BufferType type = BufferType::Vertex;
    BufferUsage usage = BufferUsage::Static;
    size_t size = 0;
    const void* initial_data = nullptr;
};

enum class TextureFormat {
    R8,
    RG8,
    RGB8,
    RGBA8,

    R8UI,
    RG8UI,
    RGB8UI,
    RGBA8UI,

    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,

    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
    Depth32FStencil8,
};

enum class TextureDimension {
    Tex1D,
    Tex2D,
    Tex3D,
    TexCube,
};

enum class TextureFilter {
    Nearest,
    Linear,
};

enum class TextureWrap {
    Clamp,
    Repeat,
    Mirror,
    ClampToBorder,
};

enum class ImageAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

enum class BufferAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

enum class IndexType {
    UInt16,
    UInt32,
};

enum class TextureUsageFlags : uint32_t {
    None           = 0,
    Sampled        = 1 << 0,  // Can be sampled in shaders
    Storage        = 1 << 1,  // Can be used as a storage image
    ColorAttachment= 1 << 2,  // Can be used as a color attachment
    TransferSrc    = 1 << 3,  // Can be used as a transfer source
    TransferDst    = 1 << 4,  // Can be used as a transfer destination
    // Convenience: default covers the most common case
    Default        = Sampled | TransferDst,
};

inline TextureUsageFlags operator|(TextureUsageFlags a, TextureUsageFlags b) {
    return static_cast<TextureUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline TextureUsageFlags operator&(TextureUsageFlags a, TextureUsageFlags b) {
    return static_cast<TextureUsageFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_usage(TextureUsageFlags flags, TextureUsageFlags flag) {
    uint32_t f = static_cast<uint32_t>(flag);
    return (static_cast<uint32_t>(flags) & f) == f;
}

struct TextureDesc {
    int width = 1;
    int height = 1;
    int depth = 1;
    TextureDimension dimension = TextureDimension::Tex2D;
    TextureFormat format = TextureFormat::RGBA8;
    TextureFilter min_filter = TextureFilter::Nearest;
    TextureFilter mag_filter = TextureFilter::Nearest;
    TextureWrap wrap_u = TextureWrap::Clamp;
    TextureWrap wrap_v = TextureWrap::Clamp;
    TextureWrap wrap_w = TextureWrap::Clamp;
    bool generate_mipmaps = false;
    const void* initial_data = nullptr;
    TextureUsageFlags usage = TextureUsageFlags::Default;
};

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation,
};

struct ShaderStageDesc {
    ShaderStage stage;
    const char* source_path = nullptr;
    const char* source_code = nullptr;
    const char* entry_point = "main";
    const uint32_t* spirv_code = nullptr;  // Precompiled SPIR-V bytecode (Vulkan)
    size_t spirv_size = 0;                 // Size of SPIR-V bytecode in bytes
};

struct ShaderDesc {
    const ShaderStageDesc* stages = nullptr;
    uint32_t stage_count = 0;
};

enum class PrimitiveTopology {
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan,
};

enum class CullMode {
    None,
    Front,
    Back,
};

enum class FrontFace {
    CounterClockwise,
    Clockwise,
};

enum class BlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
};

enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class CompareFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class VertexFormat {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt,
    UInt2,
    UInt3,
    UInt4,
    Byte4Norm,
    UByte4Norm,
};

struct VertexAttribute {
    uint32_t location = 0;
    VertexFormat format = VertexFormat::Float3;
    uint32_t offset = 0;
    uint32_t binding = 0;
};

struct VertexBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;
    bool per_instance = false;
};

struct BlendState {
    bool enabled = false;
    BlendFactor src_color = BlendFactor::SrcAlpha;
    BlendFactor dst_color = BlendFactor::OneMinusSrcAlpha;
    BlendOp color_op = BlendOp::Add;
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::OneMinusSrcAlpha;
    BlendOp alpha_op = BlendOp::Add;
};

struct DepthStencilState {
    bool depth_test = false;
    bool depth_write = true;
    CompareFunc depth_func = CompareFunc::Less;
    bool stencil_test = false;
};

struct RasterizerState {
    CullMode cull_mode = CullMode::Back;
    FrontFace front_face = FrontFace::CounterClockwise;
    bool wireframe = false;
    bool scissor_test = false;
    bool program_point_size = false;
};

struct PipelineDesc {
    class RHIShader* shader = nullptr;

    const VertexAttribute* attributes = nullptr;
    uint32_t attribute_count = 0;
    const VertexBinding* bindings = nullptr;
    uint32_t binding_count = 0;

    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    BlendState blend;
    DepthStencilState depth_stencil;
    RasterizerState rasterizer;

    // Vulkan-specific: render pass to create pipeline against.
    // If null, the swapchain render pass is used as default.
    void* render_pass_handle = nullptr;
};

struct FramebufferAttachment {
    class RHITexture* texture = nullptr;
    int mip_level = 0;
    int layer = 0;
};

struct FramebufferDesc {
    const FramebufferAttachment* color_attachments = nullptr;
    uint32_t color_attachment_count = 0;
    FramebufferAttachment depth_stencil_attachment;
    int width = 0;
    int height = 0;
};

enum class BarrierFlags : uint32_t {
    None = 0,
    VertexBuffer = 1 << 0,
    IndexBuffer = 1 << 1,
    UniformBuffer = 1 << 2,
    StorageBuffer = 1 << 3,
    TextureRead = 1 << 4,
    TextureWrite = 1 << 5,
    ImageAccess = 1 << 6,
    Framebuffer = 1 << 7,
    All = 0xFFFFFFFF,
};

inline BarrierFlags operator|(BarrierFlags a, BarrierFlags b) {
    return static_cast<BarrierFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BarrierFlags operator&(BarrierFlags a, BarrierFlags b) {
    return static_cast<BarrierFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(BarrierFlags flags, BarrierFlags flag) {
    uint32_t f = static_cast<uint32_t>(flag);
    return (static_cast<uint32_t>(flags) & f) == f;
}

inline uint32_t vertex_format_size(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:      return 4;
        case VertexFormat::Float2:     return 8;
        case VertexFormat::Float3:     return 12;
        case VertexFormat::Float4:     return 16;
        case VertexFormat::Int:        return 4;
        case VertexFormat::Int2:       return 8;
        case VertexFormat::Int3:       return 12;
        case VertexFormat::Int4:       return 16;
        case VertexFormat::UInt:       return 4;
        case VertexFormat::UInt2:      return 8;
        case VertexFormat::UInt3:      return 12;
        case VertexFormat::UInt4:      return 16;
        case VertexFormat::Byte4Norm:  return 4;
        case VertexFormat::UByte4Norm: return 4;
        default: return 0;
    }
}

inline uint32_t texture_format_bpp(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::R8UI:
            return 1;
        case TextureFormat::RG8:
        case TextureFormat::RG8UI:
        case TextureFormat::R16F:
        case TextureFormat::Depth16:
            return 2;
        case TextureFormat::RGB8:
        case TextureFormat::RGB8UI:
        case TextureFormat::Depth24:
            return 3;
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA8UI:
        case TextureFormat::RG16F:
        case TextureFormat::R32F:
        case TextureFormat::Depth32F:
        case TextureFormat::Depth24Stencil8:
            return 4;
        case TextureFormat::Depth32FStencil8:
            return 5;
        case TextureFormat::RGB16F:
            return 6;
        case TextureFormat::RGBA16F:
        case TextureFormat::RG32F:
            return 8;
        case TextureFormat::RGB32F:
            return 12;
        case TextureFormat::RGBA32F:
            return 16;
        default:
            return 4;
    }
}

inline bool is_depth_format(TextureFormat format) {
    switch (format) {
        case TextureFormat::Depth16:
        case TextureFormat::Depth24:
        case TextureFormat::Depth32F:
        case TextureFormat::Depth24Stencil8:
        case TextureFormat::Depth32FStencil8:
            return true;
        default:
            return false;
    }
}

inline bool has_stencil(TextureFormat format) {
    return format == TextureFormat::Depth24Stencil8 ||
           format == TextureFormat::Depth32FStencil8;
}

}
