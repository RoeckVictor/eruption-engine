#pragma once

#include "Gizmo.h"

namespace editor {

/// Gizmo for rotating entities.
/// Shows a circle that can be dragged to rotate.
class RotateGizmo : public Gizmo {
public:
    RotateGizmo() = default;

    GizmoResult render(
        ImDrawList* draw_list,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        entt::entity entity,
        engine::Transform& transform,
        float camera_x,
        float camera_y,
        float zoom,
        GizmoSpace space = GizmoSpace::World
    ) override;

private:
    /// Calculate angle from center to point.
    float angle_to_point(ImVec2 center, ImVec2 point) const;

    /// Check if mouse is near the rotation circle.
    bool is_mouse_near_circle(ImVec2 mouse, ImVec2 center, float radius, float threshold) const;

    bool m_is_hovering = false;
    float m_start_angle = 0.0f;
    float m_start_rotation = 0.0f;

    // Visual settings
    static constexpr float CIRCLE_RADIUS = 70.0f;
    static constexpr float CIRCLE_THICKNESS = 3.0f;
    static constexpr float HIT_THRESHOLD = 10.0f;
};

} // namespace editor
