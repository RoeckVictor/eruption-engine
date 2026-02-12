#include "engine/simulation/PixelGridComponent.h"
#include "engine/reflection/ReflectionMacros.h"

// Register PixelGridComponent for reflection
REFLECT_TYPE_BEGIN(engine::simulation::PixelGridComponent)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(pixel_grid_path, "Pixel Grid Path")
    REFLECT_PROPERTY(width, "Width")
    REFLECT_PROPERTY(height, "Height")
    REFLECT_PROPERTY(loaded, "Loaded")
REFLECT_TYPE_END()

namespace {
    static engine::reflection::TypeRegistrar<engine::simulation::PixelGridComponent> s_pixel_grid_component_registrar;
}
