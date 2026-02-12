#pragma once

#include "Panel.h"
#include "editor/gizmos/Gizmo.h"
#include "editor/gizmos/TranslateGizmo.h"
#include "editor/gizmos/RotateGizmo.h"
#include "editor/gizmos/ScaleGizmo.h"
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace editor {

class EditorContext;

/// Panel for editing prefab files in isolation.
/// Has its own registry, camera, selection, and gizmos.
class PrefabEditorPanel : public Panel {
public:
    explicit PrefabEditorPanel(EditorContext& main_context);
    ~PrefabEditorPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;
    bool on_close_requested() override;

    /// Open a prefab file for editing.
    void open_prefab(const std::string& path);

    /// Check if prefab has unsaved changes.
    bool has_unsaved_changes() const { return m_dirty; }

    /// If prefab is dirty, show save dialog then execute action.
    /// If clean, execute action immediately.
    void confirm_prefab_discard_or_save(std::function<void()> action);

    /// Render the unsaved changes modal dialog.
    /// Must be called at top-level (outside any panel's Begin/End) to avoid
    /// visual glitches when the panel tab is being closed.
    void render_unsaved_dialog();

private:
    // UI sections
    void render_toolbar();
    void render_viewport();
    void handle_viewport_input();

    // Gizmo orchestration (uses gizmo classes directly, no GizmoRenderer)
    void render_gizmos(ImDrawList* draw_list, ImVec2 vp_pos, ImVec2 vp_size);

    // Prefab I/O
    bool load_prefab_file(const std::string& path);
    bool save_prefab_file();

    // Entity selection
    void select_entity_at(float screen_x, float screen_y);

    // Activate/deactivate the editing override on EditorContext
    void activate_editing_override();
    void deactivate_editing_override();

    // Texture cache (same pattern as ViewportPanel)
    GLuint get_pixel_grid_texture(entt::entity entity, const std::string& path);
    void cleanup_texture_cache();

    // Main context ref (for snap settings, local_space, etc.)
    EditorContext& m_main_context;

    // Isolated prefab state
    entt::registry m_prefab_registry;
    struct {
        float x = 0.0f;
        float y = 0.0f;
        float zoom = 2.0f;
    } m_camera;
    std::vector<entt::entity> m_selection;

    // Gizmo instances (direct, no GizmoRenderer wrapper needed)
    GizmoMode m_gizmo_mode = GizmoMode::Translate;
    TranslateGizmo m_translate_gizmo;
    RotateGizmo m_rotate_gizmo;
    ScaleGizmo m_scale_gizmo;
    bool m_gizmo_active = false;

    // File state
    std::string m_prefab_path;
    bool m_dirty = false;
    bool m_has_prefab = false;

    // Viewport interaction state
    ImVec2 m_viewport_pos = {0, 0};
    ImVec2 m_viewport_size = {0, 0};
    bool m_is_panning = false;
    float m_pan_start_x = 0.0f;
    float m_pan_start_y = 0.0f;

    // Texture cache
    struct CachedGridTexture {
        GLuint texture_id = 0;
        std::string source_path;
        int width = 0;
        int height = 0;
    };
    std::unordered_map<entt::entity, CachedGridTexture> m_grid_textures;

    // Unsaved changes dialog state
    bool m_show_unsaved_dialog = false;
    std::function<void()> m_pending_action;
};

} // namespace editor
