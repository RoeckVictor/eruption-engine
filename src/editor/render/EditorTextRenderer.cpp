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

bool EditorTextRenderer::compute_layout(const engine::render::Text& text, float scale, TextLayout& out) {
    if (text.content.empty() || text.font_path.empty()) return false;

    std::string font_path = build_font_path(text.font_path, text.bold, text.italic);
    out.font = get_font(font_path);
    if (!out.font) out.font = get_font(text.font_path);
    if (!out.font || !out.font->is_valid()) return false;

    float effective_size = text.font_size * scale;
    out.atlas_size = engine::render::quantize_font_size(effective_size);
    out.atlas = out.font->get_atlas(out.atlas_size);
    if (!out.atlas) return false;

    out.render_scale = effective_size / static_cast<float>(out.atlas_size);
    out.codepoints = decode_utf8(text.content);
    if (out.codepoints.empty()) return false;

    out.line_height_px = (out.atlas->ascent - out.atlas->descent + out.atlas->line_gap)
                         * text.line_height * out.render_scale;

    uint8_t r = static_cast<uint8_t>(text.color_r * 255.0f);
    uint8_t g = static_cast<uint8_t>(text.color_g * 255.0f);
    uint8_t b = static_cast<uint8_t>(text.color_b * 255.0f);
    uint8_t a = static_cast<uint8_t>(text.color_a * 255.0f);
    out.color = IM_COL32(r, g, b, a);

    // Break text into lines (with word wrap)
    out.lines.clear();
    size_t line_start = 0;
    for (size_t i = 0; i <= out.codepoints.size(); ++i) {
        bool is_end = (i == out.codepoints.size());
        bool is_newline = !is_end && (out.codepoints[i] == '\n');

        if (text.max_width > 0.0f && !is_end && !is_newline) {
            float line_width = measure_line_width(out.codepoints, line_start, i + 1,
                                                   *out.font, out.atlas_size, out.render_scale);
            if (line_width > text.max_width * scale) {
                size_t break_at = i;
                for (size_t j = i; j > line_start; --j) {
                    if (out.codepoints[j - 1] == ' ') { break_at = j; break; }
                }
                if (break_at > line_start) {
                    float w = measure_line_width(out.codepoints, line_start, break_at,
                                                  *out.font, out.atlas_size, out.render_scale);
                    out.lines.push_back({line_start, break_at, w});
                    line_start = break_at;
                    if (line_start < out.codepoints.size() && out.codepoints[line_start] == ' ')
                        line_start++;
                }
            }
        }

        if (is_newline || is_end) {
            if (i > line_start || is_end) {
                float w = measure_line_width(out.codepoints, line_start, i,
                                              *out.font, out.atlas_size, out.render_scale);
                out.lines.push_back({line_start, i, w});
            }
            line_start = i + 1;
        }
    }

    return true;
}

void EditorTextRenderer::render_glyphs(ImDrawList* draw_list, const TextLayout& layout,
                                        ImTextureID tex_id, const LayoutLine& line,
                                        float cursor_x, float cursor_y) {
    uint32_t prev_cp = 0;

    for (size_t i = line.start; i < line.end; ++i) {
        uint32_t cp = layout.codepoints[i];
        if (cp == '\n' || cp == '\r') continue;

        const auto* glyph = layout.font->get_glyph(layout.atlas_size, cp);
        if (!glyph) continue;

        if (glyph->atlas_w <= 0 || glyph->atlas_h <= 0) {
            if (prev_cp != 0) cursor_x += layout.font->get_kerning(layout.atlas_size, prev_cp, cp) * layout.render_scale;
            cursor_x += glyph->advance * layout.render_scale;
            prev_cp = cp;
            continue;
        }

        if (prev_cp != 0) cursor_x += layout.font->get_kerning(layout.atlas_size, prev_cp, cp) * layout.render_scale;

        float x0 = cursor_x + glyph->bearing_x * layout.render_scale;
        float y0 = cursor_y - glyph->bearing_y * layout.render_scale;
        float x1 = x0 + glyph->atlas_w * layout.render_scale;
        float y1 = y0 + glyph->atlas_h * layout.render_scale;

        float u0 = glyph->atlas_x / static_cast<float>(layout.atlas->width);
        float v0 = glyph->atlas_y / static_cast<float>(layout.atlas->height);
        float u1 = (glyph->atlas_x + glyph->atlas_w) / static_cast<float>(layout.atlas->width);
        float v1 = (glyph->atlas_y + glyph->atlas_h) / static_cast<float>(layout.atlas->height);

        draw_list->AddImage(tex_id, ImVec2(x0, y0), ImVec2(x1, y1),
                            ImVec2(u0, v0), ImVec2(u1, v1), layout.color);

        cursor_x += glyph->advance * layout.render_scale;
        prev_cp = cp;
    }
}

void EditorTextRenderer::render(
    ImDrawList* draw_list,
    const engine::render::Text& text,
    ImVec2 pos,
    float scale
) {
    TextLayout layout;
    if (!compute_layout(text, scale, layout)) return;

    float cursor_y = pos.y + layout.atlas->ascent * layout.render_scale;
    ImTextureID tex_id = (ImTextureID)(uintptr_t)(layout.atlas->texture.imgui_texture_id());

    for (const auto& line : layout.lines) {
        float align_offset = 0.0f;
        switch (text.h_align) {
            case engine::render::TextHAlign::Center: align_offset = -line.width * 0.5f; break;
            case engine::render::TextHAlign::Right:  align_offset = -line.width; break;
            default: break;
        }

        render_glyphs(draw_list, layout, tex_id, line, pos.x + align_offset, cursor_y);
        cursor_y += layout.line_height_px;
    }
}

ImVec2 EditorTextRenderer::measure_text(const engine::render::Text& text, float scale) {
    TextLayout layout;
    if (!compute_layout(text, scale, layout)) return ImVec2(0, 0);

    float max_width = 0.0f;
    for (const auto& line : layout.lines) {
        max_width = std::max(max_width, line.width);
    }
    float total_height = static_cast<float>(layout.lines.size()) * layout.line_height_px;
    return ImVec2(max_width, total_height);
}

void EditorTextRenderer::render_in_area(
    ImDrawList* draw_list,
    const engine::render::Text& text,
    ImVec2 area_pos,
    ImVec2 area_size,
    float scale
) {
    TextLayout layout;
    if (!compute_layout(text, scale, layout)) return;

    // Calculate total text height for vertical alignment
    float total_height = static_cast<float>(layout.lines.size()) * layout.line_height_px;

    float start_y = area_pos.y;
    switch (text.v_align) {
        case engine::render::TextVAlign::Middle:
            start_y = area_pos.y + (area_size.y - total_height) * 0.5f;
            break;
        case engine::render::TextVAlign::Bottom:
            start_y = area_pos.y + (area_size.y - total_height);
            break;
        default: break;
    }

    float cursor_y = start_y + layout.atlas->ascent * layout.render_scale;
    ImTextureID tex_id = (ImTextureID)(uintptr_t)(layout.atlas->texture.imgui_texture_id());

    for (const auto& line : layout.lines) {
        // Area-relative horizontal alignment
        float align_offset = 0.0f;
        switch (text.h_align) {
            case engine::render::TextHAlign::Center: align_offset = (area_size.x - line.width) * 0.5f; break;
            case engine::render::TextHAlign::Right:  align_offset = area_size.x - line.width; break;
            default: break;
        }

        render_glyphs(draw_list, layout, tex_id, line, area_pos.x + align_offset, cursor_y);
        cursor_y += layout.line_height_px;
    }
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
