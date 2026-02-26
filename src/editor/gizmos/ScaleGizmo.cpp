#include "ScaleGizmo.h"
#include "GizmoColors.h"
#include "engine/core/MathConstants.h"
#include <cmath>
#include <algorithm>

namespace editor {

GizmoResult ScaleGizmo::update(
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

    ImVec2 x_handle(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_handle(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float raw_dx = mouse.x - m_drag_start_mouse.x;
            float raw_dy = mouse.y - m_drag_start_mouse.y;

            auto drag_rot = RotationCache::from_degrees(m_start_transform.world_rotation);
            float drag_cos = drag_rot.cos_r;
            float drag_sin = drag_rot.sin_r;

            float x_proj, y_proj;
            if (space == GizmoSpace::Local) {
                x_proj = raw_dx * drag_cos + raw_dy * (-drag_sin);
                y_proj = -(raw_dx * drag_sin + raw_dy * drag_cos);
            } else {
                x_proj = raw_dx;
                y_proj = -raw_dy;
            }

            constexpr float SENSITIVITY = 0.01f;

            switch (m_drag_axis) {
                case DragAxis::X:
                    transform.scale_x = std::max(0.01f, m_start_scale_x + x_proj * SENSITIVITY);
                    break;
                case DragAxis::Y:
                    transform.scale_y = std::max(0.01f, m_start_scale_y + y_proj * SENSITIVITY);
                    break;
                case DragAxis::Uniform: {
                    float delta = (std::abs(x_proj) > std::abs(y_proj)) ? x_proj : y_proj;
                    float scale_factor = std::max(0.01f, 1.0f + delta * SENSITIVITY);
                    transform.scale_x = m_start_scale_x * scale_factor;
                    transform.scale_y = m_start_scale_y * scale_factor;
                    break;
                }
                default:
                    break;
            }

            result.value_changed = true;
        } else {
            m_is_dragging = false;
            m_drag_axis = DragAxis::None;
            result.just_finished = true;
        }
    } else {
        m_hover_axis = DragAxis::None;

        {
            float dx = mouse.x - center.x;
            float dy = mouse.y - center.y;
            if (std::sqrt(dx * dx + dy * dy) <= CENTER_SIZE) {
                m_hover_axis = DragAxis::Uniform;
            }
        }

        if (m_hover_axis == DragAxis::None) {
            float dx = mouse.x - x_handle.x;
            float dy = mouse.y - x_handle.y;
            if (std::sqrt(dx * dx + dy * dy) <= HANDLE_SIZE * 1.5f) {
                m_hover_axis = DragAxis::X;
            }
        }

        if (m_hover_axis == DragAxis::None) {
            float dx = mouse.x - y_handle.x;
            float dy = mouse.y - y_handle.y;
            if (std::sqrt(dx * dx + dy * dy) <= HANDLE_SIZE * 1.5f) {
                m_hover_axis = DragAxis::Y;
            }
        }

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

    m_is_hovering = (m_hover_axis != DragAxis::None);

    return result;
}

void ScaleGizmo::render(
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

    ImVec2 x_handle(center.x + AXIS_LENGTH * x_dir_sx, center.y + AXIS_LENGTH * x_dir_sy);
    ImVec2 y_handle(center.x + AXIS_LENGTH * y_dir_sx, center.y + AXIS_LENGTH * y_dir_sy);

    ImU32 draw_x_color = (m_hover_axis == DragAxis::X || m_drag_axis == DragAxis::X)
        ? gizmo_colors::X_AXIS_HOVER : gizmo_colors::X_AXIS;
    ImU32 draw_y_color = (m_hover_axis == DragAxis::Y || m_drag_axis == DragAxis::Y)
        ? gizmo_colors::Y_AXIS_HOVER : gizmo_colors::Y_AXIS;
    ImU32 draw_center_color = (m_hover_axis == DragAxis::Uniform || m_drag_axis == DragAxis::Uniform)
        ? gizmo_colors::UNIFORM_SCALE_HOVER : gizmo_colors::UNIFORM_SCALE;

    constexpr float LINE_THICKNESS = 2.0f;

    draw_list->AddLine(center, x_handle, draw_x_color, LINE_THICKNESS);
    {
        float hs = HANDLE_SIZE;
        ImVec2 h0(x_handle.x - hs * x_dir_sx - hs * x_dir_sy, x_handle.y - hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h1(x_handle.x + hs * x_dir_sx - hs * x_dir_sy, x_handle.y + hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h2(x_handle.x + hs * x_dir_sx + hs * x_dir_sy, x_handle.y + hs * x_dir_sy - hs * x_dir_sx);
        ImVec2 h3(x_handle.x - hs * x_dir_sx + hs * x_dir_sy, x_handle.y - hs * x_dir_sy - hs * x_dir_sx);
        draw_list->AddQuadFilled(h0, h1, h2, h3, draw_x_color);
    }

    draw_list->AddLine(center, y_handle, draw_y_color, LINE_THICKNESS);
    {
        float hs = HANDLE_SIZE;
        ImVec2 h0(y_handle.x - hs * x_dir_sx - hs * x_dir_sy, y_handle.y - hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h1(y_handle.x + hs * x_dir_sx - hs * x_dir_sy, y_handle.y + hs * x_dir_sy + hs * x_dir_sx);
        ImVec2 h2(y_handle.x + hs * x_dir_sx + hs * x_dir_sy, y_handle.y + hs * x_dir_sy - hs * x_dir_sx);
        ImVec2 h3(y_handle.x - hs * x_dir_sx + hs * x_dir_sy, y_handle.y - hs * x_dir_sy - hs * x_dir_sx);
        draw_list->AddQuadFilled(h0, h1, h2, h3, draw_y_color);
    }

    draw_list->AddRectFilled(
        ImVec2(center.x - CENTER_SIZE, center.y - CENTER_SIZE),
        ImVec2(center.x + CENTER_SIZE, center.y + CENTER_SIZE),
        draw_center_color
    );

    if (m_is_dragging) {
        char scale_text[64];
        snprintf(scale_text, sizeof(scale_text), "%.2f x %.2f", transform.scale_x, transform.scale_y);
        ImVec2 text_pos(center.x + CENTER_SIZE + 10, center.y - CENTER_SIZE);
        draw_list->AddText(text_pos, gizmo_colors::TEXT, scale_text);
    }
}

}
