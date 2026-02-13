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

namespace engine {
class Engine;
}

namespace editor {

class ProjectManager;
class Command;
class ScriptManager;

/// Visibility mode for debug overlays in the viewport.
enum class GizmoVisibility { None, SelectedOnly, All };

/// Per-overlay visibility settings.
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

/// Editing override for panels that provide their own registry/selection
/// (e.g., PrefabEditorPanel). When active, EditorContext delegates
/// registry(), selection(), select(), mark_dirty(), etc. to the override.
struct EditingOverride {
    entt::registry* registry = nullptr;
    std::vector<entt::entity>* selection = nullptr;
    std::function<void()> mark_dirty;   // called instead of setting m_dirty
};

/// Central editor state management.
/// Holds selection, clipboard, dirty state, and provides
/// access to the current scene's registry.
class EditorContext {
public:
    EditorContext();
    ~EditorContext();

    // --- Selection Management ---

    /// Get the currently selected entities.
    /// When an editing override is active, returns the override selection.
    const std::vector<entt::entity>& selection() const { return m_editing_override.selection ? *m_editing_override.selection : m_selection; }

    /// Check if an entity is selected.
    bool is_selected(entt::entity entity) const;

    /// Select a single entity (clears previous selection).
    void select(entt::entity entity);

    /// Add an entity to the selection.
    void add_to_selection(entt::entity entity);

    /// Remove an entity from the selection.
    void remove_from_selection(entt::entity entity);

    /// Clear the selection.
    void clear_selection();

    /// Select multiple entities.
    void select_multiple(const std::vector<entt::entity>& entities);

    /// Callback when selection changes.
    using SelectionChangedCallback = std::function<void()>;
    void set_selection_changed_callback(SelectionChangedCallback callback);

    // --- Scene Registry Access ---

    /// Set the active registry (from the loaded scene).
    void set_registry(entt::registry* registry);

    /// Get the active registry (may be null if no scene loaded).
    /// When an editing override is active, returns the override registry.
    entt::registry* registry() { return m_editing_override.registry ? m_editing_override.registry : m_registry; }
    const entt::registry* registry() const { return m_editing_override.registry ? m_editing_override.registry : m_registry; }

    /// Get the scene registry directly (bypasses override).
    entt::registry* scene_registry() { return m_registry; }

    // --- Editing Override ---

    /// Set an editing override (e.g., from prefab editor).
    /// When active, registry/selection/mark_dirty delegate to the override.
    void set_editing_override(const EditingOverride& override);

    /// Clear the editing override, reverting to the scene context.
    void clear_editing_override();

    /// Check if an editing override is active.
    bool has_editing_override() const { return m_editing_override.registry != nullptr; }

    // --- Dirty State ---

    /// Check if the scene has unsaved changes.
    bool is_dirty() const { return m_dirty; }

    /// Mark the scene as dirty (has unsaved changes).
    void mark_dirty();

    /// Clear the dirty flag (after saving).
    void clear_dirty();

    // --- Clipboard ---

    /// Copy selected entities to clipboard.
    void copy_selection();

    /// Paste entities from clipboard.
    void paste();

    /// Duplicate selected entities (copy + paste).
    void duplicate_selection();

    /// Check if clipboard has content.
    bool has_clipboard() const { return !m_clipboard.empty(); }

    // --- Component Clipboard ---

    /// Check if component clipboard has content.
    bool has_component_clipboard() const { return !m_component_clipboard.empty(); }

    /// Get the type of component in clipboard.
    std::type_index component_clipboard_type() const { return m_component_clipboard_type; }

    /// Set component clipboard data.
    void set_component_clipboard(const std::string& data, std::type_index type);

    /// Get component clipboard data.
    const std::string& component_clipboard() const { return m_component_clipboard; }

    // --- Scene Path ---

    /// Get the path to the currently loaded scene.
    const std::string& current_scene_path() const { return m_scene_path; }

    /// Set the current scene path.
    void set_current_scene_path(const std::string& path);

    // --- Editor Camera ---

    struct EditorCamera {
        float x = 0.0f;
        float y = 0.0f;
        float zoom = 1.0f;
    };

    EditorCamera& camera() { return m_camera; }
    const EditorCamera& camera() const { return m_camera; }

    /// Focus camera on the selected entities.
    void focus_on_selection();

    // --- Grid and Snap Settings ---

    /// Get/set grid visibility.
    bool is_grid_visible() const { return m_grid_visible; }
    void set_grid_visible(bool visible) { m_grid_visible = visible; }

    /// Get/set snap to grid.
    bool is_snap_enabled() const { return m_snap_enabled; }
    void set_snap_enabled(bool enabled) { m_snap_enabled = enabled; }

    /// Get/set grid size.
    float grid_size() const { return m_grid_size; }
    void set_grid_size(float size) { m_grid_size = size; }

    /// Snap a value to the grid.
    float snap_to_grid(float value) const;

    /// Get/set coordinate space (local/world) for gizmos.
    bool is_local_space() const { return m_local_space; }
    void set_local_space(bool local) { m_local_space = local; }

    // --- Command History (Undo/Redo) ---

    /// Get the command history.
    CommandHistory& history() { return m_history; }
    const CommandHistory& history() const { return m_history; }

    /// Execute a command and add to history.
    void execute_command(std::unique_ptr<Command> cmd);

    /// Undo the last command.
    void undo();

    /// Redo the last undone command.
    void redo();

    /// Check if undo is available.
    bool can_undo() const { return m_history.can_undo(); }

    /// Check if redo is available.
    bool can_redo() const { return m_history.can_redo(); }

    // --- Runtime Context ---

    /// Set the runtime context.
    void set_runtime(RuntimeContext* runtime) { m_runtime = runtime; }

    /// Get the runtime context.
    RuntimeContext* runtime() { return m_runtime; }
    const RuntimeContext* runtime() const { return m_runtime; }

    /// Check if in play mode.
    bool is_playing() const { return m_runtime && m_runtime->is_playing(); }

    /// Check if paused.
    bool is_paused() const { return m_runtime && m_runtime->is_paused(); }

    /// Get current play state.
    PlayState play_state() const { return m_runtime ? m_runtime->state() : PlayState::Editing; }

    // --- Script Manager ---

    void set_script_manager(ScriptManager* sm) { m_script_manager = sm; }
    ScriptManager* script_manager() { return m_script_manager; }
    const ScriptManager* script_manager() const { return m_script_manager; }

    // --- Gizmo Visibility ---

    GizmoVisibilitySettings& gizmo_visibility() { return m_gizmo_visibility; }
    const GizmoVisibilitySettings& gizmo_visibility() const { return m_gizmo_visibility; }

    // --- Scene Settings ---

    /// Get/set scene settings (physics, simulation, etc.).
    SceneSettings& scene_settings() { return m_scene_settings; }
    const SceneSettings& scene_settings() const { return m_scene_settings; }

    // --- Pixel Grid Loader ---

    /// Get the pixel grid loader (for loading .pxg files in editor).
    EditorPixelGridLoader& pixel_grid_loader() { return m_pixel_grid_loader; }
    const EditorPixelGridLoader& pixel_grid_loader() const { return m_pixel_grid_loader; }

private:
    void notify_selection_changed();

    std::vector<entt::entity> m_selection;
    SelectionChangedCallback m_selection_callback;

    entt::registry* m_registry = nullptr;
    EditingOverride m_editing_override;

    bool m_dirty = false;
    std::string m_scene_path;
    std::string m_clipboard;  // Serialized entity data for copy/paste
    std::string m_component_clipboard;  // Serialized component data for copy/paste
    std::type_index m_component_clipboard_type = std::type_index(typeid(void));  // Track component type

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

} // namespace editor
