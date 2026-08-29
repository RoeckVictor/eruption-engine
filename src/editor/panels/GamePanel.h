#pragma once

#include "Panel.h"
#include "editor/render/EditorTextRenderer.h"
#include "editor/render/PanelSceneRenderer.h"
#include "engine/rhi/RHIFramebuffer.h"
#include <imgui.h>
#include <entt/entt.hpp>
#include <memory>
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
    void create_framebuffer(int width, int height);
    void destroy_framebuffer();
    void render_particles_to_framebuffer(ImVec2 panel_size);
    void render_game_view(ImVec2 panel_pos, ImVec2 panel_size);

    entt::entity find_camera_entity() const;

    void render_screen_image(ImDrawList* draw_list, entt::entity entity,
                             ImVec2 panel_pos, ImVec2 panel_size);

    void render_screen_text(ImDrawList* draw_list, entt::entity entity,
                            ImVec2 panel_pos, ImVec2 panel_size);

    void ensure_text_renderer();

    EditorContext& m_context;
    PanelSceneRenderer m_scene_renderer;
    FramebufferResizeDebouncer m_resize_debouncer;

    std::unique_ptr<engine::rhi::RHIFramebuffer> m_framebuffer;
    void* m_imgui_texture_id = nullptr;
    int m_framebuffer_width = 0;
    int m_framebuffer_height = 0;

    std::unique_ptr<EditorTextRenderer> m_text_renderer;
};

}
