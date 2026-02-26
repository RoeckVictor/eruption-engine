#pragma once

#include <string>

namespace engine::render { struct Image; }

namespace editor {

// Custom inspector for Image component with asset picker for image files
class ImageInspector {
public:
    static bool draw(engine::render::Image& component, const std::string& project_path);
};

}