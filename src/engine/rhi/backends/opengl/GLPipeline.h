#pragma once

#include "engine/rhi/RHIPipeline.h"
#include <cstdint>
#include <vector>

namespace engine::rhi {

class GLShader;

// OpenGL implementation of RHIPipeline
// In OpenGL, there's no explicit pipeline state object like in Vulkan/D3D12
// This class manages VAO configuration and state settings
class GLPipeline : public RHIPipeline {
public:
    GLPipeline() = default;
    ~GLPipeline() override;

    GLPipeline(GLPipeline&& other) noexcept;
    GLPipeline& operator=(GLPipeline&& other) noexcept;

    bool init(const PipelineDesc& desc);
    void destroy();

    void bind() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_vao)); }

    uint32_t vao() const { return m_vao; }

    PrimitiveTopology primitive_type() const { return m_topology; }

    uint32_t vertex_stride() const { return m_vertex_stride; }

private:
    void apply_blend_state(const BlendState& state);
    void apply_depth_stencil_state(const DepthStencilState& state);
    void apply_rasterizer_state(const RasterizerState& state);

    uint32_t m_vao = 0;
    GLShader* m_gl_shader = nullptr;

    BlendState m_blend;
    DepthStencilState m_depth_stencil;
    RasterizerState m_rasterizer;
    PrimitiveTopology m_topology = PrimitiveTopology::Triangles;
    uint32_t m_vertex_stride = 0;
};

}
