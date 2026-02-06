#include "game/systems/PlayerInputSystem.h"
#include "engine/core/Engine.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include <entt/entt.hpp>

namespace game {

using engine::platform::KeyCode;

bool PlayerInputSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    return true;
}

void PlayerInputSystem::update(engine::Engine& engine, float /*dt*/) {
    const auto& input = engine.input();
    auto view = m_registry->view<PlayerController>();
    for (auto entity : view) {
        auto& ctrl = view.get<PlayerController>(entity);

        ctrl.move_dir = 0;
        if (input.is_held(KeyCode::A) || input.is_held(KeyCode::Left))  ctrl.move_dir -= 1;
        if (input.is_held(KeyCode::D) || input.is_held(KeyCode::Right)) ctrl.move_dir += 1;

        // jump_pressed is a latch: set here, consumed (cleared) by PlayerSystem
        // in fixed_update. We only set it to true; never reset it to false here,
        // so a press is never lost if there are 0 fixed steps that frame.
        if (input.is_pressed(KeyCode::W)
            || input.is_pressed(KeyCode::Up)
            || input.is_pressed(KeyCode::Space)) {
            ctrl.jump_pressed = true;
        }
    }
}

} // namespace game
