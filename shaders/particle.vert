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

    // World pixel position to NDC (same math as render_grid.frag, inverted)
    vec2 visible_size = u_screen_size / u_zoom;
    vec2 screen_frac = (p.pos - u_camera_pos + visible_size * 0.5) / visible_size;

    // NDC: x [-1,1] left to right, y [-1,1] bottom to top
    // screen_frac: (0,0) = top-left, (1,1) = bottom-right
    vec2 ndc = screen_frac * 2.0 - 1.0;
    ndc.y = -ndc.y; // flip Y (screen Y goes down, NDC Y goes up)

    gl_Position = vec4(ndc, 0.0, 1.0);

    // Each game pixel is 'zoom' screen pixels wide
    gl_PointSize = max(1.0, u_zoom);
}
