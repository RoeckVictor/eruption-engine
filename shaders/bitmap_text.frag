#version 450 core

uniform sampler2D u_texture;

in vec2 v_uv;
in vec4 v_color;

out vec4 frag_color;

void main() {
    // Sample alpha from texture (glyph coverage)
    float alpha = texture(u_texture, v_uv).a;

    // Apply vertex color with texture alpha
    frag_color = vec4(v_color.rgb, v_color.a * alpha);

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) discard;
}
