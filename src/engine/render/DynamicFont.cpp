#include "DynamicFont.h"
#include "engine/core/Log.h"
#include <stb_truetype.h>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace engine::render {

constexpr int INITIAL_ATLAS_SIZE = 512;
constexpr int MAX_ATLAS_SIZE = 4096;
constexpr int GLYPH_PADDING = 2;

int quantize_font_size(float size) {
    if (size <= 24.0f) {
        return static_cast<int>(std::round(size / 2.0f) * 2.0f);
    }
    return static_cast<int>(std::round(size / 4.0f) * 4.0f);
}

DynamicFont::DynamicFont() = default;
DynamicFont::~DynamicFont() = default;

DynamicFont::DynamicFont(DynamicFont&&) noexcept = default;
DynamicFont& DynamicFont::operator=(DynamicFont&&) noexcept = default;

bool DynamicFont::is_valid() const {
    return font_info && !ttf_data.empty();
}

SizedAtlas* DynamicFont::get_atlas(int font_size) {
    auto it = size_atlases.find(font_size);
    if (it != size_atlases.end()) {
        return it->second.get();
    }

    auto atlas = std::make_unique<SizedAtlas>();
    atlas->font_size = font_size;
    atlas->width = INITIAL_ATLAS_SIZE;
    atlas->height = INITIAL_ATLAS_SIZE;

    // Create blank texture (RGBA, but we only use alpha)
    std::vector<uint8_t> blank(atlas->width * atlas->height * 4, 0);
    if (!atlas->texture.create_2d(
            atlas->width, atlas->height,
            graphics::TextureFormat::RGBA8,
            graphics::TextureFilter::Linear,
            graphics::TextureWrap::ClampToEdge,
            blank.data())) {
        ENGINE_ERR("DynamicFont: Failed to create atlas texture");
        return nullptr;
    }

    // Get font metrics scaled to this size
    float scale = stbtt_ScaleForPixelHeight(font_info.get(), static_cast<float>(font_size));
    int ascent_i, descent_i, line_gap_i;
    stbtt_GetFontVMetrics(font_info.get(), &ascent_i, &descent_i, &line_gap_i);

    atlas->ascent = ascent_i * scale;
    atlas->descent = descent_i * scale;
    atlas->line_gap = line_gap_i * scale;

    auto* ptr = atlas.get();
    size_atlases[font_size] = std::move(atlas);
    return ptr;
}

const RasterizedGlyph* DynamicFont::get_glyph(int font_size, uint32_t codepoint) {
    auto* atlas = get_atlas(font_size);
    if (!atlas) return nullptr;

    auto it = atlas->glyphs.find(codepoint);
    if (it != atlas->glyphs.end()) {
        return &it->second;
    }

    return rasterize_glyph(*atlas, codepoint);
}

const RasterizedGlyph* DynamicFont::rasterize_glyph(SizedAtlas& atlas, uint32_t codepoint) {
    float scale = stbtt_ScaleForPixelHeight(font_info.get(), static_cast<float>(atlas.font_size));

    // Get glyph metrics
    int advance_i, lsb_i;
    stbtt_GetCodepointHMetrics(font_info.get(), static_cast<int>(codepoint), &advance_i, &lsb_i);

    // Get bitmap bounds
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(font_info.get(), static_cast<int>(codepoint), scale, scale,
                                 &x0, &y0, &x1, &y1);

    int glyph_w = x1 - x0;
    int glyph_h = y1 - y0;

    // Handle whitespace and zero-width glyphs
    if (glyph_w <= 0 || glyph_h <= 0) {
        RasterizedGlyph glyph;
        glyph.codepoint = codepoint;
        glyph.advance = advance_i * scale;
        glyph.bearing_x = lsb_i * scale;
        glyph.bearing_y = 0;
        glyph.atlas_x = 0;
        glyph.atlas_y = 0;
        glyph.atlas_w = 0;
        glyph.atlas_h = 0;

        atlas.glyphs[codepoint] = glyph;
        return &atlas.glyphs[codepoint];
    }

    // Check if we need to wrap to the next row
    if (atlas.cursor_x + glyph_w + GLYPH_PADDING > atlas.width) {
        atlas.cursor_x = 0;
        atlas.cursor_y += atlas.row_height + GLYPH_PADDING;
        atlas.row_height = 0;
    }

    // Check if we need to grow the atlas
    if (atlas.cursor_y + glyph_h + GLYPH_PADDING > atlas.height) {
        if (!grow_atlas(atlas)) {
            ENGINE_ERR("DynamicFont: Atlas full, cannot add glyph U+%04X", codepoint);
            return nullptr;
        }
    }

    // Rasterize the glyph (single-channel alpha)
    std::vector<uint8_t> bitmap(glyph_w * glyph_h, 0);
    stbtt_MakeCodepointBitmap(font_info.get(), bitmap.data(),
                               glyph_w, glyph_h, glyph_w,
                               scale, scale, static_cast<int>(codepoint));

    // Convert to RGBA for upload (white with alpha from bitmap)
    std::vector<uint8_t> rgba(glyph_w * glyph_h * 4);
    for (int i = 0; i < glyph_w * glyph_h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    atlas.texture.upload_sub_2d(atlas.cursor_x, atlas.cursor_y, glyph_w, glyph_h, rgba.data());

    // Create glyph record
    RasterizedGlyph glyph;
    glyph.codepoint = codepoint;
    glyph.atlas_x = atlas.cursor_x;
    glyph.atlas_y = atlas.cursor_y;
    glyph.atlas_w = glyph_w;
    glyph.atlas_h = glyph_h;
    glyph.advance = advance_i * scale;
    glyph.bearing_x = static_cast<float>(x0);
    glyph.bearing_y = static_cast<float>(-y0);  // Convert to top-relative

    // Advance cursor
    atlas.cursor_x += glyph_w + GLYPH_PADDING;
    atlas.row_height = std::max(atlas.row_height, glyph_h);

    atlas.glyphs[codepoint] = glyph;
    return &atlas.glyphs[codepoint];
}

float DynamicFont::get_kerning(int font_size, uint32_t first, uint32_t second) {
    float scale = stbtt_ScaleForPixelHeight(font_info.get(), static_cast<float>(font_size));
    int kern = stbtt_GetCodepointKernAdvance(font_info.get(),
                                              static_cast<int>(first),
                                              static_cast<int>(second));
    return kern * scale;
}

bool DynamicFont::grow_atlas(SizedAtlas& atlas) {
    int new_size = atlas.height * 2;
    if (new_size > MAX_ATLAS_SIZE) {
        return false;
    }

    // Read back existing texture data
    std::vector<uint8_t> old_data(atlas.width * atlas.height * 4);
    atlas.texture.readback_sub_2d(0, 0, atlas.width, atlas.height,
                                   old_data.data(), static_cast<int>(old_data.size()));

    // Destroy old texture
    atlas.texture.destroy();

    // Create new larger texture with old data copied to top-left
    std::vector<uint8_t> new_data(new_size * new_size * 4, 0);
    for (int y = 0; y < atlas.height; ++y) {
        std::memcpy(
            new_data.data() + y * new_size * 4,
            old_data.data() + y * atlas.width * 4,
            atlas.width * 4
        );
    }

    if (!atlas.texture.create_2d(
            new_size, new_size,
            graphics::TextureFormat::RGBA8,
            graphics::TextureFilter::Linear,
            graphics::TextureWrap::ClampToEdge,
            new_data.data())) {
        ENGINE_ERR("DynamicFont: Failed to grow atlas texture");
        return false;
    }

    atlas.width = new_size;
    atlas.height = new_size;

    ENGINE_LOG("DynamicFont: Grew atlas to %dx%d for size %d", new_size, new_size, atlas.font_size);
    return true;
}

} // namespace engine::render
