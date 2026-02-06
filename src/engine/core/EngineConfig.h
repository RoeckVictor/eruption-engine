#pragma once

#include "engine/core/Result.h"
#include <string>

namespace engine {

/// Centralized engine configuration.
/// All hard-coded constants should be moved here to allow runtime configuration.
///
/// Can be loaded from JSON via load_from_json() or used with default values.
struct EngineConfig {
    // --- Physics ---

    /// Maximum number of fixed timestep iterations per frame.
    /// Prevents spiral-of-death when simulation can't keep up.
    int max_fixed_steps = 8;

    /// Pixels-per-meter scale factor for Box2D coordinate conversion.
    float pixels_per_meter = 32.0f;

    /// Default gravity in meters/sec^2 (Box2D units).
    float default_gravity_x = 0.0f;
    float default_gravity_y = 10.0f;

    /// Terrain collision chunk size in pixels.
    /// Larger chunks = fewer colliders but less granular updates.
    int terrain_chunk_size = 64;

    /// Minimum pixel count for a body to remain as a rigid body.
    /// Bodies smaller than this threshold are destroyed (can be converted to particles).
    int min_body_pixels = 4;

    /// Default Box2D substep count per physics step.
    int physics_substeps = 4;

    // --- Timing ---

    /// Maximum frame delta time in seconds.
    /// Clamps time step to prevent spiral-of-death and physics instability.
    double max_delta_time = 0.25;

    /// Fixed timestep in seconds for physics updates.
    double fixed_timestep = 1.0 / 60.0;

    // --- Simulation ---

    /// Maximum material slots for cellular automata simulation.
    /// Must match compute shader SSBO layout (2 uint32s per material).
    int max_material_slots = 256;

    /// Compute shader work group size for simulation.
    /// Must match layout(local_size_x, local_size_y) in sim_step.comp.
    int sim_workgroup_size = 16;

    // --- Graphics ---

    /// Default clear color (RGB, 0.0-1.0 range).
    float clear_color_r = 0.1f;
    float clear_color_g = 0.1f;
    float clear_color_b = 0.1f;

    // --- Asset Loading ---

    /// Base path for asset loading (relative to executable).
    std::string asset_base_path = ".";

    /// Asset hot-reload poll interval in seconds (0 = disabled).
    double hot_reload_poll_interval = 1.0;

    // --- Scene Management ---

    /// Maximum scene stack depth to prevent infinite recursion.
    int max_scene_stack_depth = 16;

    // --- Factory ---

    /// Create config with default values.
    static EngineConfig defaults() {
        return EngineConfig{};
    }

    /// Load config from JSON file.
    /// Missing fields use default values.
    /// Returns error if file cannot be read or JSON is invalid.
    static Result<EngineConfig, ErrorInfo> load_from_json(const std::string& file_path);
};

} // namespace engine
