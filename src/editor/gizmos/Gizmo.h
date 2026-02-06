#pragma once

#include "editor/core/EditorComponents.h"
#include <entt/entt.hpp>
#include <imgui.h>

namespace editor {

class EditorContext;

/// Gizmo manipulation modes.
enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};

/// Gizmo coordinate space.
enum class GizmoSpace {
    Local,
    World
};

/// Result of gizmo interaction.
struct GizmoResult {
    bool is_active = false;      // Gizmo is being manipulated
    bool value_changed = false;  // Transform was modified this frame
    bool just_started = false;   // Manipulation just started
    bool just_finished = false;  // Manipulation just ended
};

/// Base class for all gizmos.
/// Gizmos are rendered in screen space over the viewport.
class Gizmo {
public:
    virtual ~Gizmo() = default;

    /// Render the gizmo for the given entity.
    /// @param draw_list ImGui draw list to render to
    /// @param viewport_pos Top-left corner of the viewport in screen space
    /// @param viewport_size Size of the viewport
    /// @param entity The entity being manipulated
    /// @param transform The entity's transform
    /// @param camera_x Camera X position in world space
    /// @param camera_y Camera Y position in world space
    /// @param zoom Camera zoom level
    /// @return Result indicating interaction state
    virtual GizmoResult render(
        ImDrawList* draw_list,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        entt::entity entity,
        Transform& transform,
        float camera_x,
        float camera_y,
        float zoom
    ) = 0;

    /// Check if the gizmo is currently being dragged.
    bool is_dragging() const { return m_is_dragging; }

    /// Get the transform value when dragging started (for undo).
    const Transform& start_transform() const { return m_start_transform; }

protected:
    /// Convert world position to screen position.
    ImVec2 world_to_screen(
        float world_x, float world_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        float screen_x = viewport_pos.x + viewport_size.x * 0.5f + (world_x - camera_x) * zoom;
        float screen_y = viewport_pos.y + viewport_size.y * 0.5f - (world_y - camera_y) * zoom;
        return ImVec2(screen_x, screen_y);
    }

    /// Convert screen position to world position.
    ImVec2 screen_to_world(
        float screen_x, float screen_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        float world_x = camera_x + (screen_x - viewport_pos.x - viewport_size.x * 0.5f) / zoom;
        float world_y = camera_y - (screen_y - viewport_pos.y - viewport_size.y * 0.5f) / zoom;
        return ImVec2(world_x, world_y);
    }

    bool m_is_dragging = false;
    Transform m_start_transform;
    ImVec2 m_drag_start_mouse;
    ImVec2 m_drag_start_world;
};

} // namespace editor
