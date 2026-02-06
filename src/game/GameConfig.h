#pragma once

#include "engine/asset/VFS.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

namespace game {

/// Game-specific configuration loaded from game_config.json.
/// Stores values that were previously hardcoded in DemoScene.
struct GameConfig {
    // --- Scene ---
    int grid_width = 2048;
    int grid_height = 1024;
    int floor_thickness = 16;

    // --- Player ---
    int player_body_width = 6;
    int player_body_height = 12;
    int player_spawn_offset_y = 6;

    // --- Camera ---
    float camera_initial_zoom = 3.0f;
    float camera_smoothing = 8.0f;

    /// Create config with default values.
    static GameConfig defaults() {
        return GameConfig{};
    }

    /// Load config from JSON file via VFS.
    /// Missing fields use default values.
    static GameConfig load(engine::asset::VFS& vfs, const std::string& virtual_path) {
        GameConfig cfg;

        auto resolved = vfs.resolve(virtual_path);
        if (resolved.is_err()) {
            return cfg; // Return defaults if file not found
        }

        std::ifstream file(resolved.value());
        if (!file.is_open()) {
            return cfg;
        }

        try {
            nlohmann::json j;
            file >> j;

            if (j.contains("scene")) {
                auto& s = j["scene"];
                if (s.contains("grid_width")) cfg.grid_width = s["grid_width"].get<int>();
                if (s.contains("grid_height")) cfg.grid_height = s["grid_height"].get<int>();
                if (s.contains("floor_thickness")) cfg.floor_thickness = s["floor_thickness"].get<int>();
            }

            if (j.contains("player")) {
                auto& p = j["player"];
                if (p.contains("body_width")) cfg.player_body_width = p["body_width"].get<int>();
                if (p.contains("body_height")) cfg.player_body_height = p["body_height"].get<int>();
                if (p.contains("spawn_offset_y")) cfg.player_spawn_offset_y = p["spawn_offset_y"].get<int>();
            }

            if (j.contains("camera")) {
                auto& c = j["camera"];
                if (c.contains("initial_zoom")) cfg.camera_initial_zoom = c["initial_zoom"].get<float>();
                if (c.contains("smoothing")) cfg.camera_smoothing = c["smoothing"].get<float>();
            }
        } catch (...) {
            // JSON parse error, return defaults
        }

        return cfg;
    }
};

} // namespace game
