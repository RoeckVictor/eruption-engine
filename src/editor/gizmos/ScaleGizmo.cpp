#include "ScaleGizmo.h"
#include <cmath>
#include <algorithm>

namespace editor {

GizmoResult ScaleGizmo::render(
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

    // Calculate handle positions
    ImVec2 x_handle(center.x + AXIS_LENGTH, center.y);
    ImVec2 y_handle(center.x, center.y - AXIS_LENGTH);  // Y up

    // Colors
    ImU32 x_color = IM_COL32(220, 60, 60, 255);       // Red for X
    ImU32 y_color = IM_COL32(60, 180, 60, 255);       // Green for Y
    ImU32 center_color = IM_COL32(200, 200, 200, 255); // White for uniform
    ImU32 x_hover = IM_COL32(255, 120, 120, 255);
    ImU32 y_hover = IM_COL32(120, 255, 120, 255);
    ImU32 center_hover = IM_COL32(255, 255, 255, 255);

    // Handle dragging
    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Calculate mouse delta from drag start
            float dx = mouse.x - m_drag_start_mouse.x;
            float dy = m_drag_start_mouse.y - mouse.y;  // Invert Y

            // Scale sensitivity
            float sensitivity = 0.01f;

            switch (m_drag_axis) {
                case DragAxis::X:
                    transform.scale_x = std::max(0.01f, m_start_scale_x + dx * sensitivity);
                    break;
                case DragAxis::Y:
                    transform.scale_y = std::max(0.01f, m_start_scale_y + dy * sensitivity);
                    break;
                case DragAxis::Uniform: {
                    // Use the larger delta for uniform scaling
                    float delta = (std::abs(dx) > std::abs(dy)) ? dx : dy;
                    float scale_factor = 1.0f + delta * sensitivity;
                    scale_factor = std::max(0.01f, scale_factor);
                    transform.scale_x = m_start_scale_x * scale_factor;
                    transform.scale_y = m_start_scale_y * scale_factor;
                    break;
                }
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

        // Check center handle first
        if (is_mouse_in_box(mouse, center, CENTER_SIZE)) {
            m_hover_axis = DragAxis::Uniform;
        }
        // Check X handle
        else if (is_mouse_in_box(mouse, x_handle, HANDLE_SIZE)) {
            m_hover_axis = DragAxis::X;
        }
        // Check Y handle
        else if (is_mouse_in_box(mouse, y_handle, HANDLE_SIZE)) {
            m_hover_axis = DragAxis::Y;
        }

        // Start drag on click
        if (m_hover_axis != DragAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_is_dragging = true;
            m_drag_axis = m_hover_axis;
            m_start_transform = transform;
            m_start_scale_x = transform.scale_x;
            m_start_scale_y = transform.scale_y;
            m_drag_start_mouse = mouse;
            result.just_started = true;
            result.is_active = true;
        }
    }

    // Determine colors based on hover/drag state
    ImU32 draw_x_color = (m_hover_axis == DragAxis::X || m_drag_axis == DragAxis::X) ? x_hover : x_color;
    ImU32 draw_y_color = (m_hover_axis == DragAxis::Y || m_drag_axis == DragAxis::Y) ? y_hover : y_color;
    ImU32 draw_center_color = (m_hover_axis == DragAxis::Uniform || m_drag_axis == DragAxis::Uniform) ? center_hover : center_color;

    float line_thickness = 2.0f;

    // Draw X axis line and handle
    draw_list->AddLine(center, x_handle, draw_x_color, line_thickness);
    draw_list->AddRectFilled(
        ImVec2(x_handle.x - HANDLE_SIZE, x_handle.y - HANDLE_SIZE),
        ImVec2(x_handle.x + HANDLE_SIZE, x_handle.y + HANDLE_SIZE),
        draw_x_color
    );

    // Draw Y axis line and handle
    draw_list->AddLine(center, y_handle, draw_y_color, line_thickness);
    draw_list->AddRectFilled(
        ImVec2(y_handle.x - HANDLE_SIZE, y_handle.y - HANDLE_SIZE),
        ImVec2(y_handle.x + HANDLE_SIZE, y_handle.y + HANDLE_SIZE),
        draw_y_color
    );

    // Draw center handle (for uniform scale)
    draw_list->AddRectFilled(
        ImVec2(center.x - CENTER_SIZE, center.y - CENTER_SIZE),
        ImVec2(center.x + CENTER_SIZE, center.y + CENTER_SIZE),
        draw_center_color
    );

    // Show scale values when dragging
    if (m_is_dragging) {
        char scale_text[64];
        snprintf(scale_text, sizeof(scale_text), "Scale: %.2f x %.2f", transform.scale_x, transform.scale_y);
        ImVec2 text_pos(center.x + CENTER_SIZE + 10, center.y - CENTER_SIZE);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), scale_text);
    }

    return result;
}

bool ScaleGizmo::is_mouse_in_box(ImVec2 mouse, ImVec2 center, float half_size) const {
    return mouse.x >= center.x - half_size && mouse.x <= center.x + half_size &&
           mouse.y >= center.y - half_size && mouse.y <= center.y + half_size;
}

} // namespace editor
