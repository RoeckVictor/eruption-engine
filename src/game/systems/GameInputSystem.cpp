#include "game/systems/GameInputSystem.h"
#include "engine/core/Engine.h"
#include "game/GameContext.h"
#include "game/GameEvents.h"
#include "game/components/Components.h"
#include "game/GameLog.h"
#include <entt/entt.hpp>

namespace game {

using engine::platform::KeyCode;

bool GameInputSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    return true;
}

void GameInputSystem::update(engine::Engine& engine, float /*dt*/) {
    const auto& input = engine.input();
    auto& state = m_registry->ctx().get<GameInputState>();

    // One-shot actions published as events
    if (input.is_pressed(KeyCode::Escape)) {
        engine.events().publish(QuitRequestedEvent{});
    }
    if (input.is_pressed(KeyCode::R)) {
        engine.events().publish(RespawnRequestedEvent{});
    }

    // Material selection (also resets to Material tool mode)
    if (input.is_pressed(KeyCode::Num1)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_ROCK;  GAME_LOG("Selected: Rock"); }
    if (input.is_pressed(KeyCode::Num2)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_DIRT;  GAME_LOG("Selected: Dirt"); }
    if (input.is_pressed(KeyCode::Num3)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_SAND;  GAME_LOG("Selected: Sand"); }
    if (input.is_pressed(KeyCode::Num4)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_WATER; GAME_LOG("Selected: Water"); }
    if (input.is_pressed(KeyCode::Num5)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_LAVA;  GAME_LOG("Selected: Lava"); }
    if (input.is_pressed(KeyCode::Num6)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_ICE;   GAME_LOG("Selected: Ice"); }
    if (input.is_pressed(KeyCode::Num7)) { state.tool_mode = ToolMode::Material; state.selected_material = MAT_STEAM; GAME_LOG("Selected: Steam"); }

    // Sprite tool modes
    if (input.is_pressed(KeyCode::Num8)) { state.tool_mode = ToolMode::PasteSprite; GAME_LOG("Tool: Paste Sprite"); }
    if (input.is_pressed(KeyCode::Num9)) { state.tool_mode = ToolMode::SpawnBody;   GAME_LOG("Tool: Spawn Body"); }

    // Brush size
    if (input.is_pressed(KeyCode::RightBracket)) { state.brush_radius++; GAME_LOG("Brush: %d", state.brush_radius); }
    if (input.is_pressed(KeyCode::LeftBracket) && state.brush_radius > 1) { state.brush_radius--; GAME_LOG("Brush: %d", state.brush_radius); }

    // Pause toggle
    if (input.is_pressed(KeyCode::P)) {
        state.sim_paused = !state.sim_paused;
        GAME_LOG("Simulation %s", state.sim_paused ? "PAUSED" : "RUNNING");
    }

    // Debug draw toggle
    if (input.is_pressed(KeyCode::F3)) {
        state.debug_draw = !state.debug_draw;
        GAME_LOG("Debug draw %s", state.debug_draw ? "ON" : "OFF");
    }
}

} // namespace game
