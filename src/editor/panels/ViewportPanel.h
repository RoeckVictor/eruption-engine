#pragma once

#include "Panel.h"
#include "editor/gizmos/GizmoRenderer.h"
#include <glad/gl.h>

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
    void render_overlay();
    void handle_input();

    EditorContext& m_context;
    GizmoRenderer m_gizmo_renderer;

    GLuint m_framebuffer = 0;
    GLuint m_texture = 0;
    GLuint m_depth_buffer = 0;

    int m_viewport_width = 0;
    int m_viewport_height = 0;

    bool m_is_panning = false;
    float m_pan_start_x = 0.0f;
    float m_pan_start_y = 0.0f;
};

} // namespace editor
