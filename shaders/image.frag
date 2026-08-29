#version 450 core

layout(binding = 0) uniform sampler2D u_texture;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 frag_color;

void main() {
    vec4 tex_color = texture(u_texture, v_uv);
    frag_color = tex_color * v_color;

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) discard;
}
