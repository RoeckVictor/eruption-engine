#include "engine/core/EngineConfig.h"
#include "engine/core/Error.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace engine {

Result<EngineConfig, ErrorInfo> EngineConfig::load_from_json(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Err<EngineConfig>(
            EngineError::FileReadError,
            "Failed to open config file",
            file_path
        );
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(content);
    } catch (const nlohmann::json::exception& e) {
        return Err<EngineConfig>(
            EngineError::JsonParseFailed,
            std::string("JSON parse error: ") + e.what(),
            file_path
        );
    }

    // Start with defaults and override with JSON values
    EngineConfig config = defaults();

    // Physics section
    if (json.contains("physics")) {
        const auto& physics = json["physics"];
        if (physics.contains("max_fixed_steps")) config.max_fixed_steps = physics["max_fixed_steps"].get<int>();
        if (physics.contains("pixels_per_meter")) config.pixels_per_meter = physics["pixels_per_meter"].get<float>();
        if (physics.contains("default_gravity_x")) config.default_gravity_x = physics["default_gravity_x"].get<float>();
        if (physics.contains("default_gravity_y")) config.default_gravity_y = physics["default_gravity_y"].get<float>();
        if (physics.contains("terrain_chunk_size")) config.terrain_chunk_size = physics["terrain_chunk_size"].get<int>();
        if (physics.contains("min_body_pixels")) config.min_body_pixels = physics["min_body_pixels"].get<int>();
        if (physics.contains("substeps")) config.physics_substeps = physics["substeps"].get<int>();
    }

    // Timing section
    if (json.contains("timing")) {
        const auto& timing = json["timing"];
        if (timing.contains("max_delta_time")) config.max_delta_time = timing["max_delta_time"].get<double>();
        if (timing.contains("fixed_timestep")) config.fixed_timestep = timing["fixed_timestep"].get<double>();
    }

    // Simulation section
    if (json.contains("simulation")) {
        const auto& simulation = json["simulation"];
        if (simulation.contains("max_material_slots")) config.max_material_slots = simulation["max_material_slots"].get<int>();
        if (simulation.contains("workgroup_size")) config.sim_workgroup_size = simulation["workgroup_size"].get<int>();
    }

    // Graphics section
    if (json.contains("graphics")) {
        const auto& graphics = json["graphics"];
        if (graphics.contains("clear_color_r")) config.clear_color_r = graphics["clear_color_r"].get<float>();
        if (graphics.contains("clear_color_g")) config.clear_color_g = graphics["clear_color_g"].get<float>();
        if (graphics.contains("clear_color_b")) config.clear_color_b = graphics["clear_color_b"].get<float>();
    }

    // Assets section
    if (json.contains("assets")) {
        const auto& assets = json["assets"];
        if (assets.contains("base_path")) config.asset_base_path = assets["base_path"].get<std::string>();
        if (assets.contains("hot_reload_poll_interval")) config.hot_reload_poll_interval = assets["hot_reload_poll_interval"].get<double>();
    }

    // Validate critical values
    if (config.fixed_timestep <= 0.0) {
        ENGINE_ERR("Invalid fixed_timestep (%f) in config, using default", config.fixed_timestep);
        config.fixed_timestep = defaults().fixed_timestep;
    }
    if (config.max_fixed_steps <= 0) {
        ENGINE_ERR("Invalid max_fixed_steps (%d) in config, using default", config.max_fixed_steps);
        config.max_fixed_steps = defaults().max_fixed_steps;
    }
    if (config.pixels_per_meter <= 0.0f) {
        ENGINE_ERR("Invalid pixels_per_meter (%f) in config, using default", config.pixels_per_meter);
        config.pixels_per_meter = defaults().pixels_per_meter;
    }
    if (config.physics_substeps <= 0) {
        ENGINE_ERR("Invalid physics_substeps (%d) in config, using default", config.physics_substeps);
        config.physics_substeps = defaults().physics_substeps;
    }

    ENGINE_LOG("Loaded engine config from '%s'", file_path.c_str());
    return Ok(config);
}

} // namespace engine
