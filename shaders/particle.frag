#version 450 core

uniform sampler1D u_palette;

flat in uint v_material;
flat in uint v_alive;

out vec4 frag_color;

void main() {
    if (v_alive == 0u) discard;

    // Look up color from palette (same as grid renderer)
    float palette_coord = (float(v_material) + 0.5) / 256.0;
    frag_color = texture(u_palette, palette_coord);
}
