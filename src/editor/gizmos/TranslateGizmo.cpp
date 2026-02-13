#include "TranslateGizmo.h"
#include "engine/core/MathConstants.h"
#include <cmath>
#include <algorithm>

namespace editor {

GizmoResult TranslateGizmo::render(
    ImDrawList* draw_list,
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    entt::entity /*entity*/,
    engine::Transform& transform,
    float camera_x,
    float camera_y,
    float zoom,
    GizmoSpace space
) {
    GizmoResult result;

    // Position gizmo at WORLD coordinates (not local)
    ImVec2 center = world_to_screen(
        transform.world_x, transform.world_y,
        viewport_pos, viewport_size,
        camera_x, camera_y, zoom
    );

    // Check if gizmo is visible in viewport
    if (center.x < viewport_pos.x - AXIS_LENGTH * 2 ||
        center.x > viewport_pos.x + viewport_size.x + AXIS_LENGTH * 2 ||
        center.y < viewport_pos.y - AXIS_LENGTH * 2 ||
        center.y > viewport_pos.y + viewport_size.y + AXIS_LENGTH * 2) {
        return result;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    // Compute rotation for axis orientation
    float rot_rad = 0.0f;
    if (space == GizmoSpace::Local) {
        rot_rad = transform.world_rotation * engine::DEG_TO_RAD;
    }
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    // Screen-space axis directions
    // World X (1,0) in screen = (1, 0)
    // Local X (cos(wr), sin(wr)) in screen = (cos(wr), -sin(wr))  [Y inverted]
    float x_dir_sx = cos_r, x_dir_sy = -sin_r;
    float y_dir_sx = -sin_r, y_dir_sy = -cos_r;

    // Axis endpoints
    ImVec2 x_end(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_end(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    // Colors
    ImU32 x_color = IM_COL32(220, 60, 60, 255);
    ImU32 y_color = IM_COL32(60, 180, 60, 255);
    ImU32 xy_color = IM_COL32(255, 220, 60, 200);
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

            // Raw world delta from drag start
            float raw_dx = world_pos.x - m_drag_start_world.x;
            float raw_dy = world_pos.y - m_drag_start_world.y;

            // Compute constrained world delta based on axis and space
            float world_dx = 0, world_dy = 0;

            // Use start transform's world rotation for consistent axis during drag
            float drag_wr = m_start_transform.world_rotation * engine::DEG_TO_RAD;
            float drag_cos = std::cos(drag_wr);
            float drag_sin = std::sin(drag_wr);

            if (space == GizmoSpace::Local) {
                // Local axes in world space
                float lx_wx = drag_cos, lx_wy = drag_sin;
                float ly_wx = -drag_sin, ly_wy = drag_cos;

                switch (m_drag_axis) {
                    case DragAxis::X: {
                        float proj = raw_dx * lx_wx + raw_dy * lx_wy;
                        world_dx = proj * lx_wx;
                        world_dy = proj * lx_wy;
                        break;
                    }
                    case DragAxis::Y: {
                        float proj = raw_dx * ly_wx + raw_dy * ly_wy;
                        world_dx = proj * ly_wx;
                        world_dy = proj * ly_wy;
                        break;
                    }
                    case DragAxis::XY:
                        world_dx = raw_dx;
                        world_dy = raw_dy;
                        break;
                    default:
                        break;
                }
            } else {
                // World axes
                switch (m_drag_axis) {
                    case DragAxis::X: world_dx = raw_dx; break;
                    case DragAxis::Y: world_dy = raw_dy; break;
                    case DragAxis::XY: world_dx = raw_dx; world_dy = raw_dy; break;
                    default: break;
                }
            }

            // Convert world delta to parent-space (local) delta
            // Derive parent's effective transform from start values
            float parent_rot = m_start_transform.world_rotation - m_start_transform.rotation;
            float parent_sx = (std::abs(m_start_transform.scale_x) > 0.0001f)
                ? m_start_transform.world_scale_x / m_start_transform.scale_x : 1.0f;
            float parent_sy = (std::abs(m_start_transform.scale_y) > 0.0001f)
                ? m_start_transform.world_scale_y / m_start_transform.scale_y : 1.0f;

            float inv_rad = -parent_rot * engine::DEG_TO_RAD;
            float cos_inv = std::cos(inv_rad);
            float sin_inv = std::sin(inv_rad);

            float local_dx = (std::abs(parent_sx) > 0.0001f)
                ? (world_dx * cos_inv - world_dy * sin_inv) / parent_sx : 0.0f;
            float local_dy = (std::abs(parent_sy) > 0.0001f)
                ? (world_dx * sin_inv + world_dy * cos_inv) / parent_sy : 0.0f;

            transform.x = m_start_transform.x + local_dx;
            transform.y = m_start_transform.y + local_dy;

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

        // Check center square first (rotated point-in-quad test)
        {
            float rel_x = mouse.x - center.x;
            float rel_y = mouse.y - center.y;
            float proj_x = rel_x * x_dir_sx + rel_y * x_dir_sy;
            float proj_y = rel_x * y_dir_sx + rel_y * y_dir_sy;
            if (proj_x >= 0 && proj_x <= CENTER_SIZE && proj_y >= 0 && proj_y <= CENTER_SIZE) {
                m_hover_axis = DragAxis::XY;
            }
        }

        // Check X axis
        if (m_hover_axis == DragAxis::None &&
            is_mouse_near_line(mouse, center, x_end, HIT_THRESHOLD)) {
            m_hover_axis = DragAxis::X;
        }
        // Check Y axis
        if (m_hover_axis == DragAxis::None &&
            is_mouse_near_line(mouse, center, y_end, HIT_THRESHOLD)) {
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
    {
        ImVec2 tip(x_end.x + ARROW_SIZE * x_dir_sx, x_end.y + ARROW_SIZE * x_dir_sy);
        // Perpendicular in screen space: rotate direction 90 degrees
        ImVec2 base1(x_end.x - 4 * x_dir_sx + 6 * x_dir_sy,
                     x_end.y - 4 * x_dir_sy - 6 * x_dir_sx);
        ImVec2 base2(x_end.x - 4 * x_dir_sx - 6 * x_dir_sy,
                     x_end.y - 4 * x_dir_sy + 6 * x_dir_sx);
        draw_list->AddTriangleFilled(tip, base1, base2, draw_x_color);
    }

    // Y axis line and arrow
    draw_list->AddLine(center, y_end, draw_y_color, line_thickness);
    {
        ImVec2 tip(y_end.x + ARROW_SIZE * y_dir_sx, y_end.y + ARROW_SIZE * y_dir_sy);
        ImVec2 base1(y_end.x - 4 * y_dir_sx + 6 * y_dir_sy,
                     y_end.y - 4 * y_dir_sy - 6 * y_dir_sx);
        ImVec2 base2(y_end.x - 4 * y_dir_sx - 6 * y_dir_sy,
                     y_end.y - 4 * y_dir_sy + 6 * y_dir_sx);
        draw_list->AddTriangleFilled(tip, base1, base2, draw_y_color);
    }

    // Center square for XY movement (rotated quad)
    {
        ImVec2 sq0 = center;
        ImVec2 sq1(center.x + CENTER_SIZE * x_dir_sx, center.y + CENTER_SIZE * x_dir_sy);
        ImVec2 sq2(center.x + CENTER_SIZE * x_dir_sx + CENTER_SIZE * y_dir_sx,
                   center.y + CENTER_SIZE * x_dir_sy + CENTER_SIZE * y_dir_sy);
        ImVec2 sq3(center.x + CENTER_SIZE * y_dir_sx, center.y + CENTER_SIZE * y_dir_sy);
        draw_list->AddQuadFilled(sq0, sq1, sq2, sq3, draw_xy_color);
    }

    return result;
}

bool TranslateGizmo::is_mouse_near_line(ImVec2 mouse, ImVec2 a, ImVec2 b, float threshold) const {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float length_sq = dx * dx + dy * dy;

    if (length_sq < 0.0001f) {
        float dist = std::sqrt((mouse.x - a.x) * (mouse.x - a.x) + (mouse.y - a.y) * (mouse.y - a.y));
        return dist <= threshold;
    }

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
