#pragma once

#include <string>

namespace engine::ui {
struct Button;
}

namespace editor {

/// Custom inspector for Button component with asset picker for click sound.
class ButtonInspector {
public:
    /// Draw the inspector UI.
    /// @param component The Button component to edit
    /// @param project_path Project root for asset scanning
    /// @return true if any value was changed
    static bool draw(engine::ui::Button& component, const std::string& project_path);
};

} // namespace editor
