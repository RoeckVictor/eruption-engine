#pragma once

#include <string>

namespace engine::ui {
struct UIInteractable;
}

namespace editor {

/// Custom inspector for UIInteractable component with asset pickers for sprites.
class UIInteractableInspector {
public:
    /// Draw the inspector UI.
    /// @param component The UIInteractable component to edit
    /// @param project_path Project root for asset scanning
    /// @return true if any value was changed
    static bool draw(engine::ui::UIInteractable& component, const std::string& project_path);
};

} // namespace editor
