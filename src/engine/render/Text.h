#pragma once

#include <string>
#include <cstdint>

namespace engine::render {

/// Text alignment options.
enum class TextAlign : int {
    Left = 0,
    Center = 1,
    Right = 2
};

/// Text component for bitmap text rendering.
/// Works with Transform (world space) or ScreenRect (screen space).
///
/// Uses stb_truetype for runtime TTF rasterization with per-size glyph atlases.
struct Text {
    bool enabled = true;

    /// Render layer (lower renders first).
    int layer = 0;

    /// Text content (supports multiline via \n).
    std::string content = "Text";

    /// Font asset path (e.g., "fonts/Roboto-Regular.ttf").
    /// Empty path means no text rendering.
    std::string font_path;

    /// Font size in pixels.
    /// For Transform: world units.
    /// For ScreenRect: screen pixels.
    float font_size = 16.0f;

    /// Text color (RGBA, normalized 0-1).
    float color_r = 1.0f;
    float color_g = 1.0f;
    float color_b = 1.0f;
    float color_a = 1.0f;

    /// Text alignment within the bounding area.
    TextAlign align = TextAlign::Left;

    /// Line height multiplier (1.0 = default font spacing).
    float line_height = 1.2f;

    /// Font style flags.
    /// When enabled, the system looks for corresponding font variants:
    /// - bold: FontName-Bold.ttf
    /// - italic: FontName-Italic.ttf
    /// - bold+italic: FontName-BoldItalic.ttf
    bool bold = false;
    bool italic = false;

    /// Render underline below text baseline.
    bool underline = false;

    /// Maximum width for text wrapping (0 = no wrapping).
    /// In world units for Transform, pixels for ScreenRect.
    float max_width = 0.0f;

    // --- Runtime state (not serialized) ---
    bool _atlas_loaded = false;
    uint32_t _cached_atlas_handle = 0;
};

} // namespace engine::render
