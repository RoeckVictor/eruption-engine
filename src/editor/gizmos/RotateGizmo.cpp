#include "RotateGizmo.h"
#include "GizmoColors.h"
#include "engine/core/MathConstants.h"
#include <cmath>

namespace editor {

GizmoResult RotateGizmo::update(
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    entt::entity /*entity*/,
    engine::Transform& transform,
    float camera_x,
    float camera_y,
    float zoom,
    GizmoSpace /*space*/
) {
    GizmoResult result;

    auto coord = make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom);
    ImVec2 center = coord.world_to_screen(transform.world_x, transform.world_y);

    if (!is_visible_in_viewport(center, viewport_pos, viewport_size, CIRCLE_RADIUS)) {
        return result;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float current_angle = angle_to_point(center, mouse);
            float angle_delta = current_angle - m_start_angle;

            while (angle_delta > 180.0f) angle_delta -= 360.0f;
            while (angle_delta < -180.0f) angle_delta += 360.0f;

            transform.rotation = m_start_rotation + angle_delta;

            while (transform.rotation < 0.0f) transform.rotation += 360.0f;
            while (transform.rotation >= 360.0f) transform.rotation -= 360.0f;

            result.value_changed = true;
        } else {
            m_is_dragging = false;
            result.just_finished = true;
        }
    } else {
        m_is_hovering = is_mouse_near_circle(mouse, center, CIRCLE_RADIUS, HIT_THRESHOLD);

        if (m_is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_is_dragging = true;
            m_start_transform = transform;
            m_start_angle = angle_to_point(center, mouse);
            m_start_rotation = transform.rotation;
            m_drag_start_mouse = mouse;
            result.just_started = true;
            result.is_active = true;
        }
    }

    return result;
}

void RotateGizmo::render(
    ImDrawList* draw_list,
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    const engine::Transform& transform,
    float camera_x,
    float camera_y,
    float zoom,
    GizmoSpace /*space*/
) {
    auto coord = make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom);
    ImVec2 center = coord.world_to_screen(transform.world_x, transform.world_y);

    if (!is_visible_in_viewport(center, viewport_pos, viewport_size, CIRCLE_RADIUS)) {
        return;
    }

    ImU32 draw_color = (m_is_hovering || m_is_dragging)
        ? gizmo_colors::ROTATION_HOVER : gizmo_colors::ROTATION;

    draw_list->AddCircle(center, CIRCLE_RADIUS, draw_color, 64, CIRCLE_THICKNESS);

    float rad = transform.world_rotation * engine::DEG_TO_RAD;
    ImVec2 indicator_end(
        center.x + CIRCLE_RADIUS * std::cos(-rad + engine::PI * 0.5f),
        center.y + CIRCLE_RADIUS * std::sin(-rad + engine::PI * 0.5f)
    );
    draw_list->AddLine(center, indicator_end, gizmo_colors::INDICATOR, 2.0f);
    draw_list->AddCircleFilled(indicator_end, 5.0f, gizmo_colors::INDICATOR);
    draw_list->AddCircleFilled(center, 4.0f, draw_color);

    if (m_is_dragging) {
        char angle_text[32];
        snprintf(angle_text, sizeof(angle_text), "%.1f", transform.rotation);
        ImVec2 text_pos(center.x + CIRCLE_RADIUS + 10, center.y - 10);
        draw_list->AddText(text_pos, gizmo_colors::TEXT, angle_text);
    }
}

float RotateGizmo::angle_to_point(ImVec2 center, ImVec2 point) const {
    float dx = point.x - center.x;
    float dy = point.y - center.y;
    return std::atan2(-dy, dx) * engine::RAD_TO_DEG;
}

bool RotateGizmo::is_mouse_near_circle(ImVec2 mouse, ImVec2 center, float radius, float threshold) const {
    float dx = mouse.x - center.x;
    float dy = mouse.y - center.y;
    float dist_from_center = std::sqrt(dx * dx + dy * dy);
    float dist_from_circle = std::abs(dist_from_center - radius);
    return dist_from_circle <= threshold;
}

}
