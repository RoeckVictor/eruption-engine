#pragma once

#include "engine/graphics/Shader.h"
#include "engine/rhi/RHIPipeline.h"
#include <cstdint>
#include <memory>

namespace engine::render { struct Camera2D; }

namespace engine::particles {

class ParticleBuffer;

// Renders particles as GL_POINTS, reading positions from the particle SSBO.
// Particles carry their own per-particle color (no palette lookup).
class ParticleRenderer {
public:
    bool init();
    void shutdown();

    void draw(ParticleBuffer& buffer,
              const render::Camera2D& camera,
              float screen_w, float screen_h,
              float grid_origin_x, float grid_origin_y, int grid_height);

private:
    graphics::Shader m_shader;
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
};

}
