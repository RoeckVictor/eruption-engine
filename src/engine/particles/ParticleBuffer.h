#pragma once

#include "engine/graphics/ShaderStorageBuffer.h"
#include <cstdint>
#include <vector>

namespace engine::particles {

/// GPU particle data (std430 layout, 32 bytes per particle).
/// Must match the GLSL struct exactly.
struct GpuParticle {
    float px, py;           // position in world pixels
    float vx, vy;           // velocity in pixels/sec
    uint32_t material;      // material ID (matches CA material IDs)
    float lifetime;         // remaining lifetime in seconds (<=0 = dead)
    uint32_t flags;         // bit 0 = alive
    float _pad;             // padding to 32 bytes
};

static_assert(sizeof(GpuParticle) == 32, "GpuParticle must be 32 bytes for GPU alignment");

/// Particle spawn request (queued on CPU, flushed to GPU).
struct SpawnRequest {
    float px, py;
    float vx, vy;
    uint32_t material;
    float lifetime;
};

/// SSBO binding points used by the particle system.
namespace ParticleBindings {
    constexpr int PARTICLES = 3;    // main particle SSBO
    constexpr int DEAD_LIST = 4;    // dead index reclaim SSBO
}

/// Manages GPU particle storage via SSBOs.
///
/// Lifecycle:
///   1. Game code queues spawns via spawn()
///   2. reclaim_dead() reads back dead indices from GPU (from previous frame)
///   3. flush_spawns() uploads queued particles into free SSBO slots
///   4. GPU compute shader updates particles and writes dead indices
///   5. Repeat next frame
class ParticleBuffer {
public:
    bool init(int max_particles = 65536);
    void shutdown();

    /// Maximum pending spawn requests before new spawns are dropped.
    static constexpr int MAX_PENDING_SPAWNS = 10000;

    /// Queue a particle spawn request (processed during flush_spawns).
    /// Drops the request if the queue exceeds MAX_PENDING_SPAWNS.
    void spawn(const SpawnRequest& req);

    /// Read back dead particle indices from GPU and reclaim their slots.
    /// Call once per frame, before flush_spawns().
    void reclaim_dead();

    /// Upload all queued spawn requests into the particle SSBO at free slots.
    /// Call once per frame, after reclaim_dead().
    void flush_spawns();

    /// Bind particle SSBO to its binding point for compute/render.
    void bind_particles() const;

    /// Bind dead list SSBO to its binding point for compute.
    void bind_dead_list() const;

    /// Reset the dead list counter to 0 on the GPU.
    void reset_dead_counter();

    int max_particles() const { return m_max_particles; }
    int alive_count() const { return m_max_particles - static_cast<int>(m_free_list.size()); }

private:
    int m_max_particles = 0;

    graphics::ShaderStorageBuffer m_particle_ssbo;
    graphics::ShaderStorageBuffer m_dead_list_ssbo;

    // CPU-side free list (indices of available particle slots)
    std::vector<int> m_free_list;

    // Pending spawn queue
    std::vector<SpawnRequest> m_spawn_queue;
};

} // namespace engine::particles
