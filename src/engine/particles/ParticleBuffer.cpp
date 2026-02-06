#include "engine/particles/ParticleBuffer.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <algorithm>
#include <cstring>

namespace engine::particles {

bool ParticleBuffer::init(int max_particles) {
    m_max_particles = max_particles;

    // Allocate particle SSBO (all zeroed = all dead)
    std::vector<GpuParticle> initial(max_particles, GpuParticle{});
    if (!m_particle_ssbo.create(
            max_particles * sizeof(GpuParticle),
            initial.data(),
            graphics::BufferUsage::DynamicDraw)) {
        ENGINE_ERR("Failed to create particle SSBO");
        return false;
    }

    // Dead list SSBO layout:
    //   [0]              uint dead_count
    //   [4..]            uint dead_indices[max_particles]
    // Total size: (1 + max_particles) * sizeof(uint32_t)
    size_t dead_list_size = (1 + max_particles) * sizeof(uint32_t);
    std::vector<uint32_t> dead_initial(1 + max_particles, 0);
    if (!m_dead_list_ssbo.create(dead_list_size, dead_initial.data(),
                                  graphics::BufferUsage::DynamicDraw)) {
        ENGINE_ERR("Failed to create dead list SSBO");
        return false;
    }

    // All slots start free
    m_free_list.resize(max_particles);
    for (int i = max_particles - 1; i >= 0; i--) {
        m_free_list[max_particles - 1 - i] = i;
    }

    ENGINE_LOG("ParticleBuffer initialized (%d max particles)", max_particles);
    return true;
}

void ParticleBuffer::shutdown() {
    m_particle_ssbo.destroy();
    m_dead_list_ssbo.destroy();
    m_free_list.clear();
    m_spawn_queue.clear();
    m_max_particles = 0;
}

void ParticleBuffer::spawn(const SpawnRequest& req) {
    m_spawn_queue.push_back(req);
}

void ParticleBuffer::reclaim_dead() {
    if (!m_dead_list_ssbo.valid()) return;

    // Read back the dead count from offset 0
    uint32_t dead_count = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dead_list_ssbo.handle());
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &dead_count);

    if (dead_count > 0) {
        // Clamp to avoid buffer overrun
        if (dead_count > static_cast<uint32_t>(m_max_particles)) {
            dead_count = static_cast<uint32_t>(m_max_particles);
        }

        // Read back dead indices
        std::vector<uint32_t> dead_indices(dead_count);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
                           sizeof(uint32_t),
                           dead_count * sizeof(uint32_t),
                           dead_indices.data());

        // Add dead indices back to free list
        for (uint32_t idx : dead_indices) {
            if (static_cast<int>(idx) < m_max_particles) {
                m_free_list.push_back(static_cast<int>(idx));
            }
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ParticleBuffer::flush_spawns() {
    if (m_spawn_queue.empty()) return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particle_ssbo.handle());

    for (const auto& req : m_spawn_queue) {
        if (m_free_list.empty()) break; // No free slots

        int slot = m_free_list.back();
        m_free_list.pop_back();

        GpuParticle p{};
        p.px = req.px;
        p.py = req.py;
        p.vx = req.vx;
        p.vy = req.vy;
        p.material = req.material;
        p.lifetime = req.lifetime;
        p.flags = 1; // alive
        p._pad = 0.0f;

        size_t offset = static_cast<size_t>(slot) * sizeof(GpuParticle);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, sizeof(GpuParticle), &p);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_spawn_queue.clear();
}

void ParticleBuffer::bind_particles() const {
    m_particle_ssbo.bind_base(ParticleBindings::PARTICLES);
}

void ParticleBuffer::bind_dead_list() const {
    m_dead_list_ssbo.bind_base(ParticleBindings::DEAD_LIST);
}

void ParticleBuffer::reset_dead_counter() {
    uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_dead_list_ssbo.handle());
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

} // namespace engine::particles
