#pragma once

#include "Panel.h"
#include "editor/gizmos/GizmoRenderer.h"
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

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

    /// Get or create a cached RGBA texture for a pixel grid entity.
    /// Returns the GL texture ID, or 0 if the grid couldn't be loaded.
    GLuint get_pixel_grid_texture(entt::entity entity, const std::string& path);

    /// Remove stale entries from the texture cache.
    void cleanup_texture_cache();

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

    /// Cached pixel grid textures for viewport rendering.
    struct CachedGridTexture {
        GLuint texture_id = 0;
        std::string source_path;
        int width = 0;
        int height = 0;
    };
    std::unordered_map<entt::entity, CachedGridTexture> m_grid_textures;
};

} // namespace editor
