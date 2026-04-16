#include "engine/render/Camera2D.h"

namespace engine::render {

void screen_to_world(const Camera2D& camera,
                     float screen_x, float screen_y,
                     float screen_w, float screen_h,
                     float& out_world_x, float& out_world_y)
{
    float norm_x = screen_x / screen_w;
    float norm_y = screen_y / screen_h;

    float vis_w = screen_w / camera.zoom;
    float vis_h = screen_h / camera.zoom;

    out_world_x = camera.x - vis_w * 0.5f + norm_x * vis_w;
    // Use Y-UP convention: screen top (norm_y=0) → positive world_y,
    // screen bottom (norm_y=1) → negative world_y
    out_world_y = camera.y + vis_h * 0.5f - norm_y * vis_h;
}

} // namespace engine::render
