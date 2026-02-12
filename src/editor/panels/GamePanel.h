#pragma once

#include "Panel.h"
#include <imgui.h>
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

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

    /// Get or create a cached RGBA texture for a pixel grid entity.
    GLuint get_pixel_grid_texture(entt::entity entity, const std::string& path);

    /// Remove stale entries from the texture cache.
    void cleanup_texture_cache();

    EditorContext& m_context;

    struct CachedGridTexture {
        GLuint texture_id = 0;
        std::string source_path;
        int width = 0;
        int height = 0;
    };
    std::unordered_map<entt::entity, CachedGridTexture> m_grid_textures;
};

} // namespace editor
