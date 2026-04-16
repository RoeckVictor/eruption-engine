#pragma once

#include <imgui.h>
#include <cmath>

namespace editor {

// Unified coordinate transformation utilities for editor viewports
// Handles world-to-screen and screen-to-world conversions with consistent
// camera handling across all editor components (viewports, gizmos, panels)
struct CoordinateTransform {
    float screen_center_x;
    float screen_center_y;
    float viewport_width;
    float viewport_height;
    float camera_x;
    float camera_y;
    float zoom;

    CoordinateTransform(ImVec2 viewport_pos, ImVec2 viewport_size,
                        float cam_x, float cam_y, float cam_zoom)
        : screen_center_x(viewport_pos.x + viewport_size.x * 0.5f)
        , screen_center_y(viewport_pos.y + viewport_size.y * 0.5f)
        , viewport_width(viewport_size.x)
        , viewport_height(viewport_size.y)
        , camera_x(cam_x)
        , camera_y(cam_y)
        , zoom(cam_zoom)
    {}

    CoordinateTransform(float center_x, float center_y, float vp_width, float vp_height,
                        float cam_x, float cam_y, float cam_zoom)
        : screen_center_x(center_x)
        , screen_center_y(center_y)
        , viewport_width(vp_width)
        , viewport_height(vp_height)
        , camera_x(cam_x)
        , camera_y(cam_y)
        , zoom(cam_zoom)
    {}

    ImVec2 world_to_screen(float world_x, float world_y) const {
        float screen_x = screen_center_x + (world_x - camera_x) * zoom;
        float screen_y = screen_center_y - (world_y - camera_y) * zoom;
        return ImVec2(screen_x, screen_y);
    }

    ImVec2 screen_to_world(float screen_x, float screen_y) const {
        float world_x = camera_x + (screen_x - screen_center_x) / zoom;
        float world_y = camera_y - (screen_y - screen_center_y) / zoom;
        return ImVec2(world_x, world_y);
    }

    ImVec2 operator()(float world_x, float world_y) const {
        return world_to_screen(world_x, world_y);
    }

    bool is_visible(ImVec2 screen_pos, ImVec2 viewport_pos, ImVec2 viewport_size, float margin = 0.0f) const {
        return screen_pos.x >= viewport_pos.x - margin &&
               screen_pos.x <= viewport_pos.x + viewport_size.x + margin &&
               screen_pos.y >= viewport_pos.y - margin &&
               screen_pos.y <= viewport_pos.y + viewport_size.y + margin;
    }
};

using WorldToScreen = CoordinateTransform;

// ─────────────────────────────────────────────────────────────────────────────
// Screen-canvas coordinate transform for panels that display ScreenRect
// entities with zoom/pan (ScreenPanel, PrefabEditorPanel).
//
// "screen coords" = reference-resolution space (e.g. 1920×1080)
// "canvas coords" = ImGui pixel space inside the panel
// ─────────────────────────────────────────────────────────────────────────────
struct ScreenCanvasTransform {
    float zoom       = 1.0f;
    float pan_x      = 0.0f;
    float pan_y      = 0.0f;
    float ref_width  = 1920.0f;
    float ref_height = 1080.0f;

    // Convert a point in reference-resolution space to ImGui canvas space.
    ImVec2 to_canvas(float sx, float sy,
                     ImVec2 canvas_pos, ImVec2 canvas_size) const {
        float center_x = canvas_pos.x + canvas_size.x * 0.5f;
        float center_y = canvas_pos.y + canvas_size.y * 0.5f;
        float ref_cx   = ref_width  * 0.5f;
        float ref_cy   = ref_height * 0.5f;
        return ImVec2(center_x + ((sx - ref_cx) - pan_x) * zoom,
                      center_y + ((sy - ref_cy) - pan_y) * zoom);
    }

    // Convert a local mouse position (relative to canvas top-left) back to
    // reference-resolution coordinates.
    void to_screen(ImVec2 local_mouse, ImVec2 canvas_size,
                   float& out_sx, float& out_sy) const {
        float center_x = canvas_size.x * 0.5f;
        float center_y = canvas_size.y * 0.5f;
        float ref_cx   = ref_width  * 0.5f;
        float ref_cy   = ref_height * 0.5f;
        out_sx = ((local_mouse.x - center_x) / zoom) + pan_x + ref_cx;
        out_sy = ((local_mouse.y - center_y) / zoom) + pan_y + ref_cy;
    }

    // Set zoom so that the full reference rect fits inside (canvas_w × canvas_h)
    // with a small margin.
    void fit(float canvas_w, float canvas_h) {
        if (canvas_w <= 0 || canvas_h <= 0) return;
        float sx = canvas_w / ref_width;
        float sy = canvas_h / ref_height;
        zoom  = std::min(sx, sy) * 0.95f;
        pan_x = 0.0f;
        pan_y = 0.0f;
    }

    void reset() {
        zoom  = 1.0f;
        pan_x = 0.0f;
        pan_y = 0.0f;
    }
};

// Rotation helper: precomputed sin/cos for screen-space axis directions.
struct RotationCache {
    float cos_r;
    float sin_r;

    float x_dir_sx() const { return cos_r; }
    float x_dir_sy() const { return -sin_r; }

    float y_dir_sx() const { return -sin_r; }
    float y_dir_sy() const { return -cos_r; }

    static RotationCache from_degrees(float degrees) {
        constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
        float rad = degrees * DEG_TO_RAD;
        return { std::cos(rad), std::sin(rad) };
    }

    static RotationCache from_radians(float radians) {
        return { std::cos(radians), std::sin(radians) };
    }
};

}
