#pragma once

namespace engine::graphics {

class RenderContext {
public:
    void clear(float r, float g, float b, float a = 1.0f);
    void set_viewport(int x, int y, int w, int h);
    void memory_barrier_image_access();
    void memory_barrier_buffer();

    /// Dispatch compute shader with optional memory barrier
    /// @param barrier_flags OpenGL barrier flags (e.g., GL_SHADER_STORAGE_BARRIER_BIT), 0 for no barrier
    void dispatch_compute(int groups_x, int groups_y, int groups_z, unsigned int barrier_flags = 0);

    bool check_error(const char* context = nullptr);
};

} // namespace engine::graphics
