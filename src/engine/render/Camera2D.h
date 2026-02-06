#pragma once

namespace engine::render {

/// Pure data describing a 2D camera's position and zoom.
/// Gameplay logic (follow, zoom control) belongs in game-side systems.
struct Camera2D {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
    float min_zoom = 0.25f;
    float max_zoom = 8.0f;
    float smoothing = 0.0f; // 0 = instant snap, higher = smoother (e.g. 8.0f)

    float visible_width(float screen_w) const { return screen_w / zoom; }
    float visible_height(float screen_h) const { return screen_h / zoom; }
};

/// Convert screen-space coordinates to world-space given a Camera2D.
void screen_to_world(const Camera2D& camera,
                     float screen_x, float screen_y,
                     float screen_w, float screen_h,
                     float& out_world_x, float& out_world_y);

} // namespace engine::render
