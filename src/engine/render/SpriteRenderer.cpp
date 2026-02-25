#include "engine/render/SpriteRenderer.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include "engine/core/Log.h"

namespace engine::render {

SpriteRenderer::~SpriteRenderer() {
    shutdown();
}

SpriteRenderer::SpriteRenderer(SpriteRenderer&& other) noexcept
    : m_shader(std::move(other.m_shader))
    , m_pipeline(std::move(other.m_pipeline))
    , m_vbo(std::move(other.m_vbo))
    , m_ebo(std::move(other.m_ebo))
    , m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , m_vbo_capacity(other.m_vbo_capacity)
    , m_ebo_capacity(other.m_ebo_capacity)
    , m_cam_x(other.m_cam_x)
    , m_cam_y(other.m_cam_y)
    , m_zoom(other.m_zoom)
    , m_screen_w(other.m_screen_w)
    , m_screen_h(other.m_screen_h)
{
    other.m_vbo_capacity = 0;
    other.m_ebo_capacity = 0;
}

SpriteRenderer& SpriteRenderer::operator=(SpriteRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_shader = std::move(other.m_shader);
        m_pipeline = std::move(other.m_pipeline);
        m_vbo = std::move(other.m_vbo);
        m_ebo = std::move(other.m_ebo);
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_cam_x = other.m_cam_x;
        m_cam_y = other.m_cam_y;
        m_zoom = other.m_zoom;
        m_vbo_capacity = other.m_vbo_capacity;
        m_ebo_capacity = other.m_ebo_capacity;
        m_screen_w = other.m_screen_w;
        m_screen_h = other.m_screen_h;
        other.m_vbo_capacity = 0;
        other.m_ebo_capacity = 0;
    }
    return *this;
}

bool SpriteRenderer::init(const char* vert_path, const char* frag_path) {
    if (!m_shader.load_graphics(vert_path, frag_path)) {
        return false;
    }

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("SpriteRenderer::init - No RHI device available");
        return false;
    }

    // Create initial vertex and index buffers with reasonable capacity
    constexpr size_t INITIAL_VBO_SIZE = 4096 * sizeof(Vertex);  // ~4K vertices
    constexpr size_t INITIAL_EBO_SIZE = 4096 * 6 * sizeof(uint32_t);  // ~4K quads

    rhi::BufferDesc vbo_desc{};
    vbo_desc.type = rhi::BufferType::Vertex;
    vbo_desc.usage = rhi::BufferUsage::Dynamic;
    vbo_desc.size = INITIAL_VBO_SIZE;
    m_vbo = device->create_buffer(vbo_desc);
    if (!m_vbo || !m_vbo->valid()) {
        ENGINE_ERR("SpriteRenderer::init - Failed to create VBO");
        m_shader.destroy();
        return false;
    }
    m_vbo_capacity = INITIAL_VBO_SIZE;

    rhi::BufferDesc ebo_desc{};
    ebo_desc.type = rhi::BufferType::Index;
    ebo_desc.usage = rhi::BufferUsage::Dynamic;
    ebo_desc.size = INITIAL_EBO_SIZE;
    m_ebo = device->create_buffer(ebo_desc);
    if (!m_ebo || !m_ebo->valid()) {
        ENGINE_ERR("SpriteRenderer::init - Failed to create EBO");
        m_vbo.reset();
        m_shader.destroy();
        return false;
    }
    m_ebo_capacity = INITIAL_EBO_SIZE;

    // Create pipeline with vertex layout for sprite rendering
    rhi::VertexAttribute attrs[] = {
        { 0, rhi::VertexFormat::Float2, 0, 0 },  // Position
        { 1, rhi::VertexFormat::Float4, 8, 0 },  // Color (offset = 2 floats = 8 bytes)
    };
    rhi::VertexBinding bindings[] = {
        { 0, sizeof(Vertex), false },
    };

    rhi::PipelineDesc desc{};
    desc.shader = m_shader.rhi_shader();
    desc.topology = rhi::PrimitiveTopology::Triangles;
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

    // No depth test, no culling for 2D sprites
    desc.rasterizer.cull_mode = rhi::CullMode::None;

    m_pipeline = device->create_pipeline(desc);
    if (!m_pipeline || !m_pipeline->valid()) {
        ENGINE_ERR("SpriteRenderer::init - Failed to create pipeline");
        m_ebo.reset();
        m_vbo.reset();
        m_shader.destroy();
        return false;
    }

    ENGINE_LOG("Sprite renderer initialized");
    return true;
}

void SpriteRenderer::shutdown() {
    m_pipeline.reset();
    m_ebo.reset();
    m_vbo.reset();
    m_vbo_capacity = 0;
    m_ebo_capacity = 0;
    m_shader.destroy();
}

void SpriteRenderer::begin(float cam_x, float cam_y, float zoom,
                           int screen_w, int screen_h) {
    m_cam_x = cam_x;
    m_cam_y = cam_y;
    m_zoom = zoom;
    m_screen_w = screen_w;
    m_screen_h = screen_h;
    m_vertices.clear();
    m_indices.clear();
}

void SpriteRenderer::draw_rect(float min_x, float min_y, float max_x, float max_y,
                                float r, float g, float b, float a) {
    uint32_t base = static_cast<uint32_t>(m_vertices.size());

    m_vertices.push_back({ min_x, min_y, r, g, b, a });
    m_vertices.push_back({ max_x, min_y, r, g, b, a });
    m_vertices.push_back({ max_x, max_y, r, g, b, a });
    m_vertices.push_back({ min_x, max_y, r, g, b, a });

    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 0);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 3);
}

void SpriteRenderer::end() {
    if (m_indices.empty()) return;

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Update shader uniforms
    m_shader.use();
    m_shader.set_vec2("u_camera_pos", m_cam_x, m_cam_y);
    m_shader.set_vec2("u_screen_size", static_cast<float>(m_screen_w), static_cast<float>(m_screen_h));
    m_shader.set_float("u_zoom", m_zoom);

    // Update vertex buffer
    size_t vbo_size = m_vertices.size() * sizeof(Vertex);
    if (vbo_size > m_vbo_capacity) {
        m_vbo->resize(vbo_size, m_vertices.data());
        m_vbo_capacity = vbo_size;
    } else {
        m_vbo->update(0, vbo_size, m_vertices.data());
    }

    // Update index buffer
    size_t ebo_size = m_indices.size() * sizeof(uint32_t);
    if (ebo_size > m_ebo_capacity) {
        m_ebo->resize(ebo_size, m_indices.data());
        m_ebo_capacity = ebo_size;
    } else {
        m_ebo->update(0, ebo_size, m_indices.data());
    }

    // Bind pipeline and buffers
    ctx->bind_pipeline(m_pipeline.get());
    ctx->bind_vertex_buffer(m_vbo.get(), 0, 0);
    ctx->bind_index_buffer(m_ebo.get(), 4);  // 4 = uint32_t indices

    // Draw
    ctx->draw_indexed(static_cast<uint32_t>(m_indices.size()), 0, 0, 1);
}

} // namespace engine::render
