#pragma once

#include "Panel.h"
#include "editor/gizmos/Gizmo.h"
#include "editor/gizmos/TranslateGizmo.h"
#include "editor/gizmos/RotateGizmo.h"
#include "editor/gizmos/ScaleGizmo.h"
#include "editor/render/ViewportCamera.h"
#include "editor/render/EntityHitDetector.h"
#include "editor/core/PixelGridTextureCache.h"
#include <entt/entt.hpp>
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace editor {

class EditorContext;

// Panel for editing prefab files in isolation
// Has its own registry, camera, selection, and gizmos
class PrefabEditorPanel : public Panel {
public:
    explicit PrefabEditorPanel(EditorContext& main_context);
    ~PrefabEditorPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;
    bool on_close_requested() override;

    void open_prefab(const std::string& path);

    bool has_unsaved_changes() const { return m_dirty; }

    void reset_camera() { m_camera.reset(); }

    void confirm_prefab_discard_or_save(std::function<void()> action);

    void render_unsaved_dialog();

private:
    void render_toolbar();
    void render_viewport();
    void handle_viewport_input();

    void update_gizmos(ImVec2 vp_pos, ImVec2 vp_size);
    void render_gizmos(ImDrawList* draw_list, ImVec2 vp_pos, ImVec2 vp_size);

    bool load_prefab_file(const std::string& path);
    bool save_prefab_file();

    void activate_editing_override();
    void deactivate_editing_override();

    EditorContext& m_main_context;

    entt::registry m_prefab_registry;
    ViewportCamera m_camera{0.0f, 0.0f, 2.0f, 0.1f, 20.0f, 2.0f};
    std::vector<entt::entity> m_selection;

    GizmoMode m_gizmo_mode = GizmoMode::Translate;
    TranslateGizmo m_translate_gizmo;
    RotateGizmo m_rotate_gizmo;
    ScaleGizmo m_scale_gizmo;
    bool m_gizmo_active = false;
    bool m_gizmo_hovering = false;

    std::string m_prefab_path;
    bool m_dirty = false;
    bool m_has_prefab = false;

    ImVec2 m_viewport_pos = {0, 0};
    ImVec2 m_viewport_size = {0, 0};

    PixelGridTextureCache m_grid_textures;

    bool m_show_unsaved_dialog = false;
    std::function<void()> m_pending_action;

    ClickCycleState m_click_cycle_state;
};

}
