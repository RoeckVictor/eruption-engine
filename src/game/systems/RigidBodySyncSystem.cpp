#include "game/systems/RigidBodySyncSystem.h"
#include "engine/core/Engine.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBody.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include "game/components/RigidBodyComponent.h"

namespace game {

bool RigidBodySyncSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_physics_world = &ctx.physics_world;
    return true;
}

void RigidBodySyncSystem::fixed_update(engine::Engine& /*engine*/, float /*dt*/) {
    // Sync Transform FROM rigid body positions (one-way: body -> component)
    auto view = m_registry->view<Transform, RigidBodyComponent>();

    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& rb = view.get<RigidBodyComponent>(entity);

        if (!rb.body) continue;

        // Read position from PixelBody and write to Transform
        transform.x = rb.body->world_x(*m_physics_world);
        transform.y = rb.body->world_y(*m_physics_world);
        // Rotation is locked for player, so we don't need to sync angle
    }
}

} // namespace game
