#include "game/systems/PlayerSystem.h"
#include "engine/core/Engine.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBody.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include "game/components/RigidBodyComponent.h"
#include "game/GameLog.h"
#include <entt/entt.hpp>
#include <cmath>

namespace game {

bool PlayerSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_physics_world = &ctx.physics_world;
    return true;
}

void PlayerSystem::fixed_update(engine::Engine& /*engine*/, float dt) {
    auto view = m_registry->view<RigidBodyComponent, PlayerController>();

    for (auto entity : view) {
        auto& rb = view.get<RigidBodyComponent>(entity);
        auto& ctrl = view.get<PlayerController>(entity);

        if (!rb.body) continue;
        b2BodyId body_id = rb.body->body_id();

        // Check if player is grounded (for jumping)
        bool grounded = m_physics_world->is_grounded(body_id, 2.0f);

        // Get current velocity from rigid body
        b2Vec2 vel = m_physics_world->get_body_linear_velocity(body_id);

        // Horizontal movement (direct velocity manipulation for responsive controls)
        if (ctrl.move_dir != 0) {
            float accel = ctrl.move_accel * dt;
            vel.x += (float)ctrl.move_dir * accel;
            vel.x = std::clamp(vel.x, -ctrl.max_move_speed, ctrl.max_move_speed);
        } else {
            // Friction
            vel.x -= vel.x * ctrl.friction * dt;
            if (std::abs(vel.x) < 0.5f) vel.x = 0.0f;
        }

        // Handle jump (consume jump_pressed flag)
        if (ctrl.jump_pressed) {
            if (grounded) {
                // Jump is negative velocity (up in pixel coordinates)
                // Convert to impulse: impulse = mass * velocity
                float mass = m_physics_world->get_body_mass(body_id);
                float jump_impulse = mass * ctrl.jump_velocity;

                m_physics_world->apply_impulse(body_id, 0.0f, jump_impulse);

                // Get updated velocity after impulse
                vel = m_physics_world->get_body_linear_velocity(body_id);
            }
            ctrl.jump_pressed = false;  // Consume jump
        }

        // Write velocity back to rigid body
        m_physics_world->set_body_linear_velocity(body_id, vel.x, vel.y);
    }
}

} // namespace game
