#include "game/systems/EntityRenderSystem.h"
#include "engine/core/Engine.h"
#include "engine/render/Camera2D.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include "game/GameLog.h"
#include <entt/entt.hpp>

namespace game {

bool EntityRenderSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;

    if (!m_sprite_renderer.init()) {
        GAME_ERR("Failed to initialize sprite renderer");
        return false;
    }
    GAME_LOG("Entity render system initialized");
    return true;
}

void EntityRenderSystem::shutdown() {
    m_sprite_renderer.shutdown();
}

void EntityRenderSystem::render(engine::Engine& /*engine*/) {
    // NOTE: Currently disabled - no entities use Renderable component
    // Previously rendered entities with Collider+Renderable components
    // Future: Could render PixelBody entities or other sprite-based entities

    // No-op for now - all entities are rendered via PixelBody stamp/clear or particles
}

} // namespace game
