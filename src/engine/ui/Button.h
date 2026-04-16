#pragma once

#include <string>

namespace engine::ui {

/// Button component for clickable UI elements.
/// Requires UIInteractable component for interaction handling.
/// Optionally pair with Image and/or Text components for visuals.
struct Button {
    bool enabled = true;

    /// Optional sound asset to play on click
    std::string click_sound;

    // --- Runtime State (not serialized) ---
    bool _clicked_this_frame = false;
    bool _pressed_this_frame = false;
    bool _released_this_frame = false;
};

} // namespace engine::ui
