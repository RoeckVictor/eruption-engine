#include "TextUtilities.h"
#include "DynamicFont.h"

namespace engine::render {

std::vector<uint32_t> decode_utf8(const std::string& text) {
    std::vector<uint32_t> codepoints;
    codepoints.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = 0;
        unsigned char c = text[i];

        if ((c & 0x80) == 0) {
            // 1-byte sequence (ASCII)
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            cp = (c & 0x1F) << 6;
            if (i + 1 < text.size()) {
                cp |= (text[i + 1] & 0x3F);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            cp = (c & 0x0F) << 12;
            if (i + 1 < text.size()) {
                cp |= (text[i + 1] & 0x3F) << 6;
            }
            if (i + 2 < text.size()) {
                cp |= (text[i + 2] & 0x3F);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            cp = (c & 0x07) << 18;
            if (i + 1 < text.size()) {
                cp |= (text[i + 1] & 0x3F) << 12;
            }
            if (i + 2 < text.size()) {
                cp |= (text[i + 2] & 0x3F) << 6;
            }
            if (i + 3 < text.size()) {
                cp |= (text[i + 3] & 0x3F);
            }
            i += 4;
        } else {
            // Invalid sequence, skip byte
            i += 1;
            continue;
        }

        codepoints.push_back(cp);
    }

    return codepoints;
}

float measure_line_width(
    const std::vector<uint32_t>& codepoints,
    size_t start, size_t end,
    DynamicFont& font,
    int font_size,
    float render_scale
) {
    float width = 0.0f;
    uint32_t prev_cp = 0;

    for (size_t i = start; i < end; ++i) {
        uint32_t cp = codepoints[i];
        if (cp == '\n' || cp == '\r') continue;

        const auto* glyph = font.get_glyph(font_size, cp);
        if (!glyph) continue;

        if (prev_cp != 0) {
            width += font.get_kerning(font_size, prev_cp, cp) * render_scale;
        }

        width += glyph->advance * render_scale;
        prev_cp = cp;
    }

    return width;
}

TextVisualBounds measure_visual_bounds(
    const std::vector<uint32_t>& codepoints,
    size_t start, size_t end,
    DynamicFont& font,
    int font_size,
    float render_scale
) {
    TextVisualBounds bounds;
    float cursor_x = 0.0f;
    uint32_t prev_cp = 0;
    bool first_visible = true;

    for (size_t i = start; i < end; ++i) {
        uint32_t cp = codepoints[i];
        if (cp == '\n' || cp == '\r') continue;

        const auto* glyph = font.get_glyph(font_size, cp);
        if (!glyph) continue;

        if (prev_cp != 0) {
            cursor_x += font.get_kerning(font_size, prev_cp, cp) * render_scale;
        }

        // Track visual bounds (only for visible glyphs with actual width)
        if (glyph->atlas_w > 0) {
            float glyph_left = cursor_x + glyph->bearing_x * render_scale;
            float glyph_right = glyph_left + glyph->atlas_w * render_scale;

            if (first_visible) {
                bounds.left = glyph_left;
                bounds.right = glyph_right;
                first_visible = false;
            } else {
                bounds.right = glyph_right;
            }
        }

        cursor_x += glyph->advance * render_scale;
        prev_cp = cp;
    }

    return bounds;
}

} // namespace engine::render
