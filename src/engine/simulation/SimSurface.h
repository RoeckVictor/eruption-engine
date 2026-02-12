#pragma once

#include <string>

namespace engine::simulation {

/// Pixel simulation surface component.
///
/// Marks an entity as a falling-sand simulation surface.
/// The entity must also have a PixelGridComponent, which provides
/// the grid dimensions and initial pixel data for the simulation.
///
/// Multiple SimSurface components can exist in a scene, each on its own entity,
/// allowing multiple independent simulation grids.
struct SimSurface {
    // Simulation settings
    bool simulation_enabled = true;
    float simulation_speed = 1.0f;   // Time scale multiplier

    // Material set (which material library to use for physics categories)
    std::string material_set = "default";

    // Chunk dimensions for terrain collider generation (pixels)
    int chunk_size_x = 32;
    int chunk_size_y = 32;

    // Generate Box2D chain colliders from settled pixels (static + settled powder).
    // Enables physical interaction between rigidbodies and the pixel terrain.
    bool generate_colliders = false;

    // Runtime state (set by initialization logic)
    bool initialized = false;
};

} // namespace engine::simulation
