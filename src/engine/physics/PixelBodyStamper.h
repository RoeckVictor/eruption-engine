#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; }

namespace engine::physics {

class PhysicsWorld;
class PixelBody;

/// Handles stamping rigid body pixels into the cellular automata grid
/// and restoring the original grid state after simulation.
///
/// Per-frame pipeline:
///   1. stamp_all()  — Write body pixels into the CA grid, save originals
///   2. [CA simulation runs]
///   3. clear_all()  — Restore grid to pre-stamp state
class PixelBodyStamper {
public:
    PixelBodyStamper();

    /// Stamp all bodies into the grid.
    /// Reads current grid state, writes body pixels, records originals for clearing.
    /// If particle_buffer is provided, spawns particles when body pixels displace movable materials.
    /// @param bodies List of bodies to stamp
    /// @param world Physics world for coordinate queries
    /// @param grid Pixel grid to stamp into
    /// @param particle_buffer Optional particle buffer for splash effects
    void stamp_all(const std::vector<PixelBody*>& bodies,
                   PhysicsWorld& world,
                   simulation::PixelGrid& grid,
                   particles::ParticleBuffer* particle_buffer = nullptr);

    /// Restore grid pixels that were overwritten during stamping.
    /// @param grid Pixel grid to restore
    void clear_all(simulation::PixelGrid& grid);

private:
    /// Per-body stamp record: stores the original region before stamping
    /// so it can be restored in clear_all with a single upload per body.
    struct BodyStamp {
        int region_x, region_y, region_w, region_h;
        std::vector<uint8_t> original_pixels; // region_w * region_h * 4 (RGBA8UI)
    };

    std::vector<BodyStamp> m_body_stamps;

    // Random number generation for particle spawning scatter
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_scatter_dist;
};

} // namespace engine::physics
