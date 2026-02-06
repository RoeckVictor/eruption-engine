#pragma once

#include "engine/core/Result.h"
#include "box2d/box2d.h"
#include <cstdint>

namespace engine::physics {

/// Wraps a Box2D world, providing coordinate conversion between pixel space
/// and Box2D meter space, and managing body/shape lifecycle.
///
/// Box2D 3.x uses a C API with handle-based identifiers (b2BodyId, b2ShapeId, etc.).
/// This class owns the b2WorldId and provides helpers for common operations.
class PhysicsWorld {
public:
    /// Initialize the Box2D world with gravity (in meters/sec^2) and pixel scale.
    /// Default gravity is 10 m/s^2 downward (positive Y = down in pixel space).
    /// @param gravity_x Gravity in X direction (m/s^2)
    /// @param gravity_y Gravity in Y direction (m/s^2)
    /// @param pixels_per_meter Pixels-per-meter scale factor for coordinate conversion
    /// Returns an error if Box2D world creation fails.
    Result<void, ErrorInfo> init(float gravity_x = 0.0f, float gravity_y = 10.0f,
                                   float pixels_per_meter = 32.0f);
    void shutdown();

    /// Step the physics simulation by dt seconds.
    /// Uses Box2D's internal sub-stepping (4 sub-steps by default).
    void step(float dt, int sub_step_count = 4);

    // --- Coordinate conversion ---

    b2Vec2 pixels_to_meters(float px, float py) const;
    void meters_to_pixels(b2Vec2 meters, float& px, float& py) const;
    float pixels_to_meters(float pixels) const;
    float meters_to_pixels(float meters) const;

    /// Get the current pixels-per-meter scale factor.
    float pixels_per_meter() const { return m_pixels_per_meter; }

    // --- Body management ---

    b2BodyId create_dynamic_body(float pixel_x, float pixel_y, float angle_rad = 0.0f);
    b2BodyId create_static_body(float pixel_x, float pixel_y);
    b2BodyId create_kinematic_body(float pixel_x, float pixel_y);
    void destroy_body(b2BodyId body);

    // --- Shape management ---

    /// Add a convex polygon shape to a body. Vertices are in local meter space.
    /// Box2D 3.x supports up to 8 vertices per polygon.
    b2ShapeId add_polygon_shape(b2BodyId body, const b2Vec2* verts, int count,
                                float density = 1.0f, float friction = 0.3f,
                                float restitution = 0.1f);

    /// Add a chain shape (for terrain outlines). Vertices in local meter space.
    b2ChainId add_chain_shape(b2BodyId body, const b2Vec2* verts, int count,
                              float friction = 0.5f, float restitution = 0.0f);

    /// Remove all shapes from a body (for re-triangulation).
    void destroy_all_shapes(b2BodyId body);

    // --- Queries ---

    b2Vec2 get_body_position(b2BodyId body) const;
    float get_body_angle(b2BodyId body) const;
    b2Vec2 get_body_linear_velocity(b2BodyId body) const;
    float get_body_angular_velocity(b2BodyId body) const;
    float get_body_mass(b2BodyId body) const;

    void set_body_transform(b2BodyId body, float pixel_x, float pixel_y, float angle_rad);
    void set_body_linear_velocity(b2BodyId body, float vx_pixels, float vy_pixels);
    void set_body_angular_velocity(b2BodyId body, float angular_vel);
    void apply_force(b2BodyId body, float fx_pixels, float fy_pixels);
    void apply_impulse(b2BodyId body, float ix_pixels, float iy_pixels);

    /// Lock or unlock rotation for a body (useful for character controllers)
    void set_fixed_rotation(b2BodyId body, bool fixed);

    /// Set per-body gravity scale (1.0 = normal, 0.0 = no gravity, 0.5 = half gravity for buoyancy)
    void set_gravity_scale(b2BodyId body, float scale);

    /// Check if body is in contact with ground (has contact with static body below)
    bool is_grounded(b2BodyId body, float tolerance_pixels = 2.0f) const;

    b2WorldId world_id() const { return m_world_id; }
    bool valid() const;

private:
    b2WorldId m_world_id = b2_nullWorldId;
    float m_pixels_per_meter = 32.0f;
};

} // namespace engine::physics
