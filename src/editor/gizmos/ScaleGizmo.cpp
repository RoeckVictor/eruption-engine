#include "ScaleGizmo.h"
#include <cmath>
#include <algorithm>

namespace editor {

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
}

GizmoResult ScaleGizmo::render(
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
        rot_rad = transform.world_rotation * DEG_TO_RAD;
    }
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    // Screen-space axis directions (Y inverted for screen)
    float x_dir_sx = cos_r, x_dir_sy = -sin_r;
    float y_dir_sx = -sin_r, y_dir_sy = -cos_r;

    // Calculate handle positions along rotated axes
    ImVec2 x_handle(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_handle(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    // Colors
    ImU32 x_color = IM_COL32(220, 60, 60, 255);
    ImU32 y_color = IM_COL32(60, 180, 60, 255);
    ImU32 center_color = IM_COL32(200, 200, 200, 255);
    ImU32 x_hover = IM_COL32(255, 120, 120, 255);
    ImU32 y_hover = IM_COL32(120, 255, 120, 255);
    ImU32 center_hover = IM_COL32(255, 255, 255, 255);

    // Handle dragging
    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Raw screen delta from drag start
            float raw_dx = mouse.x - m_drag_start_mouse.x;
            float raw_dy = mouse.y - m_drag_start_mouse.y;

            // Project onto screen-space axis directions
            // Use start transform's rotation for consistent axis during drag
            float drag_wr = m_start_transform.world_rotation * DEG_TO_RAD;
            float drag_cos = std::cos(drag_wr);
            float drag_sin = std::sin(drag_wr);

            float x_proj, y_proj;
            if (space == GizmoSpace::Local) {
                // Project screen delta onto rotated screen-space axis directions
                x_proj = raw_dx * drag_cos + raw_dy * (-drag_sin);
                y_proj = -(raw_dx * drag_sin + raw_dy * drag_cos);
            } else {
                x_proj = raw_dx;
                y_proj = -raw_dy;  // Invert Y (screen down = negative)
            }

            float sensitivity = 0.01f;

            switch (m_drag_axis) {
                case DragAxis::X:
                    transform.scale_x = std::max(0.01f, m_start_scale_x + x_proj * sensitivity);
                    break;
                case DragAxis::Y:
                    transform.scale_y = std::max(0.01f, m_start_scale_y + y_proj * sensitivity);
                    break;
                case DragAxis::Uniform: {
                    float delta = (std::abs(x_proj) > std::abs(y_proj)) ? x_proj : y_proj;
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

        // Check center handle first (use circular distance for rotation-friendly hit test)
        {
            float dx = mouse.x - center.x;
            float dy = mouse.y - center.y;
            if (std::sqrt(dx * dx + dy * dy) <= CENTER_SIZE) {
                m_hover_axis = DragAxis::Uniform;
            }
        }

        // Check X handle (circular distance from handle center)
        if (m_hover_axis == DragAxis::None) {
            float dx = mouse.x - x_handle.x;
            float dy = mouse.y - x_handle.y;
            if (std::sqrt(dx * dx + dy * dy) <= HANDLE_SIZE * 1.5f) {
                m_hover_axis = DragAxis::X;
            }
        }

        // Check Y handle
        if (m_hover_axis == DragAxis::None) {
            float dx = mouse.x - y_handle.x;
            float dy = mouse.y - y_handle.y;
            if (std::sqrt(dx * dx + dy * dy) <= HANDLE_SIZE * 1.5f) {
                m_hover_axis = DragAxis::Y;
            }
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

    // Draw X axis line and handle (rotated)
    draw_list->AddLine(center, x_handle, draw_x_color, line_thickness);
    {
        // Rotated square handle at X endpoint
        float hs = HANDLE_SIZE;
        ImVec2 h0(x_handle.x - hs * x_dir_sx - hs * x_dir_sy, x_handle.y - hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h1(x_handle.x + hs * x_dir_sx - hs * x_dir_sy, x_handle.y + hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h2(x_handle.x + hs * x_dir_sx + hs * x_dir_sy, x_handle.y + hs * x_dir_sy - hs * x_dir_sx);
        ImVec2 h3(x_handle.x - hs * x_dir_sx + hs * x_dir_sy, x_handle.y - hs * x_dir_sy - hs * x_dir_sx);
        draw_list->AddQuadFilled(h0, h1, h2, h3, draw_x_color);
    }

    // Draw Y axis line and handle (rotated)
    draw_list->AddLine(center, y_handle, draw_y_color, line_thickness);
    {
        float hs = HANDLE_SIZE;
        ImVec2 h0(y_handle.x - hs * x_dir_sx - hs * x_dir_sy, y_handle.y - hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h1(y_handle.x + hs * x_dir_sx - hs * x_dir_sy, y_handle.y + hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h2(y_handle.x + hs * x_dir_sx + hs * x_dir_sy, y_handle.y + hs * x_dir_sy - hs * x_dir_sx);
        ImVec2 h3(y_handle.x - hs * x_dir_sx + hs * x_dir_sy, y_handle.y - hs * x_dir_sy - hs * x_dir_sx);
        draw_list->AddQuadFilled(h0, h1, h2, h3, draw_y_color);
    }

    // Draw center handle (for uniform scale) - stays axis-aligned
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
