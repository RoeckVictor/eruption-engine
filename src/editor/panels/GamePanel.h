#pragma once

#include "Panel.h"
#include "editor/core/PixelGridTextureCache.h"
#include <imgui.h>
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>

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

    EditorContext& m_context;
    PixelGridTextureCache m_grid_textures;
};

} // namespace editor
