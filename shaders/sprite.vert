#version 450 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;

uniform vec2 u_camera_pos;
uniform vec2 u_screen_size;
uniform float u_zoom;

out vec4 v_color;

void main() {
    // World position to NDC: camera-centered, zoom-scaled
    vec2 rel = a_pos - u_camera_pos;
    vec2 ndc;
    ndc.x = rel.x * 2.0 * u_zoom / u_screen_size.x;
    ndc.y = rel.y * 2.0 * u_zoom / u_screen_size.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
