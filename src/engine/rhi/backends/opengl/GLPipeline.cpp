#include "GLPipeline.h"
#include "GLShader.h"
#include <glad/gl.h>

namespace engine::rhi {

namespace {

GLenum topology_to_gl(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::Points:        return GL_POINTS;
        case PrimitiveTopology::Lines:         return GL_LINES;
        case PrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
        case PrimitiveTopology::Triangles:     return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::TriangleFan:   return GL_TRIANGLE_FAN;
        default: return GL_TRIANGLES;
    }
}

GLenum blend_factor_to_gl(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:             return GL_ZERO;
        case BlendFactor::One:              return GL_ONE;
        case BlendFactor::SrcColor:         return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        default: return GL_ONE;
    }
}

GLenum blend_op_to_gl(BlendOp op) {
    switch (op) {
        case BlendOp::Add:             return GL_FUNC_ADD;
        case BlendOp::Subtract:        return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:             return GL_MIN;
        case BlendOp::Max:             return GL_MAX;
        default: return GL_FUNC_ADD;
    }
}

GLenum compare_func_to_gl(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return GL_NEVER;
        case CompareFunc::Less:         return GL_LESS;
        case CompareFunc::Equal:        return GL_EQUAL;
        case CompareFunc::LessEqual:    return GL_LEQUAL;
        case CompareFunc::Greater:      return GL_GREATER;
        case CompareFunc::NotEqual:     return GL_NOTEQUAL;
        case CompareFunc::GreaterEqual: return GL_GEQUAL;
        case CompareFunc::Always:       return GL_ALWAYS;
        default: return GL_LESS;
    }
}

GLenum vertex_format_to_gl_type(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return GL_FLOAT;
        case VertexFormat::Int:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
            return GL_INT;
        case VertexFormat::UInt:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
            return GL_UNSIGNED_INT;
        case VertexFormat::Byte4Norm:
            return GL_BYTE;
        case VertexFormat::UByte4Norm:
            return GL_UNSIGNED_BYTE;
        default:
            return GL_FLOAT;
    }
}

int vertex_format_component_count(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Int:
        case VertexFormat::UInt:
            return 1;
        case VertexFormat::Float2:
        case VertexFormat::Int2:
        case VertexFormat::UInt2:
            return 2;
        case VertexFormat::Float3:
        case VertexFormat::Int3:
        case VertexFormat::UInt3:
            return 3;
        case VertexFormat::Float4:
        case VertexFormat::Int4:
        case VertexFormat::UInt4:
        case VertexFormat::Byte4Norm:
        case VertexFormat::UByte4Norm:
            return 4;
        default:
            return 4;
    }
}

bool vertex_format_normalized(VertexFormat format) {
    return format == VertexFormat::Byte4Norm || format == VertexFormat::UByte4Norm;
}

} // anonymous namespace

GLPipeline::~GLPipeline() {
    destroy();
}

GLPipeline::GLPipeline(GLPipeline&& other) noexcept
    : RHIPipeline()
{
    m_vao = other.m_vao;
    m_gl_shader = other.m_gl_shader;
    m_shader = other.m_shader;
    m_blend = other.m_blend;
    m_depth_stencil = other.m_depth_stencil;
    m_rasterizer = other.m_rasterizer;
    m_topology = other.m_topology;
    m_vertex_stride = other.m_vertex_stride;
    m_valid = other.m_valid;
    m_is_compute = other.m_is_compute;

    other.m_vao = 0;
    other.m_valid = false;
}

GLPipeline& GLPipeline::operator=(GLPipeline&& other) noexcept {
    if (this != &other) {
        destroy();

        m_vao = other.m_vao;
        m_gl_shader = other.m_gl_shader;
        m_shader = other.m_shader;
        m_blend = other.m_blend;
        m_depth_stencil = other.m_depth_stencil;
        m_rasterizer = other.m_rasterizer;
        m_topology = other.m_topology;
        m_vertex_stride = other.m_vertex_stride;
        m_valid = other.m_valid;
        m_is_compute = other.m_is_compute;

        other.m_vao = 0;
        other.m_valid = false;
    }
    return *this;
}

bool GLPipeline::init(const PipelineDesc& desc) {
    // Shader is optional - allows creating VAO-only pipelines for utility passes
    // where the shader is bound separately (e.g., fullscreen passes)
    if (desc.shader) {
        m_shader = desc.shader;
        m_gl_shader = static_cast<GLShader*>(desc.shader);
        m_is_compute = m_gl_shader->is_compute();

        // Compute pipelines don't need a VAO
        if (m_is_compute) {
            m_valid = true;
            return true;
        }
    }

    // Store state for later application
    m_blend = desc.blend;
    m_depth_stencil = desc.depth_stencil;
    m_rasterizer = desc.rasterizer;
    m_topology = desc.topology;

    // Store vertex stride from first binding (for bind_vertex_buffer)
    if (desc.binding_count > 0) {
        m_vertex_stride = desc.bindings[0].stride;
    }

    // Create VAO and configure vertex attributes
    glGenVertexArrays(1, &m_vao);
    if (m_vao == 0) {
        return false;
    }

    glBindVertexArray(m_vao);

    // Configure vertex attributes
    for (uint32_t i = 0; i < desc.attribute_count; ++i) {
        const auto& attr = desc.attributes[i];

        glEnableVertexAttribArray(attr.location);

        GLenum type = vertex_format_to_gl_type(attr.format);
        int count = vertex_format_component_count(attr.format);
        bool normalized = vertex_format_normalized(attr.format);

        // Find the binding for this attribute
        uint32_t stride = 0;
        for (uint32_t j = 0; j < desc.binding_count; ++j) {
            if (desc.bindings[j].binding == attr.binding) {
                stride = desc.bindings[j].stride;
                break;
            }
        }

        if (type == GL_INT || type == GL_UNSIGNED_INT) {
            glVertexAttribIPointer(attr.location, count, type, stride,
                                   reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset)));
        } else {
            glVertexAttribPointer(attr.location, count, type, normalized ? GL_TRUE : GL_FALSE,
                                  stride, reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset)));
        }

        glVertexAttribBinding(attr.location, attr.binding);
    }

    // Configure per-instance bindings
    for (uint32_t i = 0; i < desc.binding_count; ++i) {
        if (desc.bindings[i].per_instance) {
            glVertexBindingDivisor(desc.bindings[i].binding, 1);
        }
    }

    glBindVertexArray(0);

    m_valid = true;
    return true;
}

void GLPipeline::destroy() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_valid = false;
}

void GLPipeline::bind() {
    if (!m_valid) return;

    if (m_gl_shader) {
        m_gl_shader->bind();
    }

    if (m_is_compute) {
        return;  // Compute pipelines don't need VAO or state
    }

    if (m_vao) {
        glBindVertexArray(m_vao);
    }

    apply_blend_state(m_blend);
    apply_depth_stencil_state(m_depth_stencil);
    apply_rasterizer_state(m_rasterizer);
}

void GLPipeline::apply_blend_state(const BlendState& state) {
    if (state.enabled) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(
            blend_factor_to_gl(state.src_color),
            blend_factor_to_gl(state.dst_color),
            blend_factor_to_gl(state.src_alpha),
            blend_factor_to_gl(state.dst_alpha)
        );
        glBlendEquationSeparate(
            blend_op_to_gl(state.color_op),
            blend_op_to_gl(state.alpha_op)
        );
    } else {
        glDisable(GL_BLEND);
    }
}

void GLPipeline::apply_depth_stencil_state(const DepthStencilState& state) {
    if (state.depth_test) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(compare_func_to_gl(state.depth_func));
        glDepthMask(state.depth_write ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (state.stencil_test) {
        glEnable(GL_STENCIL_TEST);
    } else {
        glDisable(GL_STENCIL_TEST);
    }
}

void GLPipeline::apply_rasterizer_state(const RasterizerState& state) {
    switch (state.cull_mode) {
        case CullMode::None:
            glDisable(GL_CULL_FACE);
            break;
        case CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case CullMode::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
    }

    glFrontFace(state.front_face == FrontFace::CounterClockwise ? GL_CCW : GL_CW);

    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);

    if (state.scissor_test) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    if (state.program_point_size) {
        glEnable(GL_PROGRAM_POINT_SIZE);
    } else {
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
}

} // namespace engine::rhi
