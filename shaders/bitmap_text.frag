#version 450 core

layout(binding = 0) uniform sampler2D u_texture;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 frag_color;

void main() {
    // Sample alpha from texture (glyph coverage)
    float alpha = texture(u_texture, v_uv).a;

    // Apply vertex color with texture alpha
    frag_color = vec4(v_color.rgb, v_color.a * alpha);

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) discard;
}
