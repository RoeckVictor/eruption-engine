#include "engine/particles/ParticleRenderer.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/graphics/Texture.h"
#include "engine/render/Camera2D.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::particles {

bool ParticleRenderer::init() {
    if (!m_shader.load_graphics("shaders/particle.vert", "shaders/particle.frag")) {
        ENGINE_ERR("Failed to load particle shaders");
        return false;
    }

    // Create empty VAO (required by GL core profile for glDrawArrays)
    glGenVertexArrays(1, &m_vao);

    // Enable programmable point size
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Set constant uniforms
    m_shader.use();
    m_shader.set_int("u_palette", 0); // texture unit 0

    ENGINE_LOG("ParticleRenderer initialized");
    return true;
}

void ParticleRenderer::shutdown() {
    m_shader.destroy();
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

void ParticleRenderer::draw(ParticleBuffer& buffer,
                             const graphics::Texture& palette,
                             const render::Camera2D& camera,
                             float screen_w, float screen_h) {
    int max_p = buffer.max_particles();
    if (max_p == 0) return;

    // Enable blending for particles with alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.use();

    // Bind palette texture
    palette.bind(0);

    // Bind particle SSBO
    buffer.bind_particles();

    // Camera uniforms (same transform as grid renderer)
    m_shader.set_vec2("u_camera_pos", camera.x, camera.y);
    m_shader.set_vec2("u_screen_size", screen_w, screen_h);
    m_shader.set_float("u_zoom", camera.zoom);

    // Draw all particle slots (vertex shader skips dead ones)
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, max_p);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
}

} // namespace engine::particles
