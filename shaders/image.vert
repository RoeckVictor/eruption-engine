#version 450 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;

#ifdef VULKAN
layout(push_constant) uniform PushConstants {
    vec2 u_camera_pos;
    vec2 u_screen_size;
    float u_zoom;
    int u_screen_space;
};
#else
uniform vec2 u_camera_pos;
uniform vec2 u_screen_size;
uniform float u_zoom;
uniform int u_screen_space;
#endif

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

void main() {
    vec2 ndc;

    if (u_screen_space != 0) {
        // Screen space: pixel coordinates to NDC
        // Y-down convention (matching ImGui/editor): (0,0) is top-left
        ndc.x = (a_pos.x / u_screen_size.x) * 2.0 - 1.0;
        ndc.y = 1.0 - (a_pos.y / u_screen_size.y) * 2.0;
    } else {
        // World space: camera-centered, zoom-scaled
        vec2 rel = a_pos - u_camera_pos;
        ndc.x = rel.x * 2.0 * u_zoom / u_screen_size.x;
        ndc.y = rel.y * 2.0 * u_zoom / u_screen_size.y;
    }

    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
