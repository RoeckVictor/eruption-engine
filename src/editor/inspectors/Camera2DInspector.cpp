#include "Camera2DInspector.h"
#include "engine/render/Camera2D.h"
#include <imgui.h>

namespace editor {

bool Camera2DInspector::draw(engine::render::Camera2D& camera) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &camera.enabled)) {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Position
    ImGui::Text("Position");
    float pos[2] = {camera.x, camera.y};
    if (ImGui::DragFloat2("##CamPos", pos, 1.0f)) {
        camera.x = pos[0];
        camera.y = pos[1];
        changed = true;
    }

    ImGui::Spacing();

    // Zoom controls with visual feedback
    ImGui::Text("Zoom: %.2f", camera.zoom);
    if (ImGui::SliderFloat("##Zoom", &camera.zoom, camera.min_zoom, camera.max_zoom, "%.2f")) {
        changed = true;
    }

    // Zoom presets
    ImGui::SameLine();
    if (ImGui::SmallButton("1:1")) {
        camera.zoom = 1.0f;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("2x")) {
        camera.zoom = 2.0f;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("4x")) {
        camera.zoom = 4.0f;
        changed = true;
    }

    ImGui::Spacing();

    // Zoom limits
    ImGui::Text("Zoom Limits");
    float limits[2] = {camera.min_zoom, camera.max_zoom};
    if (ImGui::DragFloat2("##ZoomLimits", limits, 0.1f, 0.01f, 20.0f, "%.2f")) {
        camera.min_zoom = limits[0];
        camera.max_zoom = limits[1];
        // Clamp current zoom to new limits
        if (camera.zoom < camera.min_zoom) camera.zoom = camera.min_zoom;
        if (camera.zoom > camera.max_zoom) camera.zoom = camera.max_zoom;
        changed = true;
    }

    ImGui::Spacing();

    // Smoothing
    ImGui::Text("Smoothing");
    if (ImGui::SliderFloat("##Smoothing", &camera.smoothing, 0.0f, 20.0f, "%.1f")) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Lower = snappier, Higher = smoother (0 = instant)");
    }

    return changed;
}

} // namespace editor
