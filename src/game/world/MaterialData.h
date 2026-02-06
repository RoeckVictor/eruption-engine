#pragma once

#include "game/world/Materials.h"

namespace game {

// ---- Material property table (index by Material enum) ----

inline constexpr MaterialProps MATERIAL_TABLE[MAT_COUNT] = {
    // density, category,    melt, boil, melt_into, boil_into, temp,  flags
    {   0, CAT_EMPTY,    0,    0,   0,         0,         128, 0 },                // AIR
    { 255, CAT_STATIC,   250,  255, MAT_LAVA,  MAT_LAVA,  128, 0 },                // ROCK
    { 200, CAT_POWDER,   200,  255, MAT_LAVA,  MAT_LAVA,  128, 0 },                // DIRT
    { 180, CAT_POWDER,   220,  255, MAT_LAVA,  MAT_LAVA,  128, 0 },                // SAND
    { 100, CAT_LIQUID,   0,    200, MAT_ICE,   MAT_STEAM, 128, 0 },                // WATER
    { 220, CAT_LIQUID,   0,    255, MAT_ROCK,  MAT_FIRE,  255, MAT_FLAG_HAZARD },  // LAVA
    { 150, CAT_STATIC,   100,  200, MAT_WATER, MAT_STEAM, 20,  0 },                // ICE
    {  10, CAT_GAS,      0,    0,   MAT_WATER, MAT_AIR,   200, 0 },                // STEAM
    {   5, CAT_GAS,      0,    0,   MAT_AIR,   MAT_AIR,   250, MAT_FLAG_HAZARD },  // FIRE
    { 190, CAT_STATIC,   150,  255, MAT_FIRE,  MAT_FIRE,  128, 0 },                // EXPLOSIVE
};

// Compile-time validation: category is packed into 4 bits in the SSBO,
// so it must not exceed 15. All other fields are uint8_t in 8-bit slots.
inline constexpr bool validate_material_table() {
    for (int i = 0; i < MAT_COUNT; i++) {
        if (MATERIAL_TABLE[i].category > 15) return false;
        if (MATERIAL_TABLE[i].melt_into >= MAT_COUNT && MATERIAL_TABLE[i].melt_point > 0) return false;
        if (MATERIAL_TABLE[i].boil_into >= MAT_COUNT && MATERIAL_TABLE[i].boil_point > 0) return false;
    }
    return true;
}
static_assert(validate_material_table(), "MaterialProps field exceeds bit-packing limits");

// ---- Color palette (RGBA) for rendering ----

inline constexpr uint32_t MATERIAL_COLORS[MAT_COUNT] = {
    0x1A1A2EFF, // AIR       - dark blue-black
    0x6B6B6BFF, // ROCK      - grey
    0x8B6914FF, // DIRT      - brown
    0xD4B96AFF, // SAND      - tan
    0x3A7BD5FF, // WATER     - blue
    0xFF4500FF, // LAVA      - orange-red
    0xB0E0E6FF, // ICE       - light blue
    0xD0D0D0AA, // STEAM     - white-ish semi-transparent
    0xFF6600FF, // FIRE      - bright orange
    0xCC0000FF, // EXPLOSIVE - dark red
};

// ---- Material classification helpers (derived from MATERIAL_TABLE) ----

inline bool mat_is_solid(uint8_t mat) {
    if (mat >= MAT_COUNT) return false;
    uint8_t cat = MATERIAL_TABLE[mat].category;
    return cat == CAT_STATIC || cat == CAT_POWDER;
}

inline bool mat_is_hard_solid(uint8_t mat) {
    return mat < MAT_COUNT && MATERIAL_TABLE[mat].category == CAT_STATIC;
}

inline bool mat_is_powder(uint8_t mat) {
    return mat < MAT_COUNT && MATERIAL_TABLE[mat].category == CAT_POWDER;
}

inline bool mat_is_liquid(uint8_t mat) {
    return mat < MAT_COUNT && MATERIAL_TABLE[mat].category == CAT_LIQUID;
}

inline bool mat_is_gas(uint8_t mat) {
    return mat < MAT_COUNT && MATERIAL_TABLE[mat].category == CAT_GAS;
}

inline bool mat_is_hazard(uint8_t mat) {
    return mat < MAT_COUNT && (MATERIAL_TABLE[mat].flags & MAT_FLAG_HAZARD) != 0;
}

} // namespace game
