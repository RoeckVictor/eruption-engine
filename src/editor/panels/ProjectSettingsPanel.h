#pragma once

#include "Panel.h"
#include <string>

namespace editor {

class ProjectManager;

/// Panel for editing project settings.
class ProjectSettingsPanel : public Panel {
public:
    explicit ProjectSettingsPanel(ProjectManager& project_manager);
    ~ProjectSettingsPanel() override = default;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

private:
    void render_general_settings();
    void render_build_settings();
    void render_physics_settings();

    void load_settings();
    void save_settings();

    ProjectManager& m_project_manager;

    // Buffers for editing
    char m_project_name[128] = "";
    char m_default_scene[256] = "";
    char m_company_name[128] = "";
    char m_version[64] = "1.0.0";

    // Physics settings
    float m_gravity_x = 0.0f;
    float m_gravity_y = -9.81f;
    int m_physics_iterations = 8;

    // Build settings
    int m_target_fps = 60;
    bool m_vsync = true;
    int m_window_width = 1280;
    int m_window_height = 720;
    bool m_fullscreen = false;
    bool m_resizable = true;

    bool m_settings_changed = false;
};

} // namespace editor
