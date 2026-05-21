#pragma once

#include "engine/save/SaveData.h"
#include <entt/entt.hpp>

namespace engine::physics {
class PhysicsWorld;
}

namespace engine::save {

// Captures and restores full game state including pixel grids and physics
// Uses SceneSerializer for entity/component data, and adds:
// - PixelGrid GPU readback + LZ4 compression
// - Physics body velocity/position capture
class GameStateSerializer {
public:
    static SaveData capture(entt::registry& registry,
                            physics::PhysicsWorld* physics,
                            const std::string& scene_name,
                            float play_time);

    static bool restore(const SaveData& data,
                        entt::registry& registry,
                        physics::PhysicsWorld* physics);
};

}
