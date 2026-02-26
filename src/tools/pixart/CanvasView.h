#pragma once

#include "editor/core/Constants.h"

namespace pixart {

/// Encapsulates canvas view state: zoom, pan, and coordinate transformations.
/// Separates view logic from rendering and input handling.
class CanvasView {
public:
    // View parameters
    float zoom = editor::constants::DEFAULT_ZOOM;
    float pan_x = 0.0f;
    float pan_y = 0.0f;

    // Zoom constraints
    float min_zoom = editor::constants::MIN_ZOOM;
    float max_zoom = editor::constants::MAX_ZOOM;

    /// Reset view to default state.
    void reset() {
        zoom = editor::constants::DEFAULT_ZOOM;
        pan_x = 0.0f;
        pan_y = 0.0f;
    }

    /// Apply zoom centered on a screen point.
    /// @param factor Zoom multiplier (>1 = zoom in, <1 = zoom out)
    /// @param screen_x Screen X coordinate to zoom towards
    /// @param screen_y Screen Y coordinate to zoom towards
    /// @param canvas_center_x Canvas center X in screen coordinates
    /// @param canvas_center_y Canvas center Y in screen coordinates
    void zoom_towards(float factor, float screen_x, float screen_y,
                      float canvas_center_x, float canvas_center_y) {
        float old_zoom = zoom;
        zoom *= factor;
        if (zoom < min_zoom) zoom = min_zoom;
        if (zoom > max_zoom) zoom = max_zoom;

        // Adjust pan to keep the zoom point stationary
        float rel_x = screen_x - (canvas_center_x + pan_x);
        float rel_y = screen_y - (canvas_center_y + pan_y);
        pan_x -= rel_x * (zoom / old_zoom - 1.0f);
        pan_y -= rel_y * (zoom / old_zoom - 1.0f);
    }

    /// Convert screen coordinates to pixel coordinates.
    /// @param screen_x Screen X position
    /// @param screen_y Screen Y position
    /// @param canvas_x0 Canvas region top-left X
    /// @param canvas_y0 Canvas region top-left Y
    /// @param canvas_w Canvas region width
    /// @param canvas_h Canvas region height
    /// @param grid_w Grid width in pixels
    /// @param grid_h Grid height in pixels
    /// @param out_px Output pixel X coordinate
    /// @param out_py Output pixel Y coordinate
    /// @return True if the screen point is within the grid bounds
    bool screen_to_pixel(float screen_x, float screen_y,
                         float canvas_x0, float canvas_y0,
                         float canvas_w, float canvas_h,
                         int grid_w, int grid_h,
                         int& out_px, int& out_py) const {
        // Canvas center in screen coordinates
        float center_x = canvas_x0 + canvas_w * 0.5f + pan_x;
        float center_y = canvas_y0 + canvas_h * 0.5f + pan_y;

        // Grid bounds in screen coordinates
        float grid_screen_w = grid_w * zoom;
        float grid_screen_h = grid_h * zoom;
        float grid_x0 = center_x - grid_screen_w * 0.5f;
        float grid_y0 = center_y - grid_screen_h * 0.5f;

        // Convert to pixel coordinates
        out_px = static_cast<int>((screen_x - grid_x0) / zoom);
        out_py = static_cast<int>((screen_y - grid_y0) / zoom);

        return out_px >= 0 && out_px < grid_w && out_py >= 0 && out_py < grid_h;
    }

    /// Get the screen-space bounds of the grid.
    /// @param canvas_x0 Canvas region top-left X
    /// @param canvas_y0 Canvas region top-left Y
    /// @param canvas_w Canvas region width
    /// @param canvas_h Canvas region height
    /// @param grid_w Grid width in pixels
    /// @param grid_h Grid height in pixels
    /// @param out_x0 Output grid left edge in screen coords
    /// @param out_y0 Output grid top edge in screen coords
    /// @param out_w Output grid width in screen coords
    /// @param out_h Output grid height in screen coords
    void get_grid_screen_bounds(float canvas_x0, float canvas_y0,
                                float canvas_w, float canvas_h,
                                int grid_w, int grid_h,
                                float& out_x0, float& out_y0,
                                float& out_w, float& out_h) const {
        float center_x = canvas_x0 + canvas_w * 0.5f + pan_x;
        float center_y = canvas_y0 + canvas_h * 0.5f + pan_y;
        out_w = grid_w * zoom;
        out_h = grid_h * zoom;
        out_x0 = center_x - out_w * 0.5f;
        out_y0 = center_y - out_h * 0.5f;
    }
};

/// Helper for tracking pan drag state.
struct PanState {
    bool active = false;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float origin_pan_x = 0.0f;
    float origin_pan_y = 0.0f;

    void begin(float mouse_x, float mouse_y, float current_pan_x, float current_pan_y) {
        active = true;
        start_x = mouse_x;
        start_y = mouse_y;
        origin_pan_x = current_pan_x;
        origin_pan_y = current_pan_y;
    }

    void update(float mouse_x, float mouse_y, float& pan_x, float& pan_y) {
        if (active) {
            pan_x = origin_pan_x + (mouse_x - start_x);
            pan_y = origin_pan_y + (mouse_y - start_y);
        }
    }

    void end() {
        active = false;
    }
};

} // namespace pixart
