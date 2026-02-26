#include "TranslateGizmo.h"
#include "GizmoColors.h"
#include "engine/core/MathConstants.h"
#include <cmath>
#include <algorithm>

namespace editor {

GizmoResult TranslateGizmo::update(
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

    auto coord = make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom);
    ImVec2 center = coord.world_to_screen(transform.world_x, transform.world_y);

    if (!is_visible_in_viewport(center, viewport_pos, viewport_size, AXIS_LENGTH * 2)) {
        return result;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    auto rot = get_rotation_cache(transform, space);
    float x_dir_sx = rot.x_dir_sx(), x_dir_sy = rot.x_dir_sy();
    float y_dir_sx = rot.y_dir_sx(), y_dir_sy = rot.y_dir_sy();

    ImVec2 x_end(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_end(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 world_pos = coord.screen_to_world(mouse.x, mouse.y);

            float raw_dx = world_pos.x - m_drag_start_world.x;
            float raw_dy = world_pos.y - m_drag_start_world.y;

            float world_dx = 0, world_dy = 0;

            auto drag_rot = RotationCache::from_degrees(m_start_transform.world_rotation);
            float drag_cos = drag_rot.cos_r;
            float drag_sin = drag_rot.sin_r;

            if (space == GizmoSpace::Local) {
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
                switch (m_drag_axis) {
                    case DragAxis::X: world_dx = raw_dx; break;
                    case DragAxis::Y: world_dy = raw_dy; break;
                    case DragAxis::XY: world_dx = raw_dx; world_dy = raw_dy; break;
                    default: break;
                }
            }

            float parent_rot = m_start_transform.world_rotation - m_start_transform.rotation;
            float parent_sx = (std::abs(m_start_transform.scale_x) > 0.0001f)
                ? m_start_transform.world_scale_x / m_start_transform.scale_x : 1.0f;
            float parent_sy = (std::abs(m_start_transform.scale_y) > 0.0001f)
                ? m_start_transform.world_scale_y / m_start_transform.scale_y : 1.0f;

            auto inv_rot = RotationCache::from_degrees(-parent_rot);
            float cos_inv = inv_rot.cos_r;
            float sin_inv = inv_rot.sin_r;

            float local_dx = (std::abs(parent_sx) > 0.0001f)
                ? (world_dx * cos_inv - world_dy * sin_inv) / parent_sx : 0.0f;
            float local_dy = (std::abs(parent_sy) > 0.0001f)
                ? (world_dx * sin_inv + world_dy * cos_inv) / parent_sy : 0.0f;

            transform.x = m_start_transform.x + local_dx;
            transform.y = m_start_transform.y + local_dy;

            result.value_changed = true;
        } else {
            m_is_dragging = false;
            m_drag_axis = DragAxis::None;
            result.just_finished = true;
        }
    } else {
        m_hover_axis = DragAxis::None;

        {
            float rel_x = mouse.x - center.x;
            float rel_y = mouse.y - center.y;
            float proj_x = rel_x * x_dir_sx + rel_y * x_dir_sy;
            float proj_y = rel_x * y_dir_sx + rel_y * y_dir_sy;
            if (proj_x >= 0 && proj_x <= CENTER_SIZE && proj_y >= 0 && proj_y <= CENTER_SIZE) {
                m_hover_axis = DragAxis::XY;
            }
        }

        if (m_hover_axis == DragAxis::None &&
            is_mouse_near_line(mouse, center, x_end, HIT_THRESHOLD)) {
            m_hover_axis = DragAxis::X;
        }
        if (m_hover_axis == DragAxis::None &&
            is_mouse_near_line(mouse, center, y_end, HIT_THRESHOLD)) {
            m_hover_axis = DragAxis::Y;
        }

        if (m_hover_axis != DragAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_is_dragging = true;
            m_drag_axis = m_hover_axis;
            m_start_transform = transform;
            m_drag_start_mouse = mouse;
            m_drag_start_world = coord.screen_to_world(mouse.x, mouse.y);
            result.just_started = true;
            result.is_active = true;
        }
    }

    m_is_hovering = (m_hover_axis != DragAxis::None);

    return result;
}

void TranslateGizmo::render(
    ImDrawList* draw_list,
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    const engine::Transform& transform,
    float camera_x,
    float camera_y,
    float zoom,
    GizmoSpace space
) {
    auto coord = make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom);
    ImVec2 center = coord.world_to_screen(transform.world_x, transform.world_y);

    if (!is_visible_in_viewport(center, viewport_pos, viewport_size, AXIS_LENGTH * 2)) {
        return;
    }

    auto rot = get_rotation_cache(transform, space);
    float x_dir_sx = rot.x_dir_sx(), x_dir_sy = rot.x_dir_sy();
    float y_dir_sx = rot.y_dir_sx(), y_dir_sy = rot.y_dir_sy();

    ImVec2 x_end(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_end(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    ImU32 draw_x_color = (m_hover_axis == DragAxis::X || m_drag_axis == DragAxis::X)
        ? gizmo_colors::X_AXIS_HOVER : gizmo_colors::X_AXIS;
    ImU32 draw_y_color = (m_hover_axis == DragAxis::Y || m_drag_axis == DragAxis::Y)
        ? gizmo_colors::Y_AXIS_HOVER : gizmo_colors::Y_AXIS;
    ImU32 draw_xy_color = (m_hover_axis == DragAxis::XY || m_drag_axis == DragAxis::XY)
        ? gizmo_colors::XY_PLANE_HOVER : gizmo_colors::XY_PLANE;

    constexpr float LINE_THICKNESS = 3.0f;

    draw_list->AddLine(center, x_end, draw_x_color, LINE_THICKNESS);
    {
        ImVec2 tip(x_end.x + ARROW_SIZE * x_dir_sx, x_end.y + ARROW_SIZE * x_dir_sy);
        ImVec2 base1(x_end.x - 4 * x_dir_sx + 6 * x_dir_sy,
                     x_end.y - 4 * x_dir_sy - 6 * x_dir_sx);
        ImVec2 base2(x_end.x - 4 * x_dir_sx - 6 * x_dir_sy,
                     x_end.y - 4 * x_dir_sy + 6 * x_dir_sx);
        draw_list->AddTriangleFilled(tip, base1, base2, draw_x_color);
    }

    draw_list->AddLine(center, y_end, draw_y_color, LINE_THICKNESS);
    {
        ImVec2 tip(y_end.x + ARROW_SIZE * y_dir_sx, y_end.y + ARROW_SIZE * y_dir_sy);
        ImVec2 base1(y_end.x - 4 * y_dir_sx + 6 * y_dir_sy,
                     y_end.y - 4 * y_dir_sy - 6 * y_dir_sx);
        ImVec2 base2(y_end.x - 4 * y_dir_sx - 6 * y_dir_sy,
                     y_end.y - 4 * y_dir_sy + 6 * y_dir_sx);
        draw_list->AddTriangleFilled(tip, base1, base2, draw_y_color);
    }

    {
        ImVec2 sq0 = center;
        ImVec2 sq1(center.x + CENTER_SIZE * x_dir_sx, center.y + CENTER_SIZE * x_dir_sy);
        ImVec2 sq2(center.x + CENTER_SIZE * x_dir_sx + CENTER_SIZE * y_dir_sx,
                   center.y + CENTER_SIZE * x_dir_sy + CENTER_SIZE * y_dir_sy);
        ImVec2 sq3(center.x + CENTER_SIZE * y_dir_sx, center.y + CENTER_SIZE * y_dir_sy);
        draw_list->AddQuadFilled(sq0, sq1, sq2, sq3, draw_xy_color);
    }
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

}
