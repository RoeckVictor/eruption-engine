#include "engine/physics/Rigidbody.h"
#include "engine/reflection/ReflectionMacros.h"

// Register Rigidbody for reflection
// Note: body_type enum will need custom inspector UI (Phase 7)
REFLECT_TYPE_BEGIN(engine::physics::Rigidbody)
    REFLECT_PROPERTY(enabled, "Enabled")
    // body_type (enum) - needs custom inspector
    REFLECT_PROPERTY_RANGE(mass, "Mass", 0.0f, 1000.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(linear_damping, "Linear Damping", 0.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(angular_damping, "Angular Damping", 0.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(gravity_scale, "Gravity Scale", -10.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY(lock_rotation, "Lock Rotation")
    REFLECT_PROPERTY(lock_position_x, "Lock Position X")
    REFLECT_PROPERTY(lock_position_y, "Lock Position Y")
    REFLECT_PROPERTY(bullet, "Continuous Collision")
    REFLECT_PROPERTY(allow_sleep, "Allow Sleep")
    REFLECT_PROPERTY(awake, "Start Awake")
    REFLECT_PROPERTY(initial_velocity_x, "Initial Velocity X")
    REFLECT_PROPERTY(initial_velocity_y, "Initial Velocity Y")
    REFLECT_PROPERTY(initial_angular_velocity, "Initial Angular Velocity")
REFLECT_TYPE_END()

namespace {
    static engine::reflection::TypeRegistrar<engine::physics::Rigidbody> s_rigidbody_registrar;
}
