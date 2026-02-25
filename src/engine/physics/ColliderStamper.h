#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <vector>
#include <box2d/box2d.h>

namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; }

namespace engine::physics {

class PhysicsWorld;

// Stamps Box2D collider shapes into the pixel grid for CA collision detection.
// This enables collision between CA movable pixels (sand, water, etc.) and
// standard Box2D rigidbodies (circles, boxes, capsules). The stamped pixels
// are marked with FLAG_RIGIDBODY so sim_step.comp can detect collisions
// and mark movables for particle extraction.
// Per-frame pipeline:
//   1. stamp_colliders()  — Rasterize collider shapes into grid
//   2. [CA simulation runs, marks collisions]
//   3. clear_colliders()  — Restore original pixels
class ColliderStamper {
public:
    ColliderStamper();

    void configure(float scatter_min, float scatter_max, float particle_lifetime);

    void stamp_colliders(PhysicsWorld& world,
                         simulation::PixelGrid& grid,
                         float grid_origin_x, float grid_origin_y,
                         particles::ParticleBuffer* particle_buffer = nullptr);

    void clear_colliders(simulation::PixelGrid& grid);

    bool get_body_velocity_at_position(int x, int y, float& out_vx, float& out_vy) const;

private:
    struct ColliderStamp {
        int region_x, region_y, region_w, region_h;
        std::vector<uint8_t> original_pixels;
        float vel_x, vel_y;
        float center_x, center_y;
    };

    std::vector<ColliderStamp> m_stamps;

    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_scatter_dist;
    float m_particle_lifetime = 5.0f;

    void stamp_circle(b2BodyId body_id, const b2Circle& circle, float body_angle,
                      PhysicsWorld& world, simulation::PixelGrid& grid,
                      float grid_origin_x, float grid_origin_y,
                      particles::ParticleBuffer* particle_buffer);

    void stamp_polygon(b2BodyId body_id, const b2Polygon& polygon, float body_angle,
                       PhysicsWorld& world, simulation::PixelGrid& grid,
                       float grid_origin_x, float grid_origin_y,
                       particles::ParticleBuffer* particle_buffer);

    void stamp_region(b2BodyId body_id, int min_gx, int min_gy, int max_gx, int max_gy,
                      float center_gx, float center_gy,
                      float grid_origin_x, float grid_origin_y, int grid_height,
                      const std::function<bool(float, float)>& inside_test,
                      PhysicsWorld& world, simulation::PixelGrid& grid,
                      particles::ParticleBuffer* particle_buffer);
};

}
