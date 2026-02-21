#pragma once

#include <string>

namespace engine::render { struct Text; }

namespace editor {

/// Custom inspector for Text component with font asset picker
class TextInspector {
public:
    /// Draw the inspector UI for Text component
    /// Returns true if the component was modified
    /// @param project_path Path to the project root (for scanning project assets)
    static bool draw(engine::render::Text& component, const std::string& project_path);

private:
    /// Show asset picker popup for font files
    static void show_font_picker(engine::render::Text& component, const std::string& project_path);
};

} // namespace editor
