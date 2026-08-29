#pragma once

#include "engine/graphics/Shader.h"
#include "engine/graphics/ShaderStorageBuffer.h"

namespace engine::graphics { class RenderContext; }
namespace engine::simulation { class PixelGrid; }

namespace engine::particles {

class ParticleBuffer;

/// Orchestrates GPU compute shaders for particle physics and re-integration.
///
/// Two-pass pipeline per frame:
///   1. update()       — physics, gravity, DDA grid collision, bounce/settle
///   2. reintegrate()  — writes settled particles back into the CA grid
class ParticleSimulation {
public:
    bool init(int grid_width, int grid_height);
    void shutdown();

    /// Run the particle update compute shader (physics + collision).
    void update(ParticleBuffer& buffer, simulation::PixelGrid& grid,
                graphics::RenderContext& ctx, float dt);

    /// Run the particle re-integration compute shader (settled → grid).
    /// @param material_table Optional material table SSBO for category lookup (binding 2).
    void reintegrate(ParticleBuffer& buffer, simulation::PixelGrid& grid,
                     graphics::RenderContext& ctx,
                     const graphics::ShaderStorageBuffer* material_table = nullptr);

    void set_gravity(float gx, float gy) { m_gravity_x = gx; m_gravity_y = gy; }

private:
    graphics::Shader m_update_shader;
    graphics::Shader m_reintegrate_shader;

    int m_grid_width = 0;
    int m_grid_height = 0;

    float m_gravity_x = 0.0f;
    float m_gravity_y = 300.0f; // pixels/sec² downward

    static constexpr int WORKGROUP_SIZE = 256;
};

} // namespace engine::particles
