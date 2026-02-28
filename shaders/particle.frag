#version 450 core

flat in uint v_alive;
flat in vec4 v_color;

out vec4 frag_color;

void main() {
    if (v_alive == 0u) discard;

    frag_color = v_color;
}
