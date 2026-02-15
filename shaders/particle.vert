#version 450 core

struct Particle {
    vec2 pos;
    vec2 vel;
    uint material;
    float lifetime;
    uint flags;
    float _pad;
};

layout(std430, binding = 3) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform vec2  u_camera_pos;
uniform vec2  u_screen_size;
uniform float u_zoom;

flat out uint v_material;
flat out uint v_alive;

void main() {
    Particle p = particles[gl_VertexID];

    v_material = p.material;
    v_alive = p.flags & 1u;

    if (v_alive == 0u) {
        // Dead particle — clip it
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }

    // World pixel position to NDC (Y-up world matches Y-up NDC)
    vec2 rel = p.pos - u_camera_pos;
    vec2 ndc;
    ndc.x = rel.x * 2.0 * u_zoom / u_screen_size.x;
    ndc.y = rel.y * 2.0 * u_zoom / u_screen_size.y;

    gl_Position = vec4(ndc, 0.0, 1.0);

    // Each game pixel is 'zoom' screen pixels wide
    gl_PointSize = max(1.0, u_zoom);
}
