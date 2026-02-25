#include "engine/render/DebugRenderer.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include "engine/core/Log.h"

namespace engine::render {

DebugRenderer::~DebugRenderer() {
    shutdown();
}

bool DebugRenderer::init() {
    // Reuse the sprite shaders (same vertex format: position + color, same camera uniforms)
    if (!m_shader.load_graphics("shaders/sprite.vert", "shaders/sprite.frag")) {
        return false;
    }

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("DebugRenderer::init - No RHI device available");
        return false;
    }

    // Create initial vertex buffer
    constexpr size_t INITIAL_VBO_SIZE = 4096 * sizeof(Vertex);

    rhi::BufferDesc vbo_desc{};
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Stream;  // Frequent updates
    vbo_desc.size = INITIAL_VBO_SIZE;
    m_vbo = device->create_buffer(vbo_desc);
    if (!m_vbo || !m_vbo->valid()) {
        ENGINE_ERR("DebugRenderer::init - Failed to create VBO");
        m_shader.destroy();
        return false;
    }
    m_vbo_capacity = INITIAL_VBO_SIZE;

    // Create pipeline with lines topology
    rhi::VertexAttribute attrs[] = {
        { 0, rhi::VertexFormat::Float2, 0, 0 },  // Position
        { 1, rhi::VertexFormat::Float4, 8, 0 },  // Color
    };
    rhi::VertexBinding bindings[] = {
        { 0, sizeof(Vertex), false },
    };

    rhi::PipelineDesc desc{};
    desc.shader = m_shader.rhi_shader();
    desc.topology = rhi::PrimitiveTopology::Lines;
    desc.attributes = attrs;
    desc.attribute_count = 2;
    desc.bindings = bindings;
    desc.binding_count = 1;

    // Enable alpha blending
    desc.blend.enabled = true;
    desc.blend.src_color = rhi::BlendFactor::SrcAlpha;
    desc.blend.dst_color = rhi::BlendFactor::OneMinusSrcAlpha;
    desc.blend.src_alpha = rhi::BlendFactor::One;
    desc.blend.dst_alpha = rhi::BlendFactor::OneMinusSrcAlpha;

    // No depth test, no culling for debug lines
    desc.rasterizer.cull_mode = rhi::CullMode::None;

    m_pipeline = device->create_pipeline(desc);
    if (!m_pipeline || !m_pipeline->valid()) {
        ENGINE_ERR("DebugRenderer::init - Failed to create pipeline");
        m_vbo.reset();
        m_shader.destroy();
        return false;
    }

    ENGINE_LOG("Debug renderer initialized");
    return true;
}

void DebugRenderer::shutdown() {
    m_pipeline.reset();
    m_vbo.reset();
    m_vbo_capacity = 0;
    m_shader.destroy();
}

void DebugRenderer::begin(float cam_x, float cam_y, float zoom,
                           int screen_w, int screen_h) {
    m_cam_x = cam_x;
    m_cam_y = cam_y;
    m_zoom = zoom;
    m_screen_w = screen_w;
    m_screen_h = screen_h;
    m_vertices.clear();
}

void DebugRenderer::draw_line(float x0, float y0, float x1, float y1,
                               float r, float g, float b, float a) {
    m_vertices.push_back({x0, y0, r, g, b, a});
    m_vertices.push_back({x1, y1, r, g, b, a});
}

void DebugRenderer::end() {
    if (m_vertices.empty()) return;

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Update shader uniforms
    m_shader.use();
    m_shader.set_vec2("u_camera_pos", m_cam_x, m_cam_y);
    m_shader.set_vec2("u_screen_size", static_cast<float>(m_screen_w), static_cast<float>(m_screen_h));
    m_shader.set_float("u_zoom", m_zoom);

    // Update vertex buffer
    size_t data_size = m_vertices.size() * sizeof(Vertex);
    if (data_size > m_vbo_capacity) {
        // Grow buffer with headroom to avoid frequent reallocations
        m_vbo_capacity = data_size + data_size / 2;
        m_vbo->resize(m_vbo_capacity, nullptr);
    }
    m_vbo->update(0, data_size, m_vertices.data());

    // Bind pipeline and buffer, then draw
    ctx->bind_pipeline(m_pipeline.get());
    ctx->bind_vertex_buffer(m_vbo.get(), 0, 0);
    ctx->draw(static_cast<uint32_t>(m_vertices.size()), 0, 1);
}

} // namespace engine::render
