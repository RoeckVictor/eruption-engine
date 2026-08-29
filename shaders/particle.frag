#version 450 core

layout(location = 0) flat in uint v_material; // unused but must match vertex output
layout(location = 1) flat in uint v_alive;
layout(location = 2) flat in vec4 v_color;

layout(location = 0) out vec4 frag_color;

void main() {
    if (v_alive == 0u) discard;

    frag_color = v_color;
}
