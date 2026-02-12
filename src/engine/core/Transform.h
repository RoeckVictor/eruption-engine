#pragma once

#include <entt/entt.hpp>

namespace engine {

/// Transform component for position, rotation, and scale.
/// Used by both editor and runtime for all entities.
struct Transform {
    bool enabled = true;  // Component enabled flag (affects systems that query Transform)

    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  // In degrees
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    // Computed world transform (for hierarchical transforms)
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_rotation = 0.0f;
    float world_scale_x = 1.0f;
    float world_scale_y = 1.0f;
};

} // namespace engine
