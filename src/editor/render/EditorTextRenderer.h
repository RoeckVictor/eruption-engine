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

    /// Render text at a position using ImGui draw list with DynamicFont glyphs.
    /// @param draw_list ImGui draw list to render to
    /// @param text Text component with font_path, content, color, etc.
    /// @param pos Screen position (top-left)
    /// @param scale Scale factor for the text (1.0 = use text.font_size directly)
    void render(ImDrawList* draw_list,
                const engine::render::Text& text,
                ImVec2 pos,
                float scale = 1.0f);

    /// Render text centered at a position.
    /// @param draw_list ImGui draw list to render to
    /// @param text Text component with font_path, content, color, etc.
    /// @param center Screen position (center of text)
    /// @param scale Scale factor for the text (1.0 = use text.font_size directly)
    void render_centered(ImDrawList* draw_list,
                         const engine::render::Text& text,
                         ImVec2 center,
                         float scale = 1.0f);

    /// Measure the bounds of text (width, height).
    /// @param text Text component
    /// @param scale Scale factor
    /// @return Size of the text bounds
    ImVec2 measure_text(const engine::render::Text& text, float scale = 1.0f);

    /// Clear the font cache.
    void clear_cache();

private:
    engine::asset::AssetDatabase& m_assets;
    std::unordered_map<std::string, engine::asset::Handle<engine::render::DynamicFont>> m_font_cache;

    /// Get or load a font.
    engine::render::DynamicFont* get_font(const std::string& font_path);
};

} // namespace editor
