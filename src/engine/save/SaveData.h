#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace engine::save {

// LZ4-compressed snapshot of a PixelGrid's GPU data
struct PixelGridSnapshot {
    std::string entity_guid;
    int width  = 0;
    int height = 0;
    std::vector<uint8_t> compressed;
};

// Captured physics state for a single rigidbody entity
struct PhysicsBodyState {
    std::string entity_guid;
    float velocity_x       = 0.0f;
    float velocity_y       = 0.0f;
    float angular_velocity = 0.0f;
    float position_x       = 0.0f;
    float position_y       = 0.0f;
    float angle            = 0.0f;
    bool  awake            = true;
};

// Complete game save data
struct SaveData {
    int version = 1;
    std::string scene_name;
    float play_time = 0.0f;

    nlohmann::json entity_data;
    std::vector<PixelGridSnapshot> pixel_grids;
    std::vector<PhysicsBodyState> physics_bodies;
    nlohmann::json custom_data;
};

}
