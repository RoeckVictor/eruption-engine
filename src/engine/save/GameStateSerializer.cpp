#include "engine/save/GameStateSerializer.h"
#include "engine/core/Log.h"

namespace engine::save {

SaveData GameStateSerializer::capture(entt::registry& /*registry*/,
                                       physics::PhysicsWorld* /*physics*/,
                                       const std::string& scene_name,
                                       float play_time) {
    SaveData data;
    data.version = 1;
    data.scene_name = scene_name;
    data.play_time = play_time;

    ENGINE_LOG("GameStateSerializer: Capture requested (scene: %s, time: %.1f)",
               scene_name.c_str(), play_time);
    return data;
}

bool GameStateSerializer::restore(const SaveData& /*data*/,
                                    entt::registry& /*registry*/,
                                    physics::PhysicsWorld* /*physics*/) {
    ENGINE_LOG_WARN("GameStateSerializer::restore() not yet fully implemented");
    return false;
}

}
