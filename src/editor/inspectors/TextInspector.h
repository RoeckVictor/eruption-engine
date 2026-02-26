#pragma once

#include <string>

namespace engine::render { struct Text; }

namespace editor {

// Custom inspector for Text component with font asset picker
class TextInspector {
public:
    static bool draw(engine::render::Text& component, const std::string& project_path);
};

}