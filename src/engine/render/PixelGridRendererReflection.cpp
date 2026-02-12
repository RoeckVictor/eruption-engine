#include "engine/render/PixelGridRenderer.h"
#include "engine/reflection/ReflectionMacros.h"

// Register PixelGridRenderer for reflection
REFLECT_TYPE_BEGIN(engine::render::PixelGridRenderer)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(layer, "Render Layer")
    REFLECT_PROPERTY_RANGE(opacity, "Opacity", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(pixel_perfect, "Pixel Perfect")
    REFLECT_PROPERTY_RANGE(tint_r, "Tint Red", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_g, "Tint Green", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_b, "Tint Blue", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_a, "Tint Alpha", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

namespace {
    static engine::reflection::TypeRegistrar<engine::render::PixelGridRenderer> s_pixel_grid_renderer_registrar;
}
