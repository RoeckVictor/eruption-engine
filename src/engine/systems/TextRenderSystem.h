#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Shader.h"
#include "engine/render/DynamicFont.h"
#include "engine/render/Text.h"
#include "engine/render/TextUtilities.h"
#include "engine/asset/AssetHandle.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

namespace engine {

namespace render { struct Camera2D; }
namespace asset { class AssetDatabase; }

/// Renders entities with Text components using bitmap fonts.
///
/// Handles both world-space (Transform) and screen-space (ScreenRect) entities.
/// Uses stb_truetype for dynamic TTF font rendering.
class TextRenderSystem : public System {
public:
    bool init(Engine& engine) override;
    void shutdown() override;
    void render(Engine& engine) override;

private:
    entt::registry* m_registry = nullptr;
    render::Camera2D* m_camera = nullptr;
    asset::AssetDatabase* m_assets = nullptr;

    graphics::Shader m_shader;

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    size_t m_vbo_capacity = 0;

    // Font cache: font_path -> handle (safe across hot-reloads)
    std::unordered_map<std::string, asset::Handle<render::DynamicFont>> m_font_cache;

    struct TextVertex {
        float x, y;       // Position
        float u, v;       // UV in atlas
        float r, g, b, a; // Color
    };

    /// Get or load a font from the asset database.
    render::DynamicFont* get_font(const std::string& font_path);

    /// Layout text and generate vertices.
    void layout_text(
        const render::Text& text,
        render::DynamicFont& font,
        float origin_x, float origin_y,
        float rotation,  // degrees (world space only)
        bool is_screen_space,
        std::vector<TextVertex>& out_vertices
    );
};

} // namespace engine
