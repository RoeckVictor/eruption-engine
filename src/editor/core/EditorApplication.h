#pragma once

#include "engine/core/Application.h"
#include "editor/panels/PanelManager.h"
#include "editor/core/EditorContext.h"
#include "editor/core/RuntimeContext.h"
#include "editor/scripting/ScriptManager.h"
#include <string>
#include <functional>

namespace engine {
class Engine;
}

namespace editor {

class ProjectManager;

/// The main editor application.
/// Manages the editor UI, project state, and runtime context.
class EditorApplication : public engine::Application {
public:
    EditorApplication();
    ~EditorApplication() override;

    // --- Application interface ---
    bool on_init(engine::Engine& engine) override;
    void on_shutdown(engine::Engine& engine) override;
    void on_update(engine::Engine& engine, float dt) override;
    void on_render(engine::Engine& engine) override;

    /// Request the editor to close.
    void request_exit();

    /// Check if the editor should close.
    bool should_exit() const { return m_should_exit; }

    /// Get the panel manager.
    PanelManager& panels() { return m_panel_manager; }

    /// Get the project manager.
    ProjectManager& project_manager();

    /// Get the editor context.
    EditorContext& context() { return m_context; }
    const EditorContext& context() const { return m_context; }

    /// Get the runtime context.
    RuntimeContext& runtime() { return m_runtime; }
    const RuntimeContext& runtime() const { return m_runtime; }

    /// Check if in play mode.
    bool is_playing() const { return m_runtime.is_playing(); }

    /// Get the script manager.
    ScriptManager& scripts() { return m_script_manager; }
    const ScriptManager& scripts() const { return m_script_manager; }

    /// Rebuild scripts.
    void rebuild_scripts();

    /// Check if a project is currently loaded.
    bool has_project() const;

    /// Get the current project path (empty if no project loaded).
    const std::string& project_path() const;

    /// Called when a project is loaded - shows editor panels.
    void on_project_loaded();

    /// Create a new scene with default entities.
    void new_scene();

    /// Save the current scene.
    void save_scene();

    /// Save the current scene with a file dialog.
    void save_scene_as();

    /// Load a scene from file.
    bool load_scene(const std::string& path);

private:
    void init_imgui(engine::Engine& engine);
    void shutdown_imgui();
    void begin_frame();
    void end_frame();

    void render_toolbar();
    void render_unsaved_changes_dialog();
    void handle_shortcuts(engine::Engine& engine);

    /// Try to perform an action that requires the scene to be clean.
    /// If the scene is dirty, shows a confirmation dialog first.
    /// If the scene is clean, executes the action immediately.
    void confirm_discard_or_save(std::function<void()> action);

    /// Check both prefab editor and scene for unsaved changes before executing action.
    /// Chains: prefab dirty check → scene dirty check → action.
    void confirm_all_unsaved(std::function<void()> action);

    /// Handle opening a file from the file browser.
    void on_file_opened(const std::string& path);

    /// Launch PixArt to edit a .pxg file.
    void launch_pixart(const std::string& file_path);

    PanelManager m_panel_manager;
    std::unique_ptr<ProjectManager> m_project_manager;
    EditorContext m_context;
    RuntimeContext m_runtime;
    ScriptManager m_script_manager;

    // Scene registry owned by the editor
    entt::registry m_scene_registry;

    // Engine source path for script compilation
    std::string m_engine_src_path;

    bool m_should_exit = false;
    bool m_imgui_initialized = false;

    // Pending action for unsaved changes dialog
    std::function<void()> m_pending_action;
    bool m_show_unsaved_dialog = false;
};

} // namespace editor
