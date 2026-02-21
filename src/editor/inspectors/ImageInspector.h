#pragma once

#include <string>

namespace engine::render { struct Image; }

namespace editor {

/// Custom inspector for Image component with asset picker for image files
class ImageInspector {
public:
    /// Draw the inspector UI for Image component
    /// Returns true if the component was modified
    /// @param project_path Path to the project root (for scanning project assets)
    static bool draw(engine::render::Image& component, const std::string& project_path);

private:
    /// Show asset picker popup for image files (.png, .jpg, .jpeg)
    static void show_asset_picker(engine::render::Image& component, const std::string& project_path);
};

} // namespace editor
