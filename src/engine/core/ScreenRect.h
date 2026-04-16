#pragma once

#include <entt/entt.hpp>

namespace engine {

/// ScreenRect component for screen-space entities (UI/HUD).
/// Similar to Unity's RectTransform - positions are in pixels relative to anchors.
/// Entities with ScreenRect should NOT have Transform.
struct ScreenRect {
    bool enabled = true;  // Component enabled flag

    // Anchor point on parent (0-1 normalized coordinates)
    // (0,0) = bottom-left, (1,1) = top-right, (0.5, 0.5) = center
    float anchor_x = 0.5f;
    float anchor_y = 0.5f;

    // Pivot point on this element (0-1 normalized coordinates)
    // Where the element is "registered" - position is relative to this point
    float pivot_x = 0.5f;
    float pivot_y = 0.5f;

    // Offset from anchor point (in pixels)
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    // Size (in pixels)
    float width = 100.0f;
    float height = 100.0f;

    // Computed screen-space position (set by ScreenRectSystem)
    float computed_x = 0.0f;      // Final X position on screen
    float computed_y = 0.0f;      // Final Y position on screen
    float computed_width = 100.0f;  // Final width
    float computed_height = 100.0f; // Final height

    // Clipping - if set, this entity's rendering is clipped to the specified entity's bounds.
    // Runtime-only: set programmatically by systems (e.g., ScrollView). Not serialized via reflection.
    entt::entity clip_to = entt::null;
};

} // namespace engine
