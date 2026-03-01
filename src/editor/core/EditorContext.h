#pragma once

#include "editor/commands/CommandHistory.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/core/EditorPixelGridLoader.h"
#include "editor/core/SelectionContext.h"
#include "editor/core/ClipboardContext.h"
#include "editor/core/ViewportContext.h"
#include "editor/core/SceneStateContext.h"
#include "engine/asset/AssetRegistry.h"
#include "RuntimeContext.h"
#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace editor {

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

// Main editor context that coordinates all editor subsystems
class EditorContext {
public:
    EditorContext();
    ~EditorContext();

    SelectionContext& selection() { return m_selection; }
    const SelectionContext& selection() const { return m_selection; }

    ClipboardContext& clipboard() { return m_clipboard; }
    const ClipboardContext& clipboard() const { return m_clipboard; }

    ViewportContext& viewport() { return m_viewport; }
    const ViewportContext& viewport() const { return m_viewport; }

    SceneStateContext& scene_state() { return m_scene_state; }
    const SceneStateContext& scene_state() const { return m_scene_state; }

    void set_registry(entt::registry* registry);
    entt::registry* registry() { return m_editing_override.registry ? m_editing_override.registry : m_registry; }
    const entt::registry* registry() const { return m_editing_override.registry ? m_editing_override.registry : m_registry; }
    entt::registry* scene_registry() { return m_registry; }

    void set_editing_override(const EditingOverride& override);
    void clear_editing_override();
    bool has_editing_override() const { return m_editing_override.registry != nullptr; }

    CommandHistory& history() { return m_history; }
    const CommandHistory& history() const { return m_history; }
    void execute_command(std::unique_ptr<Command> cmd);
    void undo();
    void redo();

    void copy_selection();
    void paste();
    void duplicate_selection();

    void focus_on_selection();

    void sync_prefab_to_instances(const std::string& prefab_path);

    using FileBrowserRefreshCallback = std::function<void()>;
    void set_file_browser_refresh_callback(FileBrowserRefreshCallback cb) { m_file_browser_refresh = std::move(cb); }
    void refresh_file_browser() { if (m_file_browser_refresh) m_file_browser_refresh(); }

    void update_material_tables();

    void set_runtime(RuntimeContext* runtime) { m_runtime = runtime; }
    RuntimeContext* runtime() { return m_runtime; }
    const RuntimeContext* runtime() const { return m_runtime; }
    bool is_playing() const { return m_runtime && m_runtime->is_playing(); }
    bool is_paused() const { return m_runtime && m_runtime->is_paused(); }
    PlayState play_state() const { return m_runtime ? m_runtime->state() : PlayState::Editing; }

    void set_script_manager(ScriptManager* sm) { m_script_manager = sm; }
    ScriptManager* script_manager() { return m_script_manager; }

    GizmoVisibilitySettings& gizmo_visibility() { return m_gizmo_visibility; }
    const GizmoVisibilitySettings& gizmo_visibility() const { return m_gizmo_visibility; }

    SceneSettings& scene_settings() { return m_scene_settings; }
    const SceneSettings& scene_settings() const { return m_scene_settings; }

    EditorPixelGridLoader& pixel_grid_loader() { return m_pixel_grid_loader; }

    engine::asset::AssetRegistry& asset_registry() { return m_asset_registry; }
    const engine::asset::AssetRegistry& asset_registry() const { return m_asset_registry; }

    void init_asset_registry(const std::string& project_path);
    void shutdown_asset_registry();
    void rescan_assets_for_external_changes();
    void update_asset_references(const std::string& old_path, const std::string& new_path);

private:
    void update_registry_paths(entt::registry& reg, const std::string& old_path, const std::string& new_path);
    void update_file_paths(const std::filesystem::path& file_path, const std::string& old_path, const std::string& new_path);
    void sync_prefab_to_project_scenes(const std::string& prefab_path,
                                        entt::registry& prefab_registry,
                                        entt::entity prefab_root);

    SelectionContext m_selection;
    ClipboardContext m_clipboard;
    ViewportContext m_viewport;
    SceneStateContext m_scene_state;

    entt::registry* m_registry = nullptr;
    EditingOverride m_editing_override;

    FileBrowserRefreshCallback m_file_browser_refresh;

    CommandHistory m_history;

    RuntimeContext* m_runtime = nullptr;
    ScriptManager* m_script_manager = nullptr;

    GizmoVisibilitySettings m_gizmo_visibility;
    SceneSettings m_scene_settings;

    EditorPixelGridLoader m_pixel_grid_loader;
    engine::asset::AssetRegistry m_asset_registry;
};

}
