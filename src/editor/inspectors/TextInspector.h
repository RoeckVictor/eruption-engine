#pragma once

namespace engine::render { struct Text; }

namespace editor {

/// Custom inspector for Text component with font asset picker
class TextInspector {
public:
    /// Draw the inspector UI for Text component
    /// Returns true if the component was modified
    static bool draw(engine::render::Text& component);

private:
    /// Show asset picker popup for font files
    static void show_font_picker(engine::render::Text& component);
};

} // namespace editor
