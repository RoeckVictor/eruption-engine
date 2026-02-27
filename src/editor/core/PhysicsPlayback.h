#pragma once

#include "engine/physics/PixelGridTriangulation.h"
#include <entt/entt.hpp>
#include <unordered_map>

namespace engine {
struct Transform;

namespace physics {
class PhysicsWorld;
struct Rigidbody;
struct DynamicCollider;
}

namespace simulation {
struct PixelGridComponent;
}
}

namespace editor {

class PhysicsPlayback {
public:
    PhysicsPlayback(entt::registry& registry, engine::physics::PhysicsWorld& world,
                    engine::simulation::IPixelGridLoader* pixel_grid_loader = nullptr);

    void init_bodies();
    void create_body_for_entity(entt::entity entity);
    void sync_to_transforms();

    void update_dynamic_colliders();

private:
    void attach_collider_shapes(entt::entity entity, engine::physics::Rigidbody& rb);
    void attach_dynamic_collider(entt::entity entity, engine::physics::Rigidbody& rb);
    void triangulate_pixel_grid(entt::entity entity, engine::physics::DynamicCollider& collider);
    void destroy_dynamic_collider_shapes(engine::physics::DynamicCollider& collider);

    entt::registry& m_registry;
    engine::physics::PhysicsWorld& m_world;
    engine::simulation::IPixelGridLoader* m_pixel_grid_loader = nullptr;

    std::unordered_map<entt::entity, engine::physics::PixelGridMesh> m_collider_meshes;
};

}
