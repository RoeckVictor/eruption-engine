#pragma once

#include "engine/platform/InputAction.h"
#include <string>

namespace engine {
class Engine;
}

namespace editor {
class InputActionPanel {
public:
    void draw(engine::platform::InputActionMap& action_map, engine::Engine& engine);

    void set_file_path(const std::string& path) { m_file_path = path; }

private:
    void draw_bindings_list(const char* label, std::vector<engine::platform::InputBinding>& bindings,
                            engine::Engine& engine);
    void draw_binding_row(engine::platform::InputBinding& binding, int index, engine::Engine& engine);

    std::string m_file_path; // Path to save the action map JSON
    bool m_listening = false;
    int  m_listen_action_index = -1;
    int  m_listen_binding_type = 0; // 0=bindings, 1=negative, 2=positive
    int  m_listen_binding_index = -1;
};

}
