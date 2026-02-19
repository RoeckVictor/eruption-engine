#pragma once

#include "Gizmo.h"

namespace editor {

/// Gizmo for translating (moving) entities.
/// Shows X and Y axis arrows that can be dragged.
class TranslateGizmo : public Gizmo {
public:
    TranslateGizmo() = default;

    GizmoResult update(
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        entt::entity entity,
        engine::Transform& transform,
        float camera_x,
        float camera_y,
        float zoom,
        GizmoSpace space = GizmoSpace::World
    ) override;

    void render(
        ImDrawList* draw_list,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        const engine::Transform& transform,
        float camera_x,
        float camera_y,
        float zoom,
        GizmoSpace space = GizmoSpace::World
    ) override;

private:
    enum class DragAxis {
        None,
        X,
        Y,
        XY  // Both axes (center square)
    };

    /// Check if mouse is near a line segment.
    bool is_mouse_near_line(ImVec2 mouse, ImVec2 a, ImVec2 b, float threshold) const;

    /// Check if mouse is inside a rectangle.
    bool is_mouse_in_rect(ImVec2 mouse, ImVec2 min, ImVec2 max) const;

    DragAxis m_drag_axis = DragAxis::None;
    DragAxis m_hover_axis = DragAxis::None;

    // Visual settings
    static constexpr float AXIS_LENGTH = 80.0f;
    static constexpr float ARROW_SIZE = 12.0f;
    static constexpr float CENTER_SIZE = 15.0f;
    static constexpr float HIT_THRESHOLD = 8.0f;
};

} // namespace editor
