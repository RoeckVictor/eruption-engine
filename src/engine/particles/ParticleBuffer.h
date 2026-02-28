#pragma once

#include "engine/graphics/ShaderStorageBuffer.h"
#include <cstdint>
#include <vector>

namespace engine::particles {

struct GpuParticle {
    float px, py;
    float vx, vy;
    uint32_t material;
    float lifetime;
    uint32_t flags;
    uint32_t color;
};

static_assert(sizeof(GpuParticle) == 32, "GpuParticle must be 32 bytes for GPU alignment");

struct SpawnRequest {
    float px, py;
    float vx, vy;
    uint32_t material;
    float lifetime;
    uint32_t color;
};

namespace ParticleBindings {
    constexpr int PARTICLES = 3;
    constexpr int DEAD_LIST = 4;
}

// Manages GPU particle storage via SSBOs
// Lifecycle:
//   1. Game code queues spawns via spawn()
//   2. reclaim_dead() reads back dead indices from GPU (from previous frame)
//   3. flush_spawns() uploads queued particles into free SSBO slots
//   4. GPU compute shader updates particles and writes dead indices
//   5. Repeat next frame
class ParticleBuffer {
public:
    bool init(int max_particles = 65536);
    void shutdown();

    static constexpr int MAX_PENDING_SPAWNS = 10000;

    void spawn(const SpawnRequest& req);

    void reclaim_dead();

    void flush_spawns();

    void bind_particles() const;
    void bind_dead_list() const;

    void reset_dead_counter();

    int max_particles() const { return m_max_particles; }
    int alive_count() const { return m_max_particles - static_cast<int>(m_free_list.size()); }

private:
    int m_max_particles = 0;

    graphics::ShaderStorageBuffer m_particle_ssbo;
    graphics::ShaderStorageBuffer m_dead_list_ssbo;

    std::vector<int> m_free_list;

    std::vector<SpawnRequest> m_spawn_queue;
};

}
