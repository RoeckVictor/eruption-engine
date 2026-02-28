#version 450 core

struct Particle {
    vec2 pos;
    vec2 vel;
    uint material;
    float lifetime;
    uint flags;
    uint color;
};

layout(std430, binding = 3) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform vec2  u_camera_pos;
uniform vec2  u_screen_size;
uniform float u_zoom;
uniform vec2  u_grid_origin;
uniform int   u_grid_height;

flat out uint v_material;
flat out uint v_alive;
flat out vec4 v_color;

void main() {
    Particle p = particles[gl_VertexID];

    v_material = p.material;
    v_alive = p.flags & 1u;

    // Unpack color (r|g|b|a as bytes 0-3)
    v_color = vec4(
        float((p.color >>  0) & 0xFFu) / 255.0,
        float((p.color >>  8) & 0xFFu) / 255.0,
        float((p.color >> 16) & 0xFFu) / 255.0,
        float((p.color >> 24) & 0xFFu) / 255.0
    );

    if (v_alive == 0u) {
        // Dead particle — clip it
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }

    // Convert grid coords (Y-down) to world coords (Y-up)
    vec2 world_pos;
    world_pos.x = u_grid_origin.x + p.pos.x;
    world_pos.y = u_grid_origin.y + float(u_grid_height - 1) - p.pos.y;

    // World pixel position to NDC
    vec2 rel = world_pos - u_camera_pos;
    vec2 ndc;
    ndc.x = rel.x * 2.0 * u_zoom / u_screen_size.x;
    ndc.y = rel.y * 2.0 * u_zoom / u_screen_size.y;

    gl_Position = vec4(ndc, 0.0, 1.0);

    // Each game pixel is 'zoom' screen pixels wide
    gl_PointSize = max(1.0, u_zoom);
}
