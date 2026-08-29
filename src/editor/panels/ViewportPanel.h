#pragma once

#include "Panel.h"
#include "editor/core/CoordinateUtils.h"
#include "editor/gizmos/GizmoRenderer.h"
#include "editor/render/EntityHitDetector.h"
#include "editor/render/PanelSceneRenderer.h"
#include "engine/rhi/RHIFramebuffer.h"
#include <entt/entt.hpp>
#include <imgui.h>
#include <string>
#include <memory>

namespace editor {

class EditorContext;

// Viewport panel for rendering the scene
class ViewportPanel : public Panel {
public:
    explicit ViewportPanel(EditorContext& context);
    ~ViewportPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    GizmoRenderer& gizmo_renderer() { return m_gizmo_renderer; }
    const GizmoRenderer& gizmo_renderer() const { return m_gizmo_renderer; }

private:
    void create_framebuffer(int width, int height);
    void destroy_framebuffer();
    void render_scene();
    void render_grid();
    void render_entities();
    void render_overlay();
    void handle_input();

    EditorContext& m_context;
    GizmoRenderer m_gizmo_renderer;
    PanelSceneRenderer m_scene_renderer;
    FramebufferResizeDebouncer m_resize_debouncer;

    std::unique_ptr<engine::rhi::RHIFramebuffer> m_framebuffer;
    void* m_imgui_texture_id = nullptr;

    int m_viewport_width = 0;
    int m_viewport_height = 0;

    bool m_is_panning = false;
    float m_pan_start_x = 0.0f;
    float m_pan_start_y = 0.0f;

    ClickCycleState m_click_cycle_state;
};

}
