#pragma once

#include <string>

namespace engine::simulation { struct PixelGridComponent; }

namespace editor {

// Custom inspector for PixelGridComponent with asset picker for .pxg files
class PixelGridComponentInspector {
public:
    static bool draw(engine::simulation::PixelGridComponent& component, const std::string& project_path);
};

}
