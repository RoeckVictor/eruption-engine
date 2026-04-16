#pragma once

#include "engine/render/DynamicFont.h"
#include "engine/render/Text.h"
#include "engine/render/TextUtilities.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/asset/AssetHandle.h"
#include <imgui.h>
#include <unordered_map>
#include <string>

namespace editor {

/// Renders text using DynamicFont to ImGui draw lists.
/// This allows the editor panels to display text with the actual game fonts
/// instead of ImGui's default font.
class EditorTextRenderer {
public:
    explicit EditorTextRenderer(engine::asset::AssetDatabase& assets);

    void render(ImDrawList* draw_list,
                const engine::render::Text& text,
                ImVec2 pos,
                float scale = 1.0f);

    void render_in_area(ImDrawList* draw_list,
                        const engine::render::Text& text,
                        ImVec2 area_pos,
                        ImVec2 area_size,
                        float scale = 1.0f);

    void render_centered(ImDrawList* draw_list,
                         const engine::render::Text& text,
                         ImVec2 center,
                         float scale = 1.0f);

    ImVec2 measure_text(const engine::render::Text& text, float scale = 1.0f);

    void clear_cache();

private:
    engine::asset::AssetDatabase& m_assets;
    std::unordered_map<std::string, engine::asset::Handle<engine::render::DynamicFont>> m_font_cache;

    engine::render::DynamicFont* get_font(const std::string& font_path);

    // Shared text layout data
    struct LayoutLine {
        size_t start;
        size_t end;
        float width;
    };

    struct TextLayout {
        engine::render::DynamicFont* font = nullptr;
        int atlas_size = 0;
        engine::render::SizedAtlas* atlas = nullptr;
        float render_scale = 1.0f;
        float line_height_px = 0.0f;
        std::vector<uint32_t> codepoints;
        std::vector<LayoutLine> lines;
        ImU32 color = 0;
    };

    // Compute layout (font, lines, color) shared between render() and render_in_area()
    bool compute_layout(const engine::render::Text& text, float scale, TextLayout& out);

    // Render glyphs for a single line at cursor_x/cursor_y, returns updated cursor_x
    void render_glyphs(ImDrawList* draw_list, const TextLayout& layout, ImTextureID tex_id,
                       const LayoutLine& line, float cursor_x, float cursor_y);
};

}
