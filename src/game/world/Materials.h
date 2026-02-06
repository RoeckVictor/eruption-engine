#pragma once

#include "engine/simulation/MaterialDefs.h"
#include "game/world/MaterialProps.h"
#include <cstdint>

namespace game {

// Re-export engine category enum
using MaterialCategory = engine::simulation::MaterialCategory;

inline constexpr uint8_t CAT_EMPTY  = engine::simulation::CAT_EMPTY;
inline constexpr uint8_t CAT_STATIC = engine::simulation::CAT_STATIC;
inline constexpr uint8_t CAT_POWDER = engine::simulation::CAT_POWDER;
inline constexpr uint8_t CAT_LIQUID = engine::simulation::CAT_LIQUID;
inline constexpr uint8_t CAT_GAS    = engine::simulation::CAT_GAS;

// ---- Game-specific material IDs ----

enum Material : uint8_t {
    MAT_AIR       = 0,
    MAT_ROCK      = 1,
    MAT_DIRT      = 2,
    MAT_SAND      = 3,
    MAT_WATER     = 4,
    MAT_LAVA      = 5,
    MAT_ICE       = 6,
    MAT_STEAM     = 7,
    MAT_FIRE      = 8,
    MAT_EXPLOSIVE = 9,
    MAT_COUNT
};

} // namespace game
