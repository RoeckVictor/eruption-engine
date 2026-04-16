#pragma once

#include <entt/entt.hpp>
#include <string>
#include <array>

namespace engine::ui {

/// Base component for all interactive UI elements.
/// Handles visual state transitions and callback routing.
/// Must be combined with Image component for visuals.
struct UIInteractable {
    bool enabled = true;

    /// Whether this element can receive input (false = visual only, no state changes)
    bool interactable = true;

    /// Entity that receives UI callbacks. If null, callbacks go to this entity.
    entt::entity callback_target = entt::null;

    /// Visual state transition mode
    enum class TransitionMode : int {
        None = 0,       // No visual feedback
        ColorTint = 1,  // Multiply Image color by state color
        SpriteSwap = 2  // Change Image sprite_path per state
    };
    TransitionMode transition_mode = TransitionMode::ColorTint;

    /// Current visual state (managed by UIInteractionSystem)
    enum class State : int {
        Normal = 0,
        Hovered = 1,
        Pressed = 2,
        Disabled = 3
    };

    // --- Color Tint Mode ---
    // Colors are multiplied with the Image component's color
    // Format: {R, G, B, A} normalized 0-1
    std::array<float, 4> normal_color   = {1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 4> hovered_color  = {0.9f, 0.9f, 0.9f, 1.0f};
    std::array<float, 4> pressed_color  = {0.7f, 0.7f, 0.7f, 1.0f};
    std::array<float, 4> disabled_color = {0.5f, 0.5f, 0.5f, 0.5f};

    // --- Sprite Swap Mode ---
    // Sprite paths for each state (empty = use Image's default sprite_path)
    std::string normal_sprite;
    std::string hovered_sprite;
    std::string pressed_sprite;
    std::string disabled_sprite;

    // --- Runtime State (not serialized) ---
    State _current_state = State::Normal;
    bool _was_hovered = false;  // For enter/exit detection

    // Original color backup for tint mode (set when first interacted)
    std::array<float, 4> _original_color = {1.0f, 1.0f, 1.0f, 1.0f};
    bool _original_color_captured = false;
};

} // namespace engine::ui
