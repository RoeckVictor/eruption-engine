#pragma once

#include "engine/simulation/PixelData.h"
#include <cstdint>

namespace game {

/// Game-specific pixel data structure extending the engine's base.
///
/// The engine's PixelData defines the first 4 bytes that all pixels must have:
///   - material (uint8_t): Game-defined material ID
///   - category (uint8_t): Engine physics category (CAT_EMPTY, CAT_STATIC, etc.)
///   - temperature (uint8_t): Thermal value (0-255)
///   - flags (uint8_t): Engine flags (reserved)
///
/// Games can extend this struct to add game-specific fields. The engine will
/// only read/write the base fields, while game shaders can access all fields.
///
/// GLSL equivalent (must match exactly):
/// ```glsl
/// struct GamePixel {
///     uint material;    // byte 0: game-defined material ID
///     uint category;    // byte 1: engine physics category
///     uint temperature; // byte 2: thermal value (0-255)
///     uint flags;       // byte 3: engine flags
///     // Game-specific fields below (add as needed):
///     // uint pressure;
///     // float velocity_x;
///     // float velocity_y;
///     // etc.
/// };
/// ```
struct GamePixel {
    // ---- Engine-mandated fields (must be first, must match PixelData layout) ----
    uint8_t material;    // Game-defined material ID (for color lookup, game logic)
    uint8_t category;    // Engine physics category (CAT_EMPTY, CAT_STATIC, etc.)
    uint8_t temperature; // Thermal value (0-255)
    uint8_t flags;       // Engine flags (reserved for future use)

    // ---- Game-specific fields (add more as needed) ----
    // For now, we keep the same 4-byte layout as the texture-based system.
    // Future extensions might include:
    // uint8_t pressure;     // Fluid pressure (for water flow simulation)
    // uint8_t humidity;     // Environmental humidity
    // uint8_t age;          // Pixel age (for decay, etc.)
    // uint8_t strength;     // Structural integrity (for destructible terrain)
    // int16_t velocity_x;   // Per-pixel velocity X (fixed point)
    // int16_t velocity_y;   // Per-pixel velocity Y (fixed point)

    // Default constructor: empty pixel
    GamePixel()
        : material(0)
        , category(engine::simulation::CAT_EMPTY)
        , temperature(128)
        , flags(0) {}

    // Construct from base PixelData
    explicit GamePixel(const engine::simulation::PixelData& base)
        : material(base.material)
        , category(base.category)
        , temperature(base.temperature)
        , flags(base.flags) {}

    // Construct with all base fields
    GamePixel(uint8_t mat, uint8_t cat, uint8_t temp, uint8_t flg = 0)
        : material(mat), category(cat), temperature(temp), flags(flg) {}

    // Convert to engine PixelData (for engine APIs that need it)
    engine::simulation::PixelData to_base() const {
        return engine::simulation::PixelData(material, category, temperature, flags);
    }

    // Convenience accessors (delegate to category checks)
    bool is_empty() const { return category == engine::simulation::CAT_EMPTY; }
    bool is_solid() const {
        return category == engine::simulation::CAT_STATIC ||
               category == engine::simulation::CAT_POWDER;
    }
    bool is_mobile() const {
        return category == engine::simulation::CAT_POWDER ||
               category == engine::simulation::CAT_LIQUID ||
               category == engine::simulation::CAT_GAS;
    }
};

// Ensure the struct matches expected size
// Currently 4 bytes (same as texture-based system)
// Increase this when adding game-specific fields
static_assert(sizeof(GamePixel) == 4, "GamePixel size changed - update shaders!");

// Size of the pixel struct in bytes (used by PixelGrid for SSBO allocation)
constexpr size_t GAME_PIXEL_SIZE = sizeof(GamePixel);

} // namespace game
