#pragma once

#include <string>

namespace engine::simulation {

// Component that references pixel grid data from a .pxg file
// This component stores the path to the pixel grid asset
struct PixelGridComponent {
    bool enabled = true;

    std::string pixel_grid_path;

    int width = 0;
    int height = 0;

    int origin_x = 0;
    int origin_y = 0;

    bool loaded = false;

    bool destructible = true;
};

}
