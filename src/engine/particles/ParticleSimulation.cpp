#include "engine/particles/ParticleSimulation.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/graphics/RenderContext.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::particles {

bool ParticleSimulation::init(int grid_width, int grid_height) {
    m_grid_width = grid_width;
    m_grid_height = grid_height;

    if (!m_update_shader.load_compute("shaders/particle_update.comp")) {
        ENGINE_ERR("Failed to load particle update shader");
        return false;
    }

    if (!m_reintegrate_shader.load_compute("shaders/particle_reintegrate.comp")) {
        ENGINE_ERR("Failed to load particle reintegrate shader");
        return false;
    }

    // Set constant uniforms
    m_update_shader.use();
    m_update_shader.set_int("u_grid_width", grid_width);
    m_update_shader.set_int("u_grid_height", grid_height);

    m_reintegrate_shader.use();
    m_reintegrate_shader.set_int("u_grid_width", grid_width);
    m_reintegrate_shader.set_int("u_grid_height", grid_height);

    ENGINE_LOG("ParticleSimulation initialized (grid %dx%d)", grid_width, grid_height);
    return true;
}

void ParticleSimulation::shutdown() {
    m_update_shader.destroy();
    m_reintegrate_shader.destroy();
}

void ParticleSimulation::update(ParticleBuffer& buffer,
                                 simulation::PixelGrid& grid,
                                 graphics::RenderContext& ctx,
                                 float dt) {
    int max_p = buffer.max_particles();
    if (max_p == 0) return;

    m_update_shader.use();

    // Per-frame uniforms
    m_update_shader.set_float("u_dt", dt);
    m_update_shader.set_float("u_gravity_x", m_gravity_x);
    m_update_shader.set_float("u_gravity_y", m_gravity_y);
    m_update_shader.set_uint("u_max_particles", static_cast<uint32_t>(max_p));

    // Bind grid read SSBO at binding 0
    m_update_shader.set_uint("u_pixel_size", static_cast<uint32_t>(grid.pixel_size()));
    grid.bind_read_ssbo(0);

    // Bind particle SSBOs
    buffer.bind_particles();
    buffer.bind_dead_list();

    // Dispatch with SSBO barrier (updates particle buffer, rendering will read it)
    int groups = (max_p + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    ctx.dispatch_compute(groups, 1, 1, GL_SHADER_STORAGE_BARRIER_BIT);
}

void ParticleSimulation::reintegrate(ParticleBuffer& buffer,
                                      simulation::PixelGrid& grid,
                                      graphics::RenderContext& ctx) {
    int max_p = buffer.max_particles();
    if (max_p == 0) return;

    m_reintegrate_shader.use();

    // Per-frame uniforms
    m_reintegrate_shader.set_uint("u_max_particles", static_cast<uint32_t>(max_p));
    m_reintegrate_shader.set_uint("u_pixel_size", static_cast<uint32_t>(grid.pixel_size()));

    // Bind both grid SSBOs as read-write (write to both for ping-pong sync)
    grid.ssbo(0).bind_base(0);
    grid.ssbo(1).bind_base(1);

    // Bind particle SSBOs
    buffer.bind_particles();
    buffer.bind_dead_list();

    // Dispatch with both barriers (writes to grid SSBO and particle buffer)
    int groups = (max_p + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    ctx.dispatch_compute(groups, 1, 1,
                         GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

} // namespace engine::particles
