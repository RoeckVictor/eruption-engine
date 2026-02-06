#pragma once

#include "engine/graphics/Shader.h"
#include <cstdint>

namespace engine::graphics { class Texture; }
namespace engine::render { struct Camera2D; }

namespace engine::particles {

class ParticleBuffer;

/// Renders particles as GL_POINTS, reading positions from the particle SSBO.
/// Uses the same camera transform and material palette as the grid renderer.
class ParticleRenderer {
public:
    bool init();
    void shutdown();

    /// Draw all alive particles.
    /// @param buffer      The particle SSBO (bound at its binding point).
    /// @param palette     1D material color palette texture.
    /// @param camera      Current camera for world-to-screen transform.
    /// @param screen_w    Window width in pixels.
    /// @param screen_h    Window height in pixels.
    void draw(ParticleBuffer& buffer,
              const graphics::Texture& palette,
              const render::Camera2D& camera,
              float screen_w, float screen_h);

private:
    graphics::Shader m_shader;
    uint32_t m_vao = 0; // empty VAO (required by GL core profile)
};

} // namespace engine::particles
