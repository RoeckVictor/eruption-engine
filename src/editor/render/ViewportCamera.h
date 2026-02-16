#pragma once

#include <imgui.h>
#include <algorithm>

namespace editor {

/// Reusable camera state and input handler for 2D editor viewports.
/// Used by ViewportPanel, PrefabEditorPanel, and any other panel that needs
/// a pannable/zoomable view.
struct ViewportCamera {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    float min_zoom = 0.1f;
    float max_zoom = 20.0f;
    float default_zoom = 1.0f;

    ViewportCamera() = default;
    ViewportCamera(float x_, float y_, float zoom_, float min_zoom_, float max_zoom_, float default_zoom_)
        : x(x_), y(y_), zoom(zoom_), min_zoom(min_zoom_), max_zoom(max_zoom_), default_zoom(default_zoom_) {}

    /// Handle scroll-to-zoom and middle-mouse-button panning.
    /// Call this when the viewport is hovered and no gizmo is active.
    void handle_input() {
        ImGuiIO& io = ImGui::GetIO();

        // Zoom with scroll wheel
        if (io.MouseWheel != 0.0f) {
            constexpr float zoom_factor = 1.1f;
            if (io.MouseWheel > 0) {
                zoom *= zoom_factor;
            } else {
                zoom /= zoom_factor;
            }
            zoom = std::clamp(zoom, min_zoom, max_zoom);
        }

        // Pan with middle mouse button
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            m_is_panning = true;
            m_pan_start_x = io.MousePos.x;
            m_pan_start_y = io.MousePos.y;
        }

        if (m_is_panning) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                float dx = io.MousePos.x - m_pan_start_x;
                float dy = io.MousePos.y - m_pan_start_y;
                x -= dx / zoom;
                y += dy / zoom;  // Flip Y
                m_pan_start_x = io.MousePos.x;
                m_pan_start_y = io.MousePos.y;
            } else {
                m_is_panning = false;
            }
        }

        // Reset camera with Home key
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            reset();
        }
    }

    bool is_panning() const { return m_is_panning; }

    /// Reset camera to default position and zoom.
    void reset() {
        x = 0.0f;
        y = 0.0f;
        zoom = default_zoom;
    }

private:
    bool m_is_panning = false;
    float m_pan_start_x = 0.0f;
    float m_pan_start_y = 0.0f;
};

} // namespace editor
