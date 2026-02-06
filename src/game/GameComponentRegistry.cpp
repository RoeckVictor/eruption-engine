#include "game/GameComponentRegistry.h"
#include "game/components/Components.h"
#include <nlohmann/json.hpp>

namespace game {

void register_game_components(engine::prefab::ComponentRegistry& registry) {
    // Transform: position
    registry.register_component<Transform>("Transform", [](const nlohmann::json& j) {
        Transform t;
        if (j.contains("x")) t.x = j["x"].get<float>();
        if (j.contains("y")) t.y = j["y"].get<float>();
        return t;
    });

    // PlayerController: movement parameters
    registry.register_component<PlayerController>("PlayerController", [](const nlohmann::json& j) {
        PlayerController pc;
        if (j.contains("move_accel")) pc.move_accel = j["move_accel"].get<float>();
        if (j.contains("max_move_speed")) pc.max_move_speed = j["max_move_speed"].get<float>();
        if (j.contains("jump_velocity")) pc.jump_velocity = j["jump_velocity"].get<float>();
        if (j.contains("friction")) pc.friction = j["friction"].get<float>();
        // move_dir and jump_pressed are runtime state, not serialized
        return pc;
    });

    // CameraTarget: camera follow parameters
    registry.register_component<CameraTarget>("CameraTarget", [](const nlohmann::json& j) {
        CameraTarget ct;
        if (j.contains("offset_y_fraction")) ct.offset_y_fraction = j["offset_y_fraction"].get<float>();
        return ct;
    });

    // PlayerTag: marker component (no data)
    registry.register_tag<PlayerTag>("PlayerTag");
}

} // namespace game
