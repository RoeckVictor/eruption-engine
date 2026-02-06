#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::physics { class PhysicsWorld; }

namespace game {

class PlayerSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void fixed_update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
    engine::physics::PhysicsWorld* m_physics_world = nullptr;
};

} // namespace game
