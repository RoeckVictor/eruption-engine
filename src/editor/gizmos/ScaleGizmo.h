#pragma once

#include "Gizmo.h"

namespace editor {

/// Gizmo for scaling entities.
/// Shows axis handles with boxes that can be dragged to scale.
class ScaleGizmo : public Gizmo {
public:
    ScaleGizmo() = default;

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
        Uniform  // Uniform scale (center)
    };

    /// Check if mouse is in a box.
    bool is_mouse_in_box(ImVec2 mouse, ImVec2 center, float half_size) const;

    DragAxis m_drag_axis = DragAxis::None;
    DragAxis m_hover_axis = DragAxis::None;
    float m_start_scale_x = 1.0f;
    float m_start_scale_y = 1.0f;

    // Visual settings
    static constexpr float AXIS_LENGTH = 70.0f;
    static constexpr float HANDLE_SIZE = 8.0f;
    static constexpr float CENTER_SIZE = 10.0f;
};

} // namespace editor
