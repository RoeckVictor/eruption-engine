#pragma once

#include "engine/prefab/ComponentRegistry.h"

namespace game {

/// Set up component factories for all game components.
/// Call this once at startup before loading prefabs.
void register_game_components(engine::prefab::ComponentRegistry& registry);

} // namespace game
