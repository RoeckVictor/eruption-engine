#pragma once

#include <entt/entt.hpp>

namespace engine::ui {

/// Slider component for value selection via dragging.
/// Requires UIInteractable component for interaction handling.
struct Slider {
    bool enabled = true;

    /// Current value (always stored normalized 0-1 internally)
    float value = 0.5f;

    /// Value range exposed to users
    float min_value = 0.0f;
    float max_value = 1.0f;

    /// Snap value to whole numbers
    bool whole_numbers = false;

    /// Slider direction
    enum class Direction : int {
        LeftToRight = 0,
        RightToLeft = 1,
        BottomToTop = 2,
        TopToBottom = 3
    };
    Direction direction = Direction::LeftToRight;

    /// Child entity references (optional, for more complex sliders)
    /// If set, these entities are updated automatically by UIInteractionSystem
    entt::entity fill_rect = entt::null;   // Stretches width/height with value
    entt::entity handle = entt::null;       // Positioned along the slider track

    // --- Runtime State (not serialized) ---
    bool _is_dragging = false;
    float _drag_start_value = 0.0f;
    bool _visuals_initialized = false;  // Set true after fill_rect/handle are synced with value

    /// Get the actual value in user range
    float get_value() const {
        float v = value * (max_value - min_value) + min_value;
        if (whole_numbers) {
            v = static_cast<float>(static_cast<int>(v + 0.5f));
        }
        return v;
    }

    /// Set value from user range (clamped and normalized internally)
    void set_value(float v) {
        if (whole_numbers) {
            v = static_cast<float>(static_cast<int>(v + 0.5f));
        }
        float range = max_value - min_value;
        if (range > 0.0001f) {
            value = (v - min_value) / range;
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
        } else {
            value = 0.0f;
        }
    }
};

} // namespace engine::ui
