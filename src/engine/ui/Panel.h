#pragma once

#include <entt/entt.hpp>

namespace engine::ui {

/// Panel component for UI containers and backgrounds.
/// Can optionally block raycasts or be draggable (for windows).
struct Panel {
    bool enabled = true;

    /// Whether this panel blocks raycasts to elements behind it
    bool blocks_raycast = true;

    /// Whether this panel can be dragged (for floating windows)
    bool draggable = false;

    /// Optional drag handle child entity.
    /// If set, only clicks on this child initiate dragging.
    /// If null, clicking anywhere on the panel initiates dragging.
    entt::entity drag_handle = entt::null;

    /// Optional close button child entity.
    /// If set and clicked, the panel and all children are destroyed.
    entt::entity close_button = entt::null;

    // --- Runtime State (not serialized) ---
    bool _is_dragging = false;
    float _drag_offset_x = 0.0f;
    float _drag_offset_y = 0.0f;
};

} // namespace engine::ui
