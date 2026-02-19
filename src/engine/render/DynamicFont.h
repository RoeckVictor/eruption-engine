#pragma once

#include "engine/graphics/Texture.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

struct stbtt_fontinfo;

namespace engine::render {

/// A single rasterized glyph in an atlas.
struct RasterizedGlyph {
    uint32_t codepoint = 0;

    // Position in atlas texture (pixels)
    int atlas_x = 0;
    int atlas_y = 0;
    int atlas_w = 0;
    int atlas_h = 0;

    // Glyph metrics (in pixels at this font size)
    float advance = 0.0f;    // Horizontal advance after this glyph
    float bearing_x = 0.0f;  // Offset from cursor to left edge of glyph
    float bearing_y = 0.0f;  // Offset from baseline to top edge of glyph
};

/// Atlas texture for a specific font size.
struct SizedAtlas {
    graphics::Texture texture;
    int width = 0;
    int height = 0;
    int font_size = 0;

    // Packing state (simple row-based packing)
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;

    // Glyph lookup by codepoint
    std::unordered_map<uint32_t, RasterizedGlyph> glyphs;

    // Font metrics scaled to this size
    float ascent = 0.0f;   // Distance from baseline to top of tallest glyph
    float descent = 0.0f;  // Distance from baseline to bottom (negative)
    float line_gap = 0.0f; // Extra spacing between lines
};

/// Dynamic font loaded from a TTF file.
/// Manages rasterization and caching of glyphs at various sizes.
struct DynamicFont {
    // Raw TTF file data (must stay in memory for stb_truetype)
    std::vector<uint8_t> ttf_data;

    // stb_truetype font info
    std::unique_ptr<stbtt_fontinfo> font_info;

    // Per-size atlas cache
    std::unordered_map<int, std::unique_ptr<SizedAtlas>> size_atlases;

    // Source path for debugging
    std::string source_path;

    DynamicFont();
    ~DynamicFont();

    DynamicFont(const DynamicFont&) = delete;
    DynamicFont& operator=(const DynamicFont&) = delete;
    DynamicFont(DynamicFont&&) noexcept;
    DynamicFont& operator=(DynamicFont&&) noexcept;

    /// Get or create an atlas for the given font size.
    SizedAtlas* get_atlas(int font_size);

    /// Get a glyph, rasterizing on-demand if not cached.
    const RasterizedGlyph* get_glyph(int font_size, uint32_t codepoint);

    /// Get kerning adjustment between two glyphs (in pixels at font_size).
    float get_kerning(int font_size, uint32_t first, uint32_t second);

    /// Check if the font was loaded successfully.
    bool is_valid() const;

private:
    const RasterizedGlyph* rasterize_glyph(SizedAtlas& atlas, uint32_t codepoint);
    bool grow_atlas(SizedAtlas& atlas);
};

/// Quantize a font size to reduce atlas proliferation.
/// Rounds to nearest 2px for small sizes, 4px for larger sizes.
int quantize_font_size(float size);

} // namespace engine::render
