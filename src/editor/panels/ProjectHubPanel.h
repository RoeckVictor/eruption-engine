#pragma once

#include "Panel.h"
#include <string>

namespace editor {

class ProjectManager;
class EditorApplication;

/// The Project Hub panel shown when no project is loaded.
/// Displays recent projects and options to create/open projects.
class ProjectHubPanel : public Panel {
public:
    ProjectHubPanel(ProjectManager& project_manager, EditorApplication& app);

    void on_gui() override;

private:
    void render_header();
    void render_recent_projects();
    void render_actions();
    void render_new_project_dialog();

    std::string open_folder_dialog();

    ProjectManager& m_project_manager;
    EditorApplication& m_app;

    // New project dialog state
    bool m_show_new_dialog = false;
    char m_new_project_name[128] = "NewProject";
    char m_new_project_path[512] = "";
};

} // namespace editor
