#pragma once

#include "engine/simulation/MaterialDefs.h"
#include <string>
#include <cstdint>

namespace engine::simulation {

/// Runtime material definition loaded from JSON.
///
/// Represents a single material with all its properties:
/// - Physical simulation (density, category)
/// - Thermal properties (melting, boiling)
/// - Rendering (color, display name)
/// - Gameplay flags
struct MaterialDefinition {
    // Identity
    uint8_t id = 0;                // Material ID (0-255)
    std::string name;              // Display name (e.g., "Water", "Lava")
    std::string internal_name;     // Internal identifier (e.g., "water", "lava")

    // Physical properties (simulation)
    uint8_t density = 0;           // Weight/mass (affects gravity, displacement)
    MaterialCategory category = CAT_EMPTY;  // Simulation behavior category

    // Thermal properties (phase transitions)
    uint8_t melt_point = 0;        // Temperature at which material melts (0 = doesn't melt)
    uint8_t boil_point = 0;        // Temperature at which material boils (0 = doesn't boil)
    uint8_t melt_into = 0;         // Material ID to transform into when melting
    uint8_t boil_into = 0;         // Material ID to transform into when boiling
    uint8_t default_temp = 128;    // Default temperature (0-255, 128 = room temp)

    // Gameplay properties
    uint8_t flags = 0;             // Custom flags (hazard, flammable, etc.)

    // Rendering
    uint32_t color = 0xFFFFFFFF;   // RGBA color for rendering

    /// Pack this definition into a MaterialSlot for GPU upload.
    MaterialSlot to_material_slot() const {
        MaterialSlot slot{};
        slot.density = density;
        slot.category = static_cast<uint8_t>(category);

        // Pack phase transition data into user_data[0]
        slot.user_data[0] = static_cast<uint32_t>(melt_point)
                          | (static_cast<uint32_t>(melt_into) << 8);

        // Pack remaining thermal + flags into user_data[1]
        slot.user_data[1] = static_cast<uint32_t>(boil_point)
                          | (static_cast<uint32_t>(boil_into) << 8)
                          | (static_cast<uint32_t>(default_temp) << 16)
                          | (static_cast<uint32_t>(flags) << 24);

        return slot;
    }

    /// Unpack a MaterialSlot back to a definition (for debugging).
    static MaterialDefinition from_material_slot(const MaterialSlot& slot, uint8_t id) {
        MaterialDefinition def;
        def.id = id;
        def.density = slot.density;
        def.category = static_cast<MaterialCategory>(slot.category);

        def.melt_point = static_cast<uint8_t>(slot.user_data[0] & 0xFFu);
        def.melt_into = static_cast<uint8_t>((slot.user_data[0] >> 8) & 0xFFu);

        def.boil_point = static_cast<uint8_t>(slot.user_data[1] & 0xFFu);
        def.boil_into = static_cast<uint8_t>((slot.user_data[1] >> 8) & 0xFFu);
        def.default_temp = static_cast<uint8_t>((slot.user_data[1] >> 16) & 0xFFu);
        def.flags = static_cast<uint8_t>((slot.user_data[1] >> 24) & 0xFFu);

        return def;
    }
};

/// Material flag bits (stored in MaterialDefinition.flags).
constexpr uint8_t MAT_FLAG_HAZARD = 0x01;      // Causes damage to player
constexpr uint8_t MAT_FLAG_FLAMMABLE = 0x02;   // Can catch fire
constexpr uint8_t MAT_FLAG_CONDUCTIVE = 0x04;  // Conducts electricity

/// Category names for JSON parsing/display.
inline const char* category_to_string(MaterialCategory cat) {
    switch (cat) {
        case CAT_EMPTY: return "empty";
        case CAT_STATIC: return "static";
        case CAT_POWDER: return "powder";
        case CAT_LIQUID: return "liquid";
        case CAT_GAS: return "gas";
        default: return "unknown";
    }
}

inline MaterialCategory string_to_category(const std::string& str) {
    if (str == "empty") return CAT_EMPTY;
    if (str == "static") return CAT_STATIC;
    if (str == "powder") return CAT_POWDER;
    if (str == "liquid") return CAT_LIQUID;
    if (str == "gas") return CAT_GAS;
    return CAT_EMPTY;
}

} // namespace engine::simulation
