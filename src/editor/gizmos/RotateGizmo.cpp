#include "RotateGizmo.h"
#include "engine/core/MathConstants.h"
#include <cmath>

namespace editor {

GizmoResult RotateGizmo::render(
    ImDrawList* draw_list,
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

    // Position gizmo at WORLD coordinates (not local)
    ImVec2 center = world_to_screen(
        transform.world_x, transform.world_y,
        viewport_pos, viewport_size,
        camera_x, camera_y, zoom
    );

    // Check if gizmo is visible in viewport
    if (center.x < viewport_pos.x - CIRCLE_RADIUS ||
        center.x > viewport_pos.x + viewport_size.x + CIRCLE_RADIUS ||
        center.y < viewport_pos.y - CIRCLE_RADIUS ||
        center.y > viewport_pos.y + viewport_size.y + CIRCLE_RADIUS) {
        return result;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;

    // Colors
    ImU32 circle_color = IM_COL32(60, 120, 220, 255);      // Blue
    ImU32 circle_hover = IM_COL32(120, 180, 255, 255);
    ImU32 indicator_color = IM_COL32(255, 255, 255, 200);

    // Handle dragging
    if (m_is_dragging) {
        result.is_active = true;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Calculate current angle from center to mouse
            float current_angle = angle_to_point(center, mouse);

            // Calculate angle delta
            float angle_delta = current_angle - m_start_angle;

            // Normalize angle delta to [-180, 180]
            while (angle_delta > 180.0f) angle_delta -= 360.0f;
            while (angle_delta < -180.0f) angle_delta += 360.0f;

            // Apply rotation
            transform.rotation = m_start_rotation + angle_delta;

            // Normalize final rotation to [0, 360)
            while (transform.rotation < 0.0f) transform.rotation += 360.0f;
            while (transform.rotation >= 360.0f) transform.rotation -= 360.0f;

            result.value_changed = true;
        } else {
            // Mouse released - end drag
            m_is_dragging = false;
            result.just_finished = true;
        }
    } else {
        // Not dragging - check for hover and start drag
        m_is_hovering = is_mouse_near_circle(mouse, center, CIRCLE_RADIUS, HIT_THRESHOLD);

        // Start drag on click
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

    // Determine color based on hover/drag state
    ImU32 draw_color = (m_is_hovering || m_is_dragging) ? circle_hover : circle_color;

    // Draw the rotation circle
    draw_list->AddCircle(center, CIRCLE_RADIUS, draw_color, 64, CIRCLE_THICKNESS);

    // Draw rotation indicator line (from center pointing in world rotation direction)
    float rad = transform.world_rotation * engine::DEG_TO_RAD;
    ImVec2 indicator_end(
        center.x + CIRCLE_RADIUS * std::cos(-rad + engine::PI * 0.5f),
        center.y + CIRCLE_RADIUS * std::sin(-rad + engine::PI * 0.5f)
    );
    draw_list->AddLine(center, indicator_end, indicator_color, 2.0f);

    // Draw small circle at indicator end
    draw_list->AddCircleFilled(indicator_end, 5.0f, indicator_color);

    // Draw center point
    draw_list->AddCircleFilled(center, 4.0f, draw_color);

    // Show angle text when dragging
    if (m_is_dragging) {
        char angle_text[32];
        snprintf(angle_text, sizeof(angle_text), "%.1f°", transform.rotation);
        ImVec2 text_pos(center.x + CIRCLE_RADIUS + 10, center.y - 10);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), angle_text);
    }

    return result;
}

float RotateGizmo::angle_to_point(ImVec2 center, ImVec2 point) const {
    float dx = point.x - center.x;
    float dy = point.y - center.y;
    // atan2 returns angle in radians, convert to degrees
    // Note: screen Y is inverted, so we negate dy
    return std::atan2(-dy, dx) * engine::RAD_TO_DEG;
}

bool RotateGizmo::is_mouse_near_circle(ImVec2 mouse, ImVec2 center, float radius, float threshold) const {
    float dx = mouse.x - center.x;
    float dy = mouse.y - center.y;
    float dist_from_center = std::sqrt(dx * dx + dy * dy);
    float dist_from_circle = std::abs(dist_from_center - radius);
    return dist_from_circle <= threshold;
}

} // namespace editor
