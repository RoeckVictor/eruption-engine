#pragma once

#include "Panel.h"
#include "editor/gizmos/GizmoRenderer.h"
#include "editor/render/EntityHitDetector.h"
#include "editor/core/PixelGridTextureCache.h"
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <imgui.h>
#include <string>

namespace editor {

class EditorContext;

/// Viewport panel for rendering the scene.
class ViewportPanel : public Panel {
public:
    explicit ViewportPanel(EditorContext& context);
    ~ViewportPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    /// Access the gizmo renderer.
    GizmoRenderer& gizmo_renderer() { return m_gizmo_renderer; }
    const GizmoRenderer& gizmo_renderer() const { return m_gizmo_renderer; }

private:
    void create_framebuffer(int width, int height);
    void destroy_framebuffer();
    void render_scene();
    void render_grid();
    void render_entities();
    void render_overlay();
    void render_debug_overlays(ImDrawList* draw_list, ImVec2 viewport_pos, ImVec2 viewport_size);
    void handle_input();

    EditorContext& m_context;
    GizmoRenderer m_gizmo_renderer;
    PixelGridTextureCache m_grid_textures;

    GLuint m_framebuffer = 0;
    GLuint m_texture = 0;
    GLuint m_depth_buffer = 0;

    int m_viewport_width = 0;
    int m_viewport_height = 0;

    // Resize debouncing: avoid recreating the framebuffer every frame during drag
    int m_pending_width = 0;
    int m_pending_height = 0;
    float m_resize_timer = 0.0f;
    static constexpr float RESIZE_DEBOUNCE_SEC = 0.15f;
    bool m_framebuffer_failed = false;  // Prevent retry loop on persistent failure

    bool m_is_panning = false;
    float m_pan_start_x = 0.0f;
    float m_pan_start_y = 0.0f;

    // Click-to-select cycling state
    ClickCycleState m_click_cycle_state;
};

} // namespace editor
