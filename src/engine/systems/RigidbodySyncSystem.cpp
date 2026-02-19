#include "RigidbodySyncSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Transform.h"
#include "engine/core/Logger.h"
#include "engine/core/MathConstants.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/core/EngineContext.h"
#include "editor/core/EditorComponents.h"
#include <box2d/box2d.h>

namespace engine {

bool RigidbodySyncSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_physics_world = ctx.physics_world;

    if (!m_physics_world) {
        Logger::instance().warning("RigidbodySync", "No physics world - RigidbodySyncSystem disabled");
    } else {
        Logger::instance().info("RigidbodySync", "RigidbodySyncSystem initialized");
    }
    return true;
}

void RigidbodySyncSystem::fixed_update(Engine& /*engine*/, float /*dt*/) {
    if (!m_physics_world) return;

    // Sync Transform FROM Box2D bodies for Dynamic bodies (Physics → Transform)
    // Note: Kinematic bodies are already synced Transform → Physics in Box2DPhysicsSystem

    auto view = m_registry->view<physics::Rigidbody, Transform>();

    for (auto entity : view) {
        // Skip disabled entities
        if (m_registry->all_of<editor::EntityInfo>(entity)) {
            if (!m_registry->get<editor::EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        auto& rb = view.get<physics::Rigidbody>(entity);
        auto& transform = view.get<Transform>(entity);

        // Skip if rigidbody disabled or body not created
        if (!rb.enabled || !b2Body_IsValid(rb.body_id)) {
            continue;
        }

        // Only sync Dynamic bodies (physics-controlled)
        if (rb.body_type != physics::BodyType::Dynamic) {
            continue;
        }

        // Read position from Box2D (already in pixels) and angle (radians)
        b2Vec2 pos = m_physics_world->get_body_position(rb.body_id);
        float angle_rad = m_physics_world->get_body_angle(rb.body_id);
        float angle_deg = angle_rad * RAD_TO_DEG;

        // Apply position locks: keep the transform value, reset the body back
        if (!rb.lock_position_x) {
            transform.x = pos.x;
        }
        if (!rb.lock_position_y) {
            transform.y = pos.y;
        }
        if (!rb.lock_rotation) {
            transform.rotation = angle_deg;
        }

        // Enforce position/rotation locks by resetting the body to the locked transform
        // and zeroing velocity on locked axes. This prevents drift that would occur
        // if we only zeroed velocity (body already moved during the step).
        if (rb.lock_position_x || rb.lock_position_y || rb.lock_rotation) {
            float corrected_x = rb.lock_position_x ? transform.x : pos.x;
            float corrected_y = rb.lock_position_y ? transform.y : pos.y;
            float corrected_angle = rb.lock_rotation ? transform.rotation * DEG_TO_RAD : angle_rad;
            m_physics_world->set_body_transform(rb.body_id, corrected_x, corrected_y, corrected_angle);

            if (rb.lock_position_x || rb.lock_position_y) {
                b2Vec2 vel = m_physics_world->get_body_linear_velocity(rb.body_id);
                if (rb.lock_position_x) vel.x = 0.0f;
                if (rb.lock_position_y) vel.y = 0.0f;
                m_physics_world->set_body_linear_velocity(rb.body_id, vel.x, vel.y);
            }
            if (rb.lock_rotation) {
                m_physics_world->set_body_angular_velocity(rb.body_id, 0.0f);
            }
        }
    }
}

} // namespace engine
