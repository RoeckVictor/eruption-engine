#pragma once

#include "Panel.h"
#include "editor/render/PanelSceneRenderer.h"
#include <imgui.h>
#include <entt/entt.hpp>
#include <string>

namespace editor {

class EditorContext;

// Game panel that renders the scene from a Camera2D entity's perspective
// Shows the pure game view without any editor overlays, gizmos, or grid
// Requires a Camera2D entity in the scene and play mode to be active
class GamePanel : public Panel {
public:
    explicit GamePanel(EditorContext& context);
    ~GamePanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

private:
    void render_game_view(ImVec2 panel_pos, ImVec2 panel_size);

    entt::entity find_camera_entity() const;

    void render_screen_image(ImDrawList* draw_list, entt::entity entity,
                             ImVec2 panel_pos, ImVec2 panel_size);

    void render_screen_text(ImDrawList* draw_list, entt::entity entity,
                            ImVec2 panel_pos, ImVec2 panel_size);

    EditorContext& m_context;
    PanelSceneRenderer m_scene_renderer;
};

}
