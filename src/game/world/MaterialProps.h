#pragma once

#include "engine/simulation/MaterialDefs.h"
#include <cstdint>

namespace game {

/// Game-specific material properties for temperature-driven phase transitions.
/// These are packed into MaterialSlot.user_data for GPU upload.
///
/// Packing into MaterialSlot.user_data[2]:
///   user_data[0][19:0] = melt_point(8) | melt_into(8) | (reserved 4 bits)
///   user_data[1][31:0] = boil_point(8) | boil_into(8) | default_temp(8) | flags(8)
struct MaterialProps {
    uint8_t density;
    uint8_t category;      // Must be <= 15
    uint8_t melt_point;
    uint8_t boil_point;
    uint8_t melt_into;
    uint8_t boil_into;
    uint8_t default_temp;
    uint8_t flags;         // Game-specific flags (e.g. MAT_FLAG_HAZARD)
};

/// Game-specific material flag bits.
constexpr uint8_t MAT_FLAG_HAZARD = 0x01;

/// Pack game MaterialProps into engine MaterialSlot.
inline engine::simulation::MaterialSlot pack_material(const MaterialProps& props) {
    engine::simulation::MaterialSlot slot{};
    slot.density = props.density;
    slot.category = props.category;

    // Pack phase transition data into user_data
    slot.user_data[0] = (uint32_t)(props.melt_point)
                      | ((uint32_t)(props.melt_into) << 8);

    slot.user_data[1] = (uint32_t)(props.boil_point)
                      | ((uint32_t)(props.boil_into) << 8)
                      | ((uint32_t)(props.default_temp) << 16)
                      | ((uint32_t)(props.flags) << 24);

    return slot;
}

/// Unpack engine MaterialSlot back to game MaterialProps (for debugging/queries).
inline MaterialProps unpack_material(const engine::simulation::MaterialSlot& slot) {
    MaterialProps props{};
    props.density = slot.density;
    props.category = slot.category;

    props.melt_point = (uint8_t)(slot.user_data[0] & 0xFFu);
    props.melt_into = (uint8_t)((slot.user_data[0] >> 8) & 0xFFu);

    props.boil_point = (uint8_t)(slot.user_data[1] & 0xFFu);
    props.boil_into = (uint8_t)((slot.user_data[1] >> 8) & 0xFFu);
    props.default_temp = (uint8_t)((slot.user_data[1] >> 16) & 0xFFu);
    props.flags = (uint8_t)((slot.user_data[1] >> 24) & 0xFFu);

    return props;
}

} // namespace game
