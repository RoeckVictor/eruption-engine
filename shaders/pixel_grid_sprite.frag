#version 450 core

uniform sampler2D u_grid;       // RGBA8 direct color texture
uniform float u_opacity;        // Opacity multiplier
uniform vec4 u_tint;            // Tint color (default white = no tint)

in vec2 v_uv;
in vec4 v_color;
out vec4 frag_color;

void main() {
    vec4 base_color = texture(u_grid, v_uv);

    frag_color = base_color * u_tint * vec4(1.0, 1.0, 1.0, u_opacity);

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) discard;
}
