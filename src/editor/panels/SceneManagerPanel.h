#pragma once

#include "Panel.h"
#include <string>
#include <vector>

namespace editor {

/// Scene Manager panel for listing and managing scenes.
class SceneManagerPanel : public Panel {
public:
    SceneManagerPanel();

    void on_gui() override;

private:
    void render_scene_list();
    void render_context_menu(const std::string& scene_path);

    std::vector<std::string> m_scenes;
    std::string m_current_scene;
};

} // namespace editor
