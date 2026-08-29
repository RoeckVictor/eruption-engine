#version 450 core

layout(binding = 0) uniform sampler2D u_grid;

#ifdef VULKAN
layout(push_constant) uniform PushConstants {
    float u_opacity;
    vec4 u_tint;
};
#else
uniform float u_opacity;        // Opacity multiplier
uniform vec4 u_tint;            // Tint color (default white = no tint)
#endif

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 frag_color;

void main() {
    vec4 base_color = texture(u_grid, v_uv);

    frag_color = base_color * u_tint * vec4(1.0, 1.0, 1.0, u_opacity);

    // Discard fully transparent pixels
    if (frag_color.a < 0.01) discard;
}
