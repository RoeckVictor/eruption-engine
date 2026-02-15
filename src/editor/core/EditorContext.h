#pragma once

#include "editor/commands/CommandHistory.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "RuntimeContext.h"
#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_set>

namespace engine {
class Engine;
}

namespace editor {

class ProjectManager;
class Command;
class ScriptManager;

enum class GizmoVisibility { None, SelectedOnly, All };

struct GizmoVisibilitySettings {
    GizmoVisibility colliders = GizmoVisibility::SelectedOnly;
    GizmoVisibility terrain_colliders = GizmoVisibility::SelectedOnly;
    GizmoVisibility object_origin = GizmoVisibility::None;
    GizmoVisibility object_name = GizmoVisibility::None;
    GizmoVisibility camera_bounds = GizmoVisibility::SelectedOnly;
    GizmoVisibility rigidbody_velocity = GizmoVisibility::SelectedOnly;
    GizmoVisibility pixel_grid_bounds = GizmoVisibility::None;
    GizmoVisibility parent_child_links = GizmoVisibility::None;
};

struct EditingOverride {
    entt::registry* registry = nullptr;
    std::vector<entt::entity>* selection = nullptr;
    std::function<void()> mark_dirty;
};

class EditorContext {
public:
    EditorContext();
    ~EditorContext();

    const std::vector<entt::entity>& selection() const { return m_editing_override.selection ? *m_editing_override.selection : m_selection; }
    bool is_selected(entt::entity entity) const;
    void select(entt::entity entity);
    void add_to_selection(entt::entity entity);
    void remove_from_selection(entt::entity entity);
    void clear_selection();
    void select_multiple(const std::vector<entt::entity>& entities);
    using SelectionChangedCallback = std::function<void()>;
    void set_selection_changed_callback(SelectionChangedCallback callback);

    void set_registry(entt::registry* registry);
    entt::registry* registry() { return m_editing_override.registry ? m_editing_override.registry : m_registry; }
    const entt::registry* registry() const { return m_editing_override.registry ? m_editing_override.registry : m_registry; }
    entt::registry* scene_registry() { return m_registry; }

    void set_editing_override(const EditingOverride& override);
    void clear_editing_override();
    bool has_editing_override() const { return m_editing_override.registry != nullptr; }

    bool is_dirty() const { return m_dirty; }
    void mark_dirty();
    void clear_dirty();

    void copy_selection();
    void paste();
    void duplicate_selection();
    bool has_clipboard() const { return !m_clipboard.empty(); }

    bool has_component_clipboard() const { return !m_component_clipboard.empty(); }
    std::type_index component_clipboard_type() const { return m_component_clipboard_type; }
    void set_component_clipboard(const std::string& data, std::type_index type);
    const std::string& component_clipboard() const { return m_component_clipboard; }

    const std::string& current_scene_path() const { return m_scene_path; }
    void set_current_scene_path(const std::string& path);

    struct EditorCamera {
        float x = 0.0f;
        float y = 0.0f;
        float zoom = 1.0f;
    };
    EditorCamera& camera() { return m_camera; }
    const EditorCamera& camera() const { return m_camera; }
    void focus_on_selection();

    bool is_grid_visible() const { return m_grid_visible; }
    void set_grid_visible(bool visible) { m_grid_visible = visible; }
    bool is_snap_enabled() const { return m_snap_enabled; }
    void set_snap_enabled(bool enabled) { m_snap_enabled = enabled; }
    float grid_size() const { return m_grid_size; }
    void set_grid_size(float size) { m_grid_size = size; }
    float snap_to_grid(float value) const;
    bool is_local_space() const { return m_local_space; }
    void set_local_space(bool local) { m_local_space = local; }

    CommandHistory& history() { return m_history; }
    const CommandHistory& history() const { return m_history; }
    void execute_command(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    bool can_undo() const { return m_history.can_undo(); }
    bool can_redo() const { return m_history.can_redo(); }

    void set_runtime(RuntimeContext* runtime) { m_runtime = runtime; }
    RuntimeContext* runtime() { return m_runtime; }
    const RuntimeContext* runtime() const { return m_runtime; }
    bool is_playing() const { return m_runtime && m_runtime->is_playing(); }
    bool is_paused() const { return m_runtime && m_runtime->is_paused(); }
    PlayState play_state() const { return m_runtime ? m_runtime->state() : PlayState::Editing; }

    void set_script_manager(ScriptManager* sm) { m_script_manager = sm; }
    ScriptManager* script_manager() { return m_script_manager; }
    const ScriptManager* script_manager() const { return m_script_manager; }

    GizmoVisibilitySettings& gizmo_visibility() { return m_gizmo_visibility; }
    const GizmoVisibilitySettings& gizmo_visibility() const { return m_gizmo_visibility; }

    SceneSettings& scene_settings() { return m_scene_settings; }
    const SceneSettings& scene_settings() const { return m_scene_settings; }

    EditorPixelGridLoader& pixel_grid_loader() { return m_pixel_grid_loader; }
    const EditorPixelGridLoader& pixel_grid_loader() const { return m_pixel_grid_loader; }

private:
    void notify_selection_changed();

    std::vector<entt::entity> m_selection;
    std::unordered_set<entt::entity> m_selection_set;
    SelectionChangedCallback m_selection_callback;

    entt::registry* m_registry = nullptr;
    EditingOverride m_editing_override;

    bool m_dirty = false;
    std::string m_scene_path;
    std::string m_clipboard;
    std::string m_component_clipboard;
    std::type_index m_component_clipboard_type = std::type_index(typeid(void));

    EditorCamera m_camera;

    bool m_grid_visible = true;
    bool m_snap_enabled = false;
    float m_grid_size = 32.0f;
    bool m_local_space = false;

    CommandHistory m_history;

    RuntimeContext* m_runtime = nullptr;
    ScriptManager* m_script_manager = nullptr;

    GizmoVisibilitySettings m_gizmo_visibility;
    SceneSettings m_scene_settings;

    EditorPixelGridLoader m_pixel_grid_loader;
};

}
