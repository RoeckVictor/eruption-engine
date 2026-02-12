#pragma once

#include <string>

namespace engine::simulation {

/// Component that references pixel grid data from a .pxg file.
/// This component stores the path to the pixel grid asset.
struct PixelGridComponent {
    bool enabled = true;

    /// Path to the .pxg file (e.g., "assets/grids/terrain.pxg")
    std::string pixel_grid_path;

    /// Width of the grid (loaded from file, cached here)
    int width = 0;

    /// Height of the grid (loaded from file, cached here)
    int height = 0;

    /// Origin/pivot point in pixel coordinates (loaded from .pxg metadata)
    int origin_x = 0;
    int origin_y = 0;

    /// Whether the grid data has been loaded
    bool loaded = false;

    // Runtime pointer to actual PixelGrid (managed by system)
    // void* grid_data = nullptr;  // Opaque pointer to avoid circular dependency
};

} // namespace engine::simulation
