#pragma once

namespace engine::render {

/// Component that renders a pixel grid.
/// Requires a PixelGridComponent to be present on the same entity.
struct PixelGridRenderer {
    bool enabled = true;

    /// Render layer/order (lower values render first)
    int layer = 0;

    /// Opacity/alpha multiplier (0.0 = invisible, 1.0 = fully opaque)
    float opacity = 1.0f;

    /// Whether to use pixel-perfect rendering (no filtering)
    bool pixel_perfect = true;

    /// Tint color (RGBA normalized: 0.0-1.0)
    float tint_r = 1.0f;
    float tint_g = 1.0f;
    float tint_b = 1.0f;
    float tint_a = 1.0f;
};

} // namespace engine::render
