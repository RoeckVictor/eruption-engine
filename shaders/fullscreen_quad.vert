#version 450 core

#ifdef VULKAN
#define gl_VertexID gl_VertexIndex
#endif

layout(location = 0) out vec2 v_uv;

void main() {
    v_uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(v_uv * 2.0 - 1.0, 0.0, 1.0);
    v_uv.y = 1.0 - v_uv.y;
}
