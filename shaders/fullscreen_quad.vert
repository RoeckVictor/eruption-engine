#version 450 core

out vec2 v_uv;

void main() {
    // Generate fullscreen triangle from vertex ID (no VBO needed)
    v_uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
    // Flip Y so top of texture = top of screen
    v_uv.y = 1.0 - v_uv.y;
}
