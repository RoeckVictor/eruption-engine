#pragma once

#include <cstdint>

namespace engine::simulation {

// Material category flags used by the simulation compute shader
// Games define concrete materials; the engine only cares about categories
enum MaterialCategory : uint8_t {
    CAT_EMPTY  = 0,
    CAT_STATIC = 1,
    CAT_POWDER = 2,
    CAT_LIQUID = 3,
    CAT_GAS    = 4,
};

namespace PixelFlags {
    constexpr uint8_t FLAG_RIGIDBODY = 0x01;
    constexpr uint8_t FLAG_CONVERT_TO_PARTICLE = 0x02;
}

// Minimal per-material slot for the engine.
// The engine handles density and category for simulation purposes.
// Games can pack their own mechanics (phase transitions, combustion, etc.)
// into the user_data blob. The compute shader interprets user_data.
// Bit layout per material (2 uint32s uploaded to SSBO):
//   Word 0: density(8) | category(4) | user_data_0(20)
//   Word 1: user_data_1(32)
// This gives games 52 bits of custom data per material
struct MaterialSlot {
    uint8_t density;
    uint8_t category;
    uint32_t user_data[2];
};

}
