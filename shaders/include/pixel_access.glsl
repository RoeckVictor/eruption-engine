// Shared pixel SSBO access helpers.
// Requires: uniform uint u_pixel_size; uniform int u_grid_width, u_grid_height;
// Requires: Define PIXEL_READ_SSBO before including, or have a buffer named 'pixel_data'.
//
// Pixel layout (8 bytes = 2 uint32s):
//   Word 0 (bytes 0-3): material | category | temperature | flags
//   Word 1 (bytes 4-7): color_r | color_g | color_b | color_a
//
// For sim_step.comp, also define PIXEL_WRITE_SSBO for the write buffer.

// Default SSBO names if not specified
#ifndef PIXEL_READ_SSBO
#define PIXEL_READ_SSBO pixel_data
#endif

// ============================================================================
// Pack/unpack utilities (must be defined first, used by other functions)
// ============================================================================

// Unpack a uint32 into 4 component bytes: r=byte0, g=byte1, b=byte2, a=byte3
uvec4 unpackPixel(uint packed) {
    return uvec4(
        (packed >>  0) & 0xFFu,
        (packed >>  8) & 0xFFu,
        (packed >> 16) & 0xFFu,
        (packed >> 24) & 0xFFu
    );
}

// Pack 4 component bytes back into a uint32.
uint packPixel(uvec4 p) {
    return (p.r & 0xFFu) | ((p.g & 0xFFu) << 8) | ((p.b & 0xFFu) << 16) | ((p.a & 0xFFu) << 24);
}

// ============================================================================
// 8-byte pixel access (optimized for u_pixel_size == 8)
// ============================================================================

// Read simulation data (word 0) for an 8-byte pixel.
// Returns packed uint: material(8) | category(8) | temperature(8) | flags(8)
uint readPixelSim(ivec2 pos) {
    uint idx = uint(pos.y) * uint(u_grid_width) + uint(pos.x);
    return PIXEL_READ_SSBO[idx * 2u];
}

// Read color data (word 1) for an 8-byte pixel.
// Returns packed uint: r(8) | g(8) | b(8) | a(8)
uint readPixelColor(ivec2 pos) {
    uint idx = uint(pos.y) * uint(u_grid_width) + uint(pos.x);
    return PIXEL_READ_SSBO[idx * 2u + 1u];
}

// Read both simulation and color data for an 8-byte pixel.
// Returns: sim in .xy (unpacked), color in .zw (unpacked)
// Actually returns two uvec4s via out parameters for clarity.
void readPixel8(ivec2 pos, out uvec4 sim, out uvec4 color) {
    uint idx = uint(pos.y) * uint(u_grid_width) + uint(pos.x);
    uint word0 = PIXEL_READ_SSBO[idx * 2u];
    uint word1 = PIXEL_READ_SSBO[idx * 2u + 1u];
    sim = unpackPixel(word0);
    color = unpackPixel(word1);
}

// Write both simulation and color data for an 8-byte pixel.
// Only available if PIXEL_WRITE_SSBO is defined before including this file.
#ifdef PIXEL_WRITE_SSBO
void writePixel8(ivec2 pos, uvec4 sim, uvec4 color) {
    uint idx = uint(pos.y) * uint(u_grid_width) + uint(pos.x);
    PIXEL_WRITE_SSBO[idx * 2u] = packPixel(sim);
    PIXEL_WRITE_SSBO[idx * 2u + 1u] = packPixel(color);
}
#endif

// ============================================================================
// Legacy 4-byte pixel access (for backwards compatibility)
// ============================================================================

// Read the first 4 packed bytes of a pixel from the SSBO.
// `pos` is the grid coordinate.
// Returns a uint32 with bytes [0..3] = the first 4 bytes of the pixel.
uint readPackedPixel(ivec2 pos) {
    uint idx = uint(pos.y) * uint(u_grid_width) + uint(pos.x);
    uint byte_offset = idx * u_pixel_size;
    uint word_offset = byte_offset / 4u;
    uint byte_in_word = byte_offset % 4u;

    uint packed = PIXEL_READ_SSBO[word_offset];
    if (byte_in_word != 0u) {
        uint next_word = PIXEL_READ_SSBO[word_offset + 1u];
        uint shift_bits = byte_in_word * 8u;
        packed = (packed >> shift_bits) | (next_word << (32u - shift_bits));
    }
    return packed;
}

// ============================================================================
// Color utilities
// ============================================================================

// Convert packed color (r|g|b|a) to normalized vec4 for rendering
vec4 colorToVec4(uint packed) {
    return vec4(
        float((packed >>  0) & 0xFFu) / 255.0,
        float((packed >>  8) & 0xFFu) / 255.0,
        float((packed >> 16) & 0xFFu) / 255.0,
        float((packed >> 24) & 0xFFu) / 255.0
    );
}

// Blend two colors: result = src * alpha + dst * (1 - alpha)
uvec4 blendColors(uvec4 src, uvec4 dst, float alpha) {
    return uvec4(
        uint(mix(float(dst.r), float(src.r), alpha)),
        uint(mix(float(dst.g), float(src.g), alpha)),
        uint(mix(float(dst.b), float(src.b), alpha)),
        uint(mix(float(dst.a), float(src.a), alpha))
    );
}
