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
// Bit layout per material (2 uint32s uploaded to SSBO):
//   Word 0: density(8) | category(4) | interaction_offset(12) | interaction_count(4) | flags(4)
//   Word 1: default_temp(8) | conductivity_u8(8) | reserved(16)
// Interaction data is stored in a separate SSBO (binding 4).
struct MaterialSlot {
    uint8_t density;
    uint8_t category;
    uint16_t interaction_offset;  // Offset into interaction table (max 4095)
    uint8_t interaction_count;    // Number of interactions (max 15)
    uint8_t flags;                // MAT_FLAG_HAZARD, MAT_FLAG_FLAMMABLE, etc.
    uint8_t default_temp;
    uint8_t conductivity_u8;      // 0-255 = 0.0-1.0
};

// Packed interaction for GPU (6 uint32s = 24 bytes per interaction)
// Word 0 (conditions):
//   bits 0-7:   temp_above
//   bits 8-15:  temp_below
//   bits 16-23: contact_material_1 (ID, 255 = none)
//   bits 24-31: contact_material_2 (ID, 255 = none)
// Word 1 (more conditions + type):
//   bits 0-7:   contact_material_3 (ID, 255 = none)
//   bits 8-15:  contact_material_4 (ID, 255 = none)
//   bits 16-19: interaction_type (InteractionType enum)
//   bits 20-23: self_has_flag (bitmask)
//   bits 24-27: contact_with_flag (bitmask for material flags)
//   bits 28-31: contact_with_category (MaterialCategory, 15 = any)
// Word 2 (effects):
//   bits 0-3:   effect_type_1 (EffectType enum)
//   bits 4-11:  effect_param_1 (material ID or value)
//   bits 12-15: effect_type_2 (EffectType enum, 15 = none)
//   bits 16-23: effect_param_2
//   bits 24-26: color_behavior_1 (ColorBehavior enum: 0=None,1=Replace,2=Inherit,3=Blend,4=Add,5=Subtract)
//   bits 27-29: color_behavior_2 (ColorBehavior enum)
//   bits 30-31: reserved
// Word 3 (metadata):
//   bits 0-7:   priority
//   bits 8-15:  probability_u8 (0-255 = 0.0-1.0)
//   bits 16-31: sim_step_threshold
// Word 4 (color_delta_1): RGBA color for effect 1 Add/Subtract (0xRRGGBBAA)
// Word 5 (color_delta_2): RGBA color for effect 2 Add/Subtract (0xRRGGBBAA)
struct PackedInteraction {
    uint32_t conditions_0;
    uint32_t conditions_1;
    uint32_t effects;
    uint32_t metadata;
    uint32_t color_delta_1;
    uint32_t color_delta_2;
};

}
