#pragma once

#include "engine/core/Application.h"
#include "editor/panels/PanelManager.h"
#include "editor/panels/EditorToolbar.h"
#include "editor/core/EditorContext.h"
#include "editor/core/RuntimeContext.h"
#include "editor/scripting/ScriptManager.h"
#include "engine/simulation/CategoryLibrary.h"
#include <string>
#include <functional>

namespace engine {
class Engine;
}

namespace editor {

class ProjectManager;

class EditorApplication : public engine::Application {
public:
    EditorApplication();
    ~EditorApplication() override;

    bool on_init(engine::Engine& engine) override;
    void on_shutdown(engine::Engine& engine) override;
    void on_update(engine::Engine& engine, float dt) override;
    void on_render(engine::Engine& engine) override;

    void request_exit();
    bool should_exit() const { return m_should_exit; }

    PanelManager& panels() { return m_panel_manager; }

    ProjectManager& project_manager();

    EditorContext& context() { return m_context; }
    const EditorContext& context() const { return m_context; }

    RuntimeContext& runtime() { return m_runtime; }
    const RuntimeContext& runtime() const { return m_runtime; }

    bool is_playing() const { return m_runtime.is_playing(); }

    ScriptManager& scripts() { return m_script_manager; }
    const ScriptManager& scripts() const { return m_script_manager; }
    void rebuild_scripts();

    bool has_project() const;
    const std::string& project_path() const;

    void on_project_loaded();

    void new_scene();
    void save_scene();
    void save_scene_as();
    bool load_scene(const std::string& path);

private:
    void init_imgui(engine::Engine& engine);
    void shutdown_imgui();
    void begin_frame();
    void end_frame();

    void render_unsaved_changes_dialog();
    void handle_shortcuts(engine::Engine& engine);

    void confirm_discard_or_save(std::function<void()> action);
    void confirm_all_unsaved(std::function<void()> action);

    void on_file_opened(const std::string& path);

    void launch_pixart(const std::string& file_path);

    void delete_selection();

    void load_project_assets();

    PanelManager m_panel_manager;
    std::unique_ptr<ProjectManager> m_project_manager;
    EditorContext m_context;
    RuntimeContext m_runtime;
    ScriptManager m_script_manager;
    EditorToolbar m_toolbar{m_context, m_runtime, m_script_manager, m_panel_manager};

    entt::registry m_scene_registry;

    engine::simulation::CategoryLibrary m_category_library;

    std::string m_engine_src_path;

    bool m_should_exit = false;
    bool m_imgui_initialized = false;

    std::function<void()> m_pending_action;
    bool m_show_unsaved_dialog = false;
};

}