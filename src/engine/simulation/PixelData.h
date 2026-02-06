#pragma once

#include "engine/simulation/MaterialDefs.h"
#include <cstdint>

namespace engine::simulation {

/// Per-pixel data structure for SSBO-based grid storage.
///
/// This struct defines the engine-mandated fields that every pixel must have.
/// Games can extend this by defining their own pixel struct that includes
/// these fields plus game-specific ones (pressure, velocity, etc.).
///
/// The struct is designed to be GPU-friendly:
/// - Tightly packed (no padding within the base fields)
/// - Power-of-2 aligned when extended
/// - Fields ordered by frequency of access
///
/// GLSL equivalent (must match exactly):
/// ```glsl
/// struct Pixel {
///     uint material;    // byte 0: game-defined material ID
///     uint category;    // byte 1: engine physics category
///     uint temperature; // byte 2: thermal value (0-255)
///     uint flags;       // byte 3: engine flags
/// };
/// ```
struct PixelData {
    uint8_t material;    // Game-defined material ID (for color lookup, game logic)
    uint8_t category;    // Engine physics category (CAT_EMPTY, CAT_STATIC, etc.)
    uint8_t temperature; // Thermal value (0-255)
    uint8_t flags;       // Engine flags (reserved for future use)

    // Default constructor: empty pixel
    PixelData() : material(0), category(CAT_EMPTY), temperature(128), flags(0) {}

    // Construct with all fields
    PixelData(uint8_t mat, uint8_t cat, uint8_t temp, uint8_t flg = 0)
        : material(mat), category(cat), temperature(temp), flags(flg) {}

    // Check if pixel is empty (no material)
    bool is_empty() const { return category == CAT_EMPTY; }

    // Check if pixel is solid (static or powder)
    bool is_solid() const { return category == CAT_STATIC || category == CAT_POWDER; }

    // Check if pixel is mobile (powder, liquid, or gas)
    bool is_mobile() const {
        return category == CAT_POWDER || category == CAT_LIQUID || category == CAT_GAS;
    }
};

// Ensure the struct is exactly 4 bytes (no padding)
static_assert(sizeof(PixelData) == 4, "PixelData must be exactly 4 bytes");

} // namespace engine::simulation
