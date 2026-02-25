#include "EditorTextRenderer.h"
#include "engine/asset/loaders/DynamicFontLoader.h"
#include <cmath>

namespace editor {

using engine::render::decode_utf8;
using engine::render::measure_line_width;
using engine::render::build_font_path;
using engine::render::normalize_font_path;

EditorTextRenderer::EditorTextRenderer(engine::asset::AssetDatabase& assets)
    : m_assets(assets)
{
}

void EditorTextRenderer::clear_cache() {
    m_font_cache.clear();
}

engine::render::DynamicFont* EditorTextRenderer::get_font(const std::string& font_path) {
    std::string normalized = normalize_font_path(font_path);

    // Check cache for existing handle
    auto it = m_font_cache.find(normalized);
    if (it != m_font_cache.end()) {
        // Always resolve through AssetDatabase to get current pointer
        if (auto* font = m_assets.get(it->second)) {
            return font;
        }
        // Handle became invalid, remove from cache
        m_font_cache.erase(it);
    }

    // Load via AssetDatabase
    auto handle = m_assets.load<engine::render::DynamicFont>(normalized);
    if (!handle.valid()) {
        return nullptr;
    }
    m_font_cache[normalized] = handle;
    return m_assets.get(handle);
}

void EditorTextRenderer::render(
    ImDrawList* draw_list,
    const engine::render::Text& text,
    ImVec2 pos,
    float scale
) {
    if (text.content.empty() || text.font_path.empty()) {
        return;
    }

    // Get the font
    std::string font_path = build_font_path(text.font_path, text.bold, text.italic);
    auto* font = get_font(font_path);
    if (!font) {
        font = get_font(text.font_path);
    }
    if (!font || !font->is_valid()) {
        return;
    }

    // Calculate effective font size and atlas size
    float effective_size = text.font_size * scale;
    int atlas_size = engine::render::quantize_font_size(effective_size);
    auto* atlas = font->get_atlas(atlas_size);
    if (!atlas) return;

    float render_scale = effective_size / static_cast<float>(atlas_size);

    // Decode content
    auto codepoints = decode_utf8(text.content);
    if (codepoints.empty()) return;

    // Line height using font metrics
    float line_height_px = (atlas->ascent - atlas->descent + atlas->line_gap)
                           * text.line_height * render_scale;

    // Text color
    uint8_t r = static_cast<uint8_t>(text.color_r * 255.0f);
    uint8_t g = static_cast<uint8_t>(text.color_g * 255.0f);
    uint8_t b = static_cast<uint8_t>(text.color_b * 255.0f);
    uint8_t a = static_cast<uint8_t>(text.color_a * 255.0f);
    ImU32 color = IM_COL32(r, g, b, a);

    // Break text into lines
    struct Line {
        size_t start;
        size_t end;
        float width;
    };
    std::vector<Line> lines;

    size_t line_start = 0;
    for (size_t i = 0; i <= codepoints.size(); ++i) {
        bool is_end = (i == codepoints.size());
        bool is_newline = !is_end && (codepoints[i] == '\n');

        // Check for word wrap
        if (text.max_width > 0.0f && !is_end && !is_newline) {
            float line_width = measure_line_width(codepoints, line_start, i + 1, *font, atlas_size, render_scale);
            if (line_width > text.max_width * scale) {
                size_t break_at = i;
                for (size_t j = i; j > line_start; --j) {
                    if (codepoints[j - 1] == ' ') {
                        break_at = j;
                        break;
                    }
                }
                if (break_at > line_start) {
                    float w = measure_line_width(codepoints, line_start, break_at, *font, atlas_size, render_scale);
                    lines.push_back({ line_start, break_at, w });
                    line_start = break_at;
                    if (line_start < codepoints.size() && codepoints[line_start] == ' ') {
                        line_start++;
                    }
                }
            }
        }

        if (is_newline || is_end) {
            if (i > line_start || is_end) {
                float w = measure_line_width(codepoints, line_start, i, *font, atlas_size, render_scale);
                lines.push_back({ line_start, i, w });
            }
            line_start = i + 1;
        }
    }

    // Render each line
    // Start position adjusted for ascent (Y-down in screen space)
    float cursor_y = pos.y + atlas->ascent * render_scale;

    ImTextureID tex_id = (ImTextureID)(uintptr_t)(atlas->texture.imgui_texture_id());

    for (const auto& line : lines) {
        // Calculate alignment offset
        float align_offset = 0.0f;
        switch (text.align) {
            case engine::render::TextAlign::Center:
                align_offset = -line.width * 0.5f;
                break;
            case engine::render::TextAlign::Right:
                align_offset = -line.width;
                break;
            default:
                break;
        }

        float cursor_x = pos.x + align_offset;
        uint32_t prev_cp = 0;

        for (size_t i = line.start; i < line.end; ++i) {
            uint32_t cp = codepoints[i];
            if (cp == '\n' || cp == '\r') continue;

            const auto* glyph = font->get_glyph(atlas_size, cp);
            if (!glyph) continue;

            // Skip zero-width glyphs
            if (glyph->atlas_w <= 0 || glyph->atlas_h <= 0) {
                if (prev_cp != 0) {
                    cursor_x += font->get_kerning(atlas_size, prev_cp, cp) * render_scale;
                }
                cursor_x += glyph->advance * render_scale;
                prev_cp = cp;
                continue;
            }

            // Apply kerning
            if (prev_cp != 0) {
                cursor_x += font->get_kerning(atlas_size, prev_cp, cp) * render_scale;
            }

            // Calculate quad position (screen space, Y-down)
            // bearing_y is distance from baseline to top of glyph
            float x0 = cursor_x + glyph->bearing_x * render_scale;
            float y0 = cursor_y - glyph->bearing_y * render_scale;  // Up from baseline
            float x1 = x0 + glyph->atlas_w * render_scale;
            float y1 = y0 + glyph->atlas_h * render_scale;

            // UV coordinates
            float u0 = glyph->atlas_x / static_cast<float>(atlas->width);
            float v0 = glyph->atlas_y / static_cast<float>(atlas->height);
            float u1 = (glyph->atlas_x + glyph->atlas_w) / static_cast<float>(atlas->width);
            float v1 = (glyph->atlas_y + glyph->atlas_h) / static_cast<float>(atlas->height);

            // Add the glyph quad as an image
            draw_list->AddImage(
                tex_id,
                ImVec2(x0, y0), ImVec2(x1, y1),
                ImVec2(u0, v0), ImVec2(u1, v1),
                color
            );

            cursor_x += glyph->advance * render_scale;
            prev_cp = cp;
        }

        cursor_y += line_height_px;
    }
}

ImVec2 EditorTextRenderer::measure_text(const engine::render::Text& text, float scale) {
    if (text.content.empty() || text.font_path.empty()) {
        return ImVec2(0, 0);
    }

    std::string font_path = build_font_path(text.font_path, text.bold, text.italic);
    auto* font = get_font(font_path);
    if (!font) {
        font = get_font(text.font_path);
    }
    if (!font || !font->is_valid()) {
        return ImVec2(0, 0);
    }

    float effective_size = text.font_size * scale;
    int atlas_size = engine::render::quantize_font_size(effective_size);
    auto* atlas = font->get_atlas(atlas_size);
    if (!atlas) return ImVec2(0, 0);

    float render_scale = effective_size / static_cast<float>(atlas_size);
    auto codepoints = decode_utf8(text.content);
    if (codepoints.empty()) return ImVec2(0, 0);

    float line_height_px = (atlas->ascent - atlas->descent + atlas->line_gap)
                           * text.line_height * render_scale;

    // Find max width and count lines
    float max_width = 0.0f;
    int line_count = 1;
    size_t line_start = 0;

    for (size_t i = 0; i <= codepoints.size(); ++i) {
        bool is_end = (i == codepoints.size());
        bool is_newline = !is_end && (codepoints[i] == '\n');

        if (is_newline || is_end) {
            float w = measure_line_width(codepoints, line_start, i, *font, atlas_size, render_scale);
            max_width = std::max(max_width, w);
            if (is_newline) line_count++;
            line_start = i + 1;
        }
    }

    float total_height = line_count * line_height_px;
    return ImVec2(max_width, total_height);
}

void EditorTextRenderer::render_centered(
    ImDrawList* draw_list,
    const engine::render::Text& text,
    ImVec2 center,
    float scale
) {
    ImVec2 size = measure_text(text, scale);
    ImVec2 top_left(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    render(draw_list, text, top_left, scale);
}

} // namespace editor
