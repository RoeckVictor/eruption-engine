#include "engine/render/DebugRenderer.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::render {

DebugRenderer::~DebugRenderer() {
    shutdown();
}

bool DebugRenderer::init() {
    // Reuse the sprite shaders (same vertex format: position + color, same camera uniforms)
    if (!m_shader.load_graphics("shaders/sprite.vert", "shaders/sprite.frag")) {
        ENGINE_ERR("DebugRenderer: Failed to load shaders");
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, x));

    // Color (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, r));

    glBindVertexArray(0);

    ENGINE_LOG("Debug renderer initialized");
    return true;
}

void DebugRenderer::shutdown() {
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
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

    m_shader.use();
    m_shader.set_vec2("u_camera_pos", m_cam_x, m_cam_y);
    m_shader.set_vec2("u_screen_size", (float)m_screen_w, (float)m_screen_h);
    m_shader.set_float("u_zoom", m_zoom);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(m_vertices.size() * sizeof(Vertex)),
                 m_vertices.data(), GL_DYNAMIC_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_LINES, 0, (GLsizei)m_vertices.size());

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

} // namespace engine::render
