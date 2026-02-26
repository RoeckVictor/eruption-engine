#pragma once

#include "Panel.h"
#include <string>

namespace editor {

class ProjectManager;
class EditorApplication;

// The Project Hub panel shown when no project is loaded
// Displays recent projects and options to create/open projects
class ProjectHubPanel : public Panel {
public:
    ProjectHubPanel(ProjectManager& project_manager, EditorApplication& app);

    void on_gui() override;

private:
    void render_header();
    void render_recent_projects();
    void render_actions();
    void render_new_project_dialog();
    void render_error_dialog();

    ProjectManager& m_project_manager;
    EditorApplication& m_app;

    bool m_show_new_dialog = false;
    char m_new_project_name[128] = "NewProject";
    char m_new_project_path[512] = "";

    bool m_show_error_dialog = false;
    std::string m_error_message;
};

}