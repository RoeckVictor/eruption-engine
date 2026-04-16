#pragma once

#include <string>
#include <entt/entt.hpp>

namespace engine::ui {
struct Dropdown;
}

namespace editor {

/// Custom inspector for Dropdown component with options list editor.
class DropdownInspector {
public:
    /// Draw the inspector UI.
    /// @param component The Dropdown component to edit
    /// @param registry The ECS registry for entity reference resolution
    /// @param owner The entity that owns this component
    /// @return true if any value was changed
    static bool draw(engine::ui::Dropdown& component, entt::registry& registry, entt::entity owner);
};

} // namespace editor
