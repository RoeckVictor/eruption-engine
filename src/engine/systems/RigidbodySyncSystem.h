#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::physics { class PhysicsWorld; }

namespace engine {

/// Synchronizes Transform components bidirectionally with Box2D bodies.
///
/// This system handles transform sync for component-based Rigidbody physics:
/// - Dynamic bodies: Physics → Transform (physics controls position)
/// - Kinematic bodies: Already synced in Box2DPhysicsSystem before step
/// - Static bodies: No sync needed (immovable)
///
/// Execution Order: Runs AFTER SimulationPipelineSystem in fixed_update phase,
/// alongside the existing RigidBodySyncSystem (which handles PixelBody).
class RigidbodySyncSystem : public System {
public:
    bool init(Engine& engine) override;
    void fixed_update(Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
    physics::PhysicsWorld* m_physics_world = nullptr;
};

} // namespace engine
