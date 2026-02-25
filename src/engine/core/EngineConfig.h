#pragma once

#include "engine/core/Result.h"
#include <string>

namespace engine {

// Centralized engine configuration
// All hard-coded constants should be moved here to allow runtime configuration
// Can be loaded from JSON via load_from_json() or used with default values
struct EngineConfig {
    int max_fixed_steps = 8;
    float pixels_per_meter = 32.0f;

    // Default gravity in meters/sec^2 (Box2D units)
    float default_gravity_x = 0.0f;
    float default_gravity_y = 10.0f;

    int terrain_chunk_size = 64;

    // Minimum pixel count for a body to remain as a rigid body
    int min_body_pixels = 4;
    int physics_substeps = 4;

    double max_delta_time = 0.25;
    double fixed_timestep = 1.0 / 60.0;

    int max_material_slots = 256;
    int sim_workgroup_size = 16;

    int max_particles = 16384;
    int max_particle_extractions = 4096;
    float particle_lifetime = 5.0f;

    /// Velocity scatter for rigidbody collision extractions (pixels/sec)
    float particle_scatter_min = -30.0f;
    float particle_scatter_max = 30.0f;

    /// Velocity scatter for direct collider displacement (pixels/sec)
    float displacement_scatter_min = -50.0f;
    float displacement_scatter_max = 50.0f;

    float clear_color_r = 0.1f;
    float clear_color_g = 0.1f;
    float clear_color_b = 0.1f;

    std::string asset_base_path = ".";
    double hot_reload_poll_interval = 1.0;

    int max_scene_stack_depth = 16;

    static EngineConfig defaults() {
        return EngineConfig{};
    }

    static Result<EngineConfig, ErrorInfo> load_from_json(const std::string& file_path);
};

}
