#pragma once

#include <entt/entt.hpp>
#include <string>

namespace engine::ui {

/// Checkbox component for boolean toggle UI.
/// Requires UIInteractable component for interaction handling.
struct Checkbox {
    bool enabled = true;

    /// Current checked state
    bool checked = false;

    /// Child entity reference for checkmark visual (shown/hidden based on checked)
    entt::entity checkmark = entt::null;

    /// Optional toggle group name for radio button behavior.
    /// Empty string = independent checkbox.
    /// Same group name = only one can be checked at a time.
    std::string toggle_group;

    // --- Runtime State (not serialized) ---
    bool _toggled_this_frame = false;
    bool _visuals_initialized = false;
};

} // namespace engine::ui
