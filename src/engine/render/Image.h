#pragma once

#include <string>
#include <cstdint>

namespace engine::render {

/// Image component for rendering sprites/quads with optional texture.
/// Works with Transform (world space) or ScreenRect (screen space).
///
/// If sprite_path is empty, renders a solid white quad (color becomes fill).
/// If sprite_path is set, the texture is multiplied by the color for tinting.
struct Image {
    bool enabled = true;

    /// Render layer (lower renders first, same convention as PixelGridRenderer)
    int layer = 0;

    /// Sprite asset path relative to assets/ (e.g., "textures/player.png").
    /// Empty string = solid color quad (uses a 1x1 white texture internally).
    std::string sprite_path;

    /// Color/tint (RGBA, normalized 0-1).
    /// If sprite_path is empty: this IS the fill color.
    /// If sprite_path is set: multiplied with sprite for tinting/transparency.
    float color_r = 1.0f;
    float color_g = 1.0f;
    float color_b = 1.0f;
    float color_a = 1.0f;

    /// UV coordinates for sprite atlas regions.
    /// Default (0,0)-(1,1) uses the full texture.
    float uv_min_x = 0.0f;
    float uv_min_y = 0.0f;
    float uv_max_x = 1.0f;
    float uv_max_y = 1.0f;

    /// Flip the sprite horizontally or vertically.
    bool flip_x = false;
    bool flip_y = false;

    // --- Runtime state (not serialized) ---
    uint32_t _cached_texture_handle = 0;
    bool _texture_loaded = false;
    int _cached_width = 0;   // Texture width in pixels (populated by renderer)
    int _cached_height = 0;  // Texture height in pixels (populated by renderer)
};

} // namespace engine::render
