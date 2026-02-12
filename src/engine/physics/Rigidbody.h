#pragma once

#include "box2d/box2d.h"

namespace engine::physics {

/// Body type for rigidbody physics (matches Box2D body types)
enum class BodyType {
    Static,    // Zero mass, zero velocity, no forces applied (immovable)
    Kinematic, // Zero mass, velocity set manually, no forces applied (scripted movement)
    Dynamic    // Positive mass, velocity computed, forces applied (normal physics)
};

/// Component for Box2D rigidbody physics.
/// Represents a physical body that can collide with other bodies.
struct Rigidbody {
    bool enabled = true;

    // --- Body Configuration ---

    /// Body type (Static, Kinematic, Dynamic)
    BodyType body_type = BodyType::Dynamic;

    /// Mass of the body in kilograms (only for Dynamic bodies)
    /// If mass = 0, it will be computed from collider density
    float mass = 1.0f;

    /// Linear damping (air resistance for translation: 0 = no drag, higher = more drag)
    float linear_damping = 0.0f;

    /// Angular damping (air resistance for rotation: 0 = no spin loss, higher = more loss)
    float angular_damping = 0.0f;

    /// Gravity scale multiplier (0 = no gravity, 1 = normal gravity, negative = reverse gravity)
    float gravity_scale = 1.0f;

    // --- Constraints ---

    /// Lock rotation (fixed_rotation in Box2D)
    bool lock_rotation = false;

    /// Freeze X position (prevent movement along X axis)
    bool lock_position_x = false;

    /// Freeze Y position (prevent movement along Y axis)
    bool lock_position_y = false;

    // --- Advanced Physics ---

    /// Enable continuous collision detection (prevents tunneling for fast objects)
    bool bullet = false;

    /// Allow the body to sleep when at rest (performance optimization)
    bool allow_sleep = true;

    /// Start awake (if false, body starts sleeping)
    bool awake = true;

    // --- Initial Velocity ---

    /// Initial linear velocity X (m/s)
    float initial_velocity_x = 0.0f;

    /// Initial linear velocity Y (m/s)
    float initial_velocity_y = 0.0f;

    /// Initial angular velocity (radians/s)
    float initial_angular_velocity = 0.0f;

    // --- Runtime State (not serialized, managed by physics system) ---

    /// Box2D body handle (managed by physics system, not serialized)
    b2BodyId body_id = b2_nullBodyId;
};

} // namespace engine::physics
