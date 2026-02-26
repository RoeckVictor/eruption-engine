#pragma once

#include "Panel.h"
#include "editor/build/GameBuilder.h"
#include <string>

namespace editor {

// Panel for configuring and running game builds
class BuildSettingsPanel : public Panel {
public:
    BuildSettingsPanel();
    ~BuildSettingsPanel() override = default;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void set_project_path(const std::string& path);
    void set_engine_paths(const std::string& src_path, const std::string& build_path);

    void update();

private:
    void render_build_settings();
    void render_build_progress();
    void render_build_status();

    GameBuilder m_builder;
    BuildConfig m_config;

    std::string m_project_path;
    std::string m_engine_src_path;
    std::string m_engine_build_path;

    bool m_build_succeeded = false;
    char m_output_path_buffer[512] = "";
    char m_product_name_buffer[128] = "";
};

}