// Shared material property accessors.
// Requires: SSBO `mat_props` with 2 uints per material entry bound at binding 2.
//
// Engine packing (v2 with interactions):
//   Word 0: density(8) | category(4) | interaction_offset(12) | interaction_count(4) | flags(4)
//   Word 1: default_temp(8) | conductivity_u8(8) | reserved(16)
//
// Interaction table (binding 4): 4 uints per interaction
//   See InteractionCompiler.h for packed format

uint getDensity(uint id)           { return (mat_props[id * 2u] >>  0) & 0xFFu; }
uint getCategory(uint id)          { return (mat_props[id * 2u] >>  8) & 0x0Fu; }
uint getInteractionOffset(uint id) { return (mat_props[id * 2u] >> 12) & 0xFFFu; }
uint getInteractionCount(uint id)  { return (mat_props[id * 2u] >> 24) & 0x0Fu; }
uint getMatFlags(uint id)          { return (mat_props[id * 2u] >> 28) & 0x0Fu; }
uint getDefaultTemp(uint id)       { return (mat_props[id * 2u + 1u] >>  0) & 0xFFu; }
uint getConductivity(uint id)      { return (mat_props[id * 2u + 1u] >>  8) & 0xFFu; }

// Material flags (stored in MaterialDefinition.flags)
const uint MAT_FLAG_HAZARD     = 0x01u;
const uint MAT_FLAG_FLAMMABLE  = 0x02u;
const uint MAT_FLAG_CONDUCTIVE = 0x04u;

// Engine default category IDs (matching .phys files in assets/categories/)
const uint CAT_EMPTY  = 0u;
const uint CAT_STATIC = 1u;
const uint CAT_POWDER = 2u;
const uint CAT_LIQUID = 3u;
const uint CAT_GAS    = 4u;

// Pixel flags (stored in byte 3 / .a component of pixel data)
const uint FLAG_RIGIDBODY = 0x01u;            // Pixel is part of a rigidbody
const uint FLAG_CONVERT_TO_PARTICLE = 0x02u;  // Movable pixel should become a particle

bool hasFlag(uvec4 pixel, uint flag) {
    return (pixel.a & flag) != 0u;
}

void setFlag(inout uvec4 pixel, uint flag) {
    pixel.a |= flag;
}
