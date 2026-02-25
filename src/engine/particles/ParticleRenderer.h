#pragma once

#include "engine/graphics/Shader.h"
#include <cstdint>

namespace engine::graphics { class Texture; }
namespace engine::render { struct Camera2D; }

namespace engine::particles {

class ParticleBuffer;

// Renders particles as GL_POINTS, reading positions from the particle SSBO.
// Uses the same camera transform and material palette as the grid renderer.
class ParticleRenderer {
public:
    bool init();
    void shutdown();

    void draw(ParticleBuffer& buffer,
              const graphics::Texture& palette,
              const render::Camera2D& camera,
              float screen_w, float screen_h,
              float grid_origin_x, float grid_origin_y, int grid_height);

private:
    graphics::Shader m_shader;
    uint32_t m_vao = 0;
};

}
