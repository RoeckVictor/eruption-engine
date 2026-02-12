#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>
#include <box2d/id.h>

namespace engine { struct Transform; }

namespace engine::physics {
    class PhysicsWorld;
    struct Rigidbody;
    struct BoxCollider;
    struct CircleCollider;
    struct CapsuleCollider;
    struct DynamicCollider;
}

namespace engine::simulation { struct PixelGridComponent; }

namespace engine {

/// Creates and manages Box2D bodies for entities with Rigidbody + Collider components.
///
/// This system provides standard rigid body physics using Box2D's simple collider shapes
/// (box, circle, capsule) and auto-generated colliders from pixel grids (DynamicCollider).
///
/// Execution Order: Runs BEFORE SimulationPipelineSystem in fixed_update phase.
/// - Creates bodies for new Rigidbody entities
/// - Syncs Transform → Physics for Kinematic bodies (before physics step)
/// - Manages collider shapes and handles triangulation for DynamicCollider
class Box2DPhysicsSystem : public System {
public:
    bool init(Engine& engine) override;
    void fixed_update(Engine& engine, float dt) override;
    void shutdown() override;

private:
    entt::registry* m_registry = nullptr;
    physics::PhysicsWorld* m_physics_world = nullptr;

    // --- Body Management ---

    /// Create a Box2D body for an entity with a Rigidbody component
    void create_body_for_entity(entt::entity entity, physics::Rigidbody& rb, const Transform& transform);

    /// Update kinematic body transforms before physics step (Transform → Physics)
    void sync_kinematic_bodies();

    /// Destroy Box2D body when Rigidbody component is removed
    void on_rigidbody_destroyed(entt::registry& registry, entt::entity entity);

    // --- Collider Creation ---

    void create_box_collider(b2BodyId body, physics::BoxCollider& collider, const Transform& transform);
    void create_circle_collider(b2BodyId body, physics::CircleCollider& collider, const Transform& transform);
    void create_capsule_collider(b2BodyId body, physics::CapsuleCollider& collider, const Transform& transform);
    void create_dynamic_collider(entt::entity entity, b2BodyId body, physics::DynamicCollider& collider);

    // --- DynamicCollider Triangulation ---

    /// Generate collision mesh from pixel grid boundaries
    void triangulate_pixel_grid(entt::entity entity, physics::DynamicCollider& collider);
};

} // namespace engine
