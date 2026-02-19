#pragma once

#include <entt/fwd.hpp>

namespace engine {

namespace render { struct Camera2D; }
namespace physics { class PhysicsWorld; }

/// Minimal context for engine systems.
///
/// Engine systems (Box2DPhysicsSystem, RigidbodySyncSystem, etc.) use this
/// to access the ECS registry, physics world, and camera. Whoever creates
/// the runtime (editor RuntimeContext, standalone game, etc.) must populate
/// an EngineContext and set it via Engine::set_app_context().
struct EngineContext {
    entt::registry& registry;
    physics::PhysicsWorld* physics_world;  // Can be null if physics not needed
    render::Camera2D& camera;
};

} // namespace engine
