#pragma once

#include <entt/entt.hpp>

namespace engine {
struct Transform;

namespace physics {
class PhysicsWorld;
struct Rigidbody;
}
}

namespace editor {

class PhysicsPlayback {
public:
    PhysicsPlayback(entt::registry& registry, engine::physics::PhysicsWorld& world);

    void init_bodies();
    void create_body_for_entity(entt::entity entity);
    void sync_to_transforms();

private:
    void attach_collider_shapes(entt::entity entity, engine::physics::Rigidbody& rb);

    entt::registry& m_registry;
    engine::physics::PhysicsWorld& m_world;
};

}
