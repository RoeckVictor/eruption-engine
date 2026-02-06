#include "TranslateGizmo.h"
#include <cmath>
#include <algorithm>

namespace editor {

GizmoResult TranslateGizmo::render(
    ImDrawList* draw_list,
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    entt::entity /*entity*/,
    Transform& transform,
    float camera_x,
    float camera_y,
    float zoom
) {
    GizmoResult result;

    // Get entity position in screen space
    ImVec2 center = world_to_screen(
        transform.x, transform.y,
        viewport_pos, viewport_size,
        camera_x, camera_y, zoom
    );

    // Check if gizmo is visible in viewport
    if (center.x < viewport_pos.x - AXIS_LENGTH ||
        center.x > viewport_pos.x + viewport_size.x + AXIS_LENGTH ||
        center.y < viewport_pos.y - AXIS_LENGTH ||
        center.y > viewport_pos.y + viewport_size.y + AXIS_LENGTH) {
        return result;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    // Calculate axis endpoints
    ImVec2 x_end(center.x + AXIS_LENGTH, center.y);
    ImVec2 y_end(center.x, center.y - AXIS_LENGTH);  // Y up in screen space is negative

    // Calculate center square corners
    ImVec2 square_min(center.x, center.y - CENTER_SIZE);
    ImVec2 square_max(center.x + CENTER_SIZE, center.y);

    // Colors
    ImU32 x_color = IM_COL32(220, 60, 60, 255);      // Red for X
    ImU32 y_color = IM_COL32(60, 180, 60, 255);      // Green for Y
    ImU32 xy_color = IM_COL32(255, 220, 60, 200);    // Yellow for XY
    ImU32 x_hover = IM_COL32(255, 120, 120, 255);
    ImU32 y_hover = IM_COL32(120, 255, 120, 255);
    ImU32 xy_hover = IM_COL32(255, 255, 120, 255);

    // Handle dragging
    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Convert current mouse to world space
            ImVec2 world_pos = screen_to_world(
                mouse.x, mouse.y,
                viewport_pos, viewport_size,
                camera_x, camera_y, zoom
            );

            // Calculate delta from drag start
            float dx = world_pos.x - m_drag_start_world.x;
            float dy = world_pos.y - m_drag_start_world.y;

            // Apply delta based on axis constraint
            switch (m_drag_axis) {
                case DragAxis::X:
                    transform.x = m_start_transform.x + dx;
                    break;
                case DragAxis::Y:
                    transform.y = m_start_transform.y + dy;
                    break;
                case DragAxis::XY:
                    transform.x = m_start_transform.x + dx;
                    transform.y = m_start_transform.y + dy;
                    break;
                default:
                    break;
            }

            result.value_changed = true;
        } else {
            // Mouse released - end drag
            m_is_dragging = false;
            m_drag_axis = DragAxis::None;
            result.just_finished = true;
        }
    } else {
        // Not dragging - check for hover and start drag
        m_hover_axis = DragAxis::None;

        // Check center square first (highest priority)
        if (is_mouse_in_rect(mouse, square_min, square_max)) {
            m_hover_axis = DragAxis::XY;
        }
        // Check X axis
        else if (is_mouse_near_line(mouse, center, x_end, HIT_THRESHOLD)) {
            m_hover_axis = DragAxis::X;
        }
        // Check Y axis
        else if (is_mouse_near_line(mouse, center, y_end, HIT_THRESHOLD)) {
            m_hover_axis = DragAxis::Y;
        }

        // Start drag on click
        if (m_hover_axis != DragAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_is_dragging = true;
            m_drag_axis = m_hover_axis;
            m_start_transform = transform;
            m_drag_start_mouse = mouse;
            m_drag_start_world = screen_to_world(
                mouse.x, mouse.y,
                viewport_pos, viewport_size,
                camera_x, camera_y, zoom
            );
            result.just_started = true;
            result.is_active = true;
        }
    }

    // Determine colors based on hover/drag state
    ImU32 draw_x_color = (m_hover_axis == DragAxis::X || m_drag_axis == DragAxis::X) ? x_hover : x_color;
    ImU32 draw_y_color = (m_hover_axis == DragAxis::Y || m_drag_axis == DragAxis::Y) ? y_hover : y_color;
    ImU32 draw_xy_color = (m_hover_axis == DragAxis::XY || m_drag_axis == DragAxis::XY) ? xy_hover : xy_color;

    // Draw gizmo
    float line_thickness = 3.0f;

    // X axis line and arrow
    draw_list->AddLine(center, x_end, draw_x_color, line_thickness);
    // Arrow head (triangle)
    ImVec2 arrow_points[3] = {
        ImVec2(x_end.x + ARROW_SIZE, x_end.y),
        ImVec2(x_end.x - 4, x_end.y - 6),
        ImVec2(x_end.x - 4, x_end.y + 6)
    };
    draw_list->AddTriangleFilled(arrow_points[0], arrow_points[1], arrow_points[2], draw_x_color);

    // Y axis line and arrow
    draw_list->AddLine(center, y_end, draw_y_color, line_thickness);
    // Arrow head (triangle pointing up)
    ImVec2 y_arrow_points[3] = {
        ImVec2(y_end.x, y_end.y - ARROW_SIZE),
        ImVec2(y_end.x - 6, y_end.y + 4),
        ImVec2(y_end.x + 6, y_end.y + 4)
    };
    draw_list->AddTriangleFilled(y_arrow_points[0], y_arrow_points[1], y_arrow_points[2], draw_y_color);

    // Center square for XY movement
    draw_list->AddRectFilled(square_min, square_max, draw_xy_color);

    return result;
}

bool TranslateGizmo::is_mouse_near_line(ImVec2 mouse, ImVec2 a, ImVec2 b, float threshold) const {
    // Calculate distance from point to line segment
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float length_sq = dx * dx + dy * dy;

    if (length_sq < 0.0001f) {
        // Line is a point
        float dist = std::sqrt((mouse.x - a.x) * (mouse.x - a.x) + (mouse.y - a.y) * (mouse.y - a.y));
        return dist <= threshold;
    }

    // Project mouse onto line
    float t = std::max(0.0f, std::min(1.0f, ((mouse.x - a.x) * dx + (mouse.y - a.y) * dy) / length_sq));
    float proj_x = a.x + t * dx;
    float proj_y = a.y + t * dy;

    float dist = std::sqrt((mouse.x - proj_x) * (mouse.x - proj_x) + (mouse.y - proj_y) * (mouse.y - proj_y));
    return dist <= threshold;
}

bool TranslateGizmo::is_mouse_in_rect(ImVec2 mouse, ImVec2 min, ImVec2 max) const {
    return mouse.x >= min.x && mouse.x <= max.x &&
           mouse.y >= min.y && mouse.y <= max.y;
}

} // namespace editor
