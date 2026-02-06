#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::physics { class PhysicsWorld; class PixelBodyManager; }

namespace game {

/// Synchronizes Transform components FROM rigid body positions after physics step.
/// This system runs after SimulationPipelineSystem to ensure Transform reflects
/// the latest physics state for rendering and camera tracking.
class RigidBodySyncSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void fixed_update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
    engine::physics::PhysicsWorld* m_physics_world = nullptr;
};

} // namespace game
