// Shared material property accessors.
// Requires: SSBO `mat_props` with 2 uints per material entry bound at binding 2.
//
// Engine packing: Word 0 = density(8) | category(4) | user_data_low(20)
//                 Word 1 = user_data_high(32)
// Game packing:   user_data_low  = melt_point(8) | melt_into(8) | (unused 4)
//                 user_data_high = boil_point(8) | boil_into(8) | default_temp(8) | flags(8)

uint getDensity(uint id)     { return (mat_props[id * 2u] >>  0) & 0xFFu; }
uint getCategory(uint id)    { return (mat_props[id * 2u] >>  8) & 0x0Fu; }
uint getMeltPoint(uint id)   { return (mat_props[id * 2u] >> 12) & 0xFFu; }
uint getMeltInto(uint id)    { return (mat_props[id * 2u] >> 20) & 0xFFu; }
uint getBoilPoint(uint id)   { return (mat_props[id * 2u + 1u] >>  0) & 0xFFu; }
uint getBoilInto(uint id)    { return (mat_props[id * 2u + 1u] >>  8) & 0xFFu; }
uint getDefaultTemp(uint id) { return (mat_props[id * 2u + 1u] >> 16) & 0xFFu; }
uint getFlags(uint id)       { return (mat_props[id * 2u + 1u] >> 24) & 0xFFu; }

const uint CAT_EMPTY  = 0u;
const uint CAT_STATIC = 1u;
const uint CAT_POWDER = 2u;
const uint CAT_LIQUID = 3u;
const uint CAT_GAS    = 4u;

bool isMobile(uint cat) {
    return cat == CAT_POWDER || cat == CAT_LIQUID || cat == CAT_GAS;
}
