#pragma once

#include "Panel.h"
#include "editor/core/PixelGridTextureCache.h"
#include "editor/render/EditorTextureCache.h"
#include "editor/render/EditorTextRenderer.h"
#include <imgui.h>
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>
#include <memory>

namespace editor {

class EditorContext;

/// Game panel that renders the scene from a Camera2D entity's perspective.
/// Shows the pure game view without any editor overlays, gizmos, or grid.
/// Requires a Camera2D entity in the scene and play mode to be active.
class GamePanel : public Panel {
public:
    explicit GamePanel(EditorContext& context);
    ~GamePanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

private:
    /// Render entities using the game camera.
    void render_game_view(ImVec2 panel_pos, ImVec2 panel_size);

    /// Find the first entity with an enabled Camera2D component.
    entt::entity find_camera_entity() const;

    /// Render world-space Image entity
    void render_world_image(ImDrawList* draw_list, entt::entity entity,
                            float cam_x, float cam_y, float cam_zoom,
                            ImVec2 panel_pos, ImVec2 panel_size);

    /// Render world-space Text entity
    void render_world_text(ImDrawList* draw_list, entt::entity entity,
                           float cam_x, float cam_y, float cam_zoom,
                           ImVec2 panel_pos, ImVec2 panel_size);

    /// Render screen-space Image entity
    void render_screen_image(ImDrawList* draw_list, entt::entity entity,
                             ImVec2 panel_pos, ImVec2 panel_size);

    /// Render screen-space Text entity
    void render_screen_text(ImDrawList* draw_list, entt::entity entity,
                            ImVec2 panel_pos, ImVec2 panel_size);

    EditorContext& m_context;
    PixelGridTextureCache m_grid_textures;
    EditorTextureCache m_image_textures;

    // Text renderer using game fonts
    std::unique_ptr<EditorTextRenderer> m_text_renderer;
    void ensure_text_renderer();
};

} // namespace editor
