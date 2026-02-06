#include "engine/render/SpriteRenderer.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::render {

SpriteRenderer::~SpriteRenderer() {
    shutdown();
}

SpriteRenderer::SpriteRenderer(SpriteRenderer&& other) noexcept
    : m_shader(std::move(other.m_shader))
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , m_cam_x(other.m_cam_x)
    , m_cam_y(other.m_cam_y)
    , m_zoom(other.m_zoom)
    , m_screen_w(other.m_screen_w)
    , m_screen_h(other.m_screen_h)
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
}

SpriteRenderer& SpriteRenderer::operator=(SpriteRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_shader = std::move(other.m_shader);
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_cam_x = other.m_cam_x;
        m_cam_y = other.m_cam_y;
        m_zoom = other.m_zoom;
        m_screen_w = other.m_screen_w;
        m_screen_h = other.m_screen_h;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
    }
    return *this;
}

bool SpriteRenderer::init(const char* vert_path, const char* frag_path) {
    if (!m_shader.load_graphics(vert_path, frag_path)) {
        ENGINE_ERR("Failed to load sprite shaders");
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    // Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, x));

    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, r));

    glBindVertexArray(0);

    ENGINE_LOG("Sprite renderer initialized");
    return true;
}

void SpriteRenderer::shutdown() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
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
    uint32_t base = (uint32_t)m_vertices.size();

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

    m_shader.use();
    m_shader.set_vec2("u_camera_pos", m_cam_x, m_cam_y);
    m_shader.set_vec2("u_screen_size", (float)m_screen_w, (float)m_screen_h);
    m_shader.set_float("u_zoom", m_zoom);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(m_indices.size() * sizeof(uint32_t)),
                 m_indices.data(), GL_DYNAMIC_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, nullptr);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

} // namespace engine::render
