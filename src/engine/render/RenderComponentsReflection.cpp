#include "engine/render/Camera2D.h"
#include "engine/reflection/ReflectionMacros.h"

// Register Camera2D component for reflection
REFLECT_TYPE_BEGIN(engine::render::Camera2D)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY_RANGE(zoom, "Zoom", 0.1f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_zoom, "Min Zoom", 0.01f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(max_zoom, "Max Zoom", 1.0f, 20.0f, 0.5f)
    REFLECT_PROPERTY_RANGE(smoothing, "Smoothing", 0.0f, 20.0f, 0.5f)
REFLECT_TYPE_END()

// Register the type at static initialization
// Using anonymous namespace to avoid variable name conflicts with ::
namespace {
    static engine::reflection::TypeRegistrar<engine::render::Camera2D> s_camera2d_registrar;
}
