// Shared pixel SSBO access helpers.
// Requires: uniform uint u_pixel_size; uniform int u_grid_width, u_grid_height;
// Requires: Define PIXEL_READ_SSBO before including, or have a buffer named 'pixel_data'.

// Default SSBO name if not specified
#ifndef PIXEL_READ_SSBO
#define PIXEL_READ_SSBO pixel_data
#endif

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
