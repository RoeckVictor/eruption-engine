#include "engine/render/TexturedSpriteRenderer.h"
#include "engine/graphics/Texture.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::render {

TexturedSpriteRenderer::~TexturedSpriteRenderer() {
    shutdown();
}

TexturedSpriteRenderer::TexturedSpriteRenderer(TexturedSpriteRenderer&& other) noexcept
    : m_shader(std::move(other.m_shader))
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , m_vbo_capacity(other.m_vbo_capacity)
    , m_ebo_capacity(other.m_ebo_capacity)
    , m_cam_x(other.m_cam_x)
    , m_cam_y(other.m_cam_y)
    , m_zoom(other.m_zoom)
    , m_screen_w(other.m_screen_w)
    , m_screen_h(other.m_screen_h)
    , m_bound_texture(other.m_bound_texture)
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_vbo_capacity = 0;
    other.m_ebo_capacity = 0;
    other.m_bound_texture = nullptr;
}

TexturedSpriteRenderer& TexturedSpriteRenderer::operator=(TexturedSpriteRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_shader = std::move(other.m_shader);
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_vbo_capacity = other.m_vbo_capacity;
        m_ebo_capacity = other.m_ebo_capacity;
        m_cam_x = other.m_cam_x;
        m_cam_y = other.m_cam_y;
        m_zoom = other.m_zoom;
        m_screen_w = other.m_screen_w;
        m_screen_h = other.m_screen_h;
        m_bound_texture = other.m_bound_texture;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_vbo_capacity = 0;
        other.m_ebo_capacity = 0;
        other.m_bound_texture = nullptr;
    }
    return *this;
}

bool TexturedSpriteRenderer::init(const char* vert_path, const char* frag_path) {
    if (!m_shader.load_graphics(vert_path, frag_path)) {
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

    // UV (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, u));

    // Color tint (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, r));

    glBindVertexArray(0);

    ENGINE_LOG("Textured sprite renderer initialized");
    return true;
}

void TexturedSpriteRenderer::shutdown() {
    if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_vbo_capacity = 0;
    m_ebo_capacity = 0;
    m_shader.destroy();
}

void TexturedSpriteRenderer::begin(const graphics::Texture& texture,
                                    float cam_x, float cam_y, float zoom,
                                    int screen_w, int screen_h) {
    m_bound_texture = &texture;
    m_cam_x = cam_x;
    m_cam_y = cam_y;
    m_zoom = zoom;
    m_screen_w = screen_w;
    m_screen_h = screen_h;
    m_vertices.clear();
    m_indices.clear();
}

void TexturedSpriteRenderer::draw_sprite(float min_x, float min_y,
                                          float max_x, float max_y,
                                          float u0, float v0, float u1, float v1,
                                          float r, float g, float b, float a) {
    uint32_t base = static_cast<uint32_t>(m_vertices.size());

    m_vertices.push_back({min_x, min_y, u0, v0, r, g, b, a});
    m_vertices.push_back({max_x, min_y, u1, v0, r, g, b, a});
    m_vertices.push_back({max_x, max_y, u1, v1, r, g, b, a});
    m_vertices.push_back({min_x, max_y, u0, v1, r, g, b, a});

    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 0);
    m_indices.push_back(base + 2);
    m_indices.push_back(base + 3);
}

void TexturedSpriteRenderer::end() {
    if (m_indices.empty() || !m_bound_texture) return;

    m_shader.use();
    m_shader.set_vec2("u_camera_pos", m_cam_x, m_cam_y);
    m_shader.set_vec2("u_screen_size", static_cast<float>(m_screen_w),
                      static_cast<float>(m_screen_h));
    m_shader.set_float("u_zoom", m_zoom);
    m_shader.set_int("u_texture", 0);

    m_bound_texture->bind(0);

    glBindVertexArray(m_vao);

    size_t vbo_size = m_vertices.size() * sizeof(Vertex);
    size_t ebo_size = m_indices.size() * sizeof(uint32_t);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (vbo_size > m_vbo_capacity) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vbo_size),
                     m_vertices.data(), GL_DYNAMIC_DRAW);
        m_vbo_capacity = vbo_size;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vbo_size),
                        m_vertices.data());
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    if (ebo_size > m_ebo_capacity) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(ebo_size),
                     m_indices.data(), GL_DYNAMIC_DRAW);
        m_ebo_capacity = ebo_size;
    } else {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(ebo_size),
                        m_indices.data());
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                   GL_UNSIGNED_INT, nullptr);

    glDisable(GL_BLEND);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_bound_texture = nullptr;
}

} // namespace engine::render
