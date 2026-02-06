#include "engine/graphics/RenderContext.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::graphics {

void RenderContext::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderContext::set_viewport(int x, int y, int w, int h) {
    glViewport(x, y, w, h);
}

void RenderContext::memory_barrier_image_access() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void RenderContext::memory_barrier_buffer() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void RenderContext::dispatch_compute(int groups_x, int groups_y, int groups_z, unsigned int barrier_flags) {
    glDispatchCompute(groups_x, groups_y, groups_z);

    // Insert memory barrier if requested
    if (barrier_flags != 0) {
        glMemoryBarrier(barrier_flags);
    }
}

bool RenderContext::check_error(const char* context) {
#ifdef NDEBUG
    (void)context;
    return false;
#else
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return false;

    const char* ctx = context ? context : "OpenGL";
    while (err != GL_NO_ERROR) {
        ENGINE_ERR("GL error in %s: 0x%x", ctx, err);
        err = glGetError();
    }
    return true;
#endif
}

} // namespace engine::graphics
