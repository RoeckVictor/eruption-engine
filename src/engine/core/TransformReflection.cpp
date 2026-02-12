#include "engine/core/Transform.h"
#include "engine/reflection/ReflectionMacros.h"

// Register Transform component for reflection
REFLECT_TYPE_BEGIN(engine::Transform)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY(rotation, "Rotation")
    REFLECT_PROPERTY(scale_x, "Scale X")
    REFLECT_PROPERTY(scale_y, "Scale Y")
    // Note: world_* properties are computed and read-only, handled separately
REFLECT_TYPE_END()

// Register the type at static initialization
// Using anonymous namespace to avoid variable name conflicts with ::
namespace {
    static engine::reflection::TypeRegistrar<engine::Transform> s_transform_registrar;
}
