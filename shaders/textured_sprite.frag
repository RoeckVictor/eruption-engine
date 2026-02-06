#version 450 core

uniform sampler2D u_texture;

in vec2 v_uv;
in vec4 v_color;
out vec4 frag_color;

void main() {
    vec4 tex_color = texture(u_texture, v_uv);
    frag_color = tex_color * v_color;
    if (frag_color.a < 0.01) discard;
}
