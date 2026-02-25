#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; }

namespace engine::physics {

class PhysicsWorld;
class PixelBody;

// Handles stamping rigid body pixels into the cellular automata grid
// and restoring the original grid state after simulation.
//
// Per-frame pipeline:
//   1. stamp_all()  — Write body pixels into the CA grid, save originals
//   2. [CA simulation runs]
//   3. clear_all()  — Restore grid to pre-stamp state
class PixelBodyStamper {
public:
    PixelBodyStamper();

    void stamp_all(const std::vector<PixelBody*>& bodies,
                   PhysicsWorld& world,
                   simulation::PixelGrid& grid,
                   particles::ParticleBuffer* particle_buffer = nullptr);

    void clear_all(simulation::PixelGrid& grid);

    bool get_body_velocity_at_position(int x, int y, float& out_vx, float& out_vy) const;

private:
    struct BodyStamp {
        int region_x, region_y, region_w, region_h;
        std::vector<uint8_t> original_pixels;
        float vel_x, vel_y;
    };

    std::vector<BodyStamp> m_body_stamps;
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_scatter_dist;
};

}
