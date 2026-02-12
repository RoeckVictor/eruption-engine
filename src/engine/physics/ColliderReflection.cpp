#include "engine/physics/Colliders.h"
#include "engine/reflection/ReflectionMacros.h"

// Register BoxCollider for reflection
REFLECT_TYPE_BEGIN(engine::physics::BoxCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(width, "Width", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(height, "Height", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// Register CapsuleCollider for reflection
REFLECT_TYPE_BEGIN(engine::physics::CapsuleCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(length, "Length", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// Register CircleCollider for reflection
REFLECT_TYPE_BEGIN(engine::physics::CircleCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// Register DynamicCollider for reflection
REFLECT_TYPE_BEGIN(engine::physics::DynamicCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(simplification, "Simplification", 0.0f, 5.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_contour_area, "Min Contour Area", 0.0f, 100.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(generated, "Generated")
    REFLECT_PROPERTY(triangle_count, "Triangle Count")
REFLECT_TYPE_END()

namespace {
    static engine::reflection::TypeRegistrar<engine::physics::BoxCollider> s_box_collider_registrar;
    static engine::reflection::TypeRegistrar<engine::physics::CapsuleCollider> s_capsule_collider_registrar;
    static engine::reflection::TypeRegistrar<engine::physics::CircleCollider> s_circle_collider_registrar;
    static engine::reflection::TypeRegistrar<engine::physics::DynamicCollider> s_dynamic_collider_registrar;
}
