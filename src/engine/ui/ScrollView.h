#pragma once

#include <entt/entt.hpp>

namespace engine::ui {

/// ScrollView component for scrollable content areas.
/// Requires a viewport (mask) and content container as children.
struct ScrollView {
    bool enabled = true;

    /// Scroll directions
    bool horizontal = false;
    bool vertical = true;

    /// Scroll speed multiplier for mouse wheel
    float scroll_sensitivity = 20.0f;

    /// Inertia - content continues moving after drag release
    bool inertia = true;
    float deceleration_rate = 0.135f;  // How quickly velocity decays (0 = instant stop)

    /// Elastic bounce when scrolling past content bounds
    bool elastic = true;
    float elasticity = 0.1f;  // Spring strength for bounce back

    /// Scrollbar visibility mode
    enum class ScrollbarVisibility : int {
        Permanent = 0,        // Always visible
        AutoHide = 1,         // Visible when scrolling, fades out
        AutoHideAndExpand = 2 // Hidden, viewport expands to fill scrollbar space
    };
    ScrollbarVisibility horizontal_scrollbar_visibility = ScrollbarVisibility::AutoHide;
    ScrollbarVisibility vertical_scrollbar_visibility = ScrollbarVisibility::AutoHide;

    /// Child entity references
    entt::entity viewport = entt::null;    // The visible/clipped area
    entt::entity content = entt::null;     // The scrollable content container
    entt::entity horizontal_scrollbar = entt::null;  // Optional scrollbar
    entt::entity vertical_scrollbar = entt::null;    // Optional scrollbar

    // --- Runtime State (not serialized) ---
    float scroll_x = 0.0f;  // Current scroll offset (pixels)
    float scroll_y = 0.0f;
    float velocity_x = 0.0f;  // Current velocity for inertia
    float velocity_y = 0.0f;
    float content_width = 0.0f;   // Computed content size
    float content_height = 0.0f;
    bool _is_dragging = false;
    float _drag_start_x = 0.0f;
    float _drag_start_y = 0.0f;
    float _drag_start_scroll_x = 0.0f;
    float _drag_start_scroll_y = 0.0f;
    bool _visuals_initialized = false;
};

} // namespace engine::ui
