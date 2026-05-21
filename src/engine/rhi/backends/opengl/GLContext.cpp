#include "GLContext.h"
#include "GLPipeline.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLFramebuffer.h"
#include "GLShader.h"
#include "GLCommandBuffer.h"
#include "GLDescriptorSet.h"
#include "GLSynchronization.h"
#include "engine/core/Logger.h"
#include <glad/gl.h>

namespace engine::rhi {

namespace {

GLenum primitive_topology_to_gl(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::Points:        return GL_POINTS;
        case PrimitiveTopology::Lines:         return GL_LINES;
        case PrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
        case PrimitiveTopology::Triangles:     return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::TriangleFan:   return GL_TRIANGLE_FAN;
    }
    return GL_TRIANGLES;
}

const char* gl_error_to_string(GLenum err) {
    switch (err) {
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
        default:                               return "Unknown error";
    }
}

} // anonymous namespace

void GLContext::begin_frame() {
    // Ensure we're rendering to the default framebuffer at the start of each frame.
    // ImGui multi-viewport rendering (update_platform_windows) may leave a non-default
    // framebuffer bound, which would cause subsequent clear/draw calls to target the
    // wrong surface — resulting in rapid flickering of the main window.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Reset GL state that may leak from the previous frame's rendering.
    // Panel FBO rendering (ViewportPanel, ScreenPanel, GamePanel) may leave
    // depth test, scissor, or blend enabled which interferes with ImGui.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
}

void GLContext::end_frame() {
    glFlush();
}

void GLContext::set_viewport(int x, int y, int w, int h) {
    glViewport(x, y, w, h);
}

void GLContext::set_scissor(int x, int y, int w, int h) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

void GLContext::enable_scissor_test(bool enable) {
    if (enable) {
        glEnable(GL_SCISSOR_TEST);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GLContext::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLContext::clear_depth(float depth) {
    glClearDepth(depth);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void GLContext::clear_stencil(int stencil) {
    glClearStencil(stencil);
    glClear(GL_STENCIL_BUFFER_BIT);
}

void GLContext::bind_pipeline(RHIPipeline* pipeline) {
    m_current_pipeline = static_cast<GLPipeline*>(pipeline);
    if (m_current_pipeline) {
        m_current_pipeline->bind();
    }
}

void GLContext::bind_framebuffer(RHIFramebuffer* fb) {
    if (fb) {
        auto* gl_fb = static_cast<GLFramebuffer*>(fb);
        gl_fb->bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void GLContext::bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding, size_t offset) {
    if (buffer) {
        auto* gl_buffer = static_cast<GLBuffer*>(buffer);
        uint32_t stride = m_current_pipeline ? m_current_pipeline->vertex_stride() : 0;
        glBindVertexBuffer(binding, gl_buffer->handle(), static_cast<GLintptr>(offset), stride);
    }
}

void GLContext::bind_index_buffer(RHIBuffer* buffer, uint32_t index_type) {
    m_index_type = index_type;
    if (buffer) {
        auto* gl_buffer = static_cast<GLBuffer*>(buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buffer->handle());
    }
}

void GLContext::bind_texture(const RHITexture* texture, uint32_t unit) {
    if (texture) {
        auto* gl_texture = static_cast<const GLTexture*>(texture);
        gl_texture->bind(unit);
    }
}

void GLContext::bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) {
    if (buffer) {
        auto* gl_buffer = static_cast<GLBuffer*>(buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, gl_buffer->handle());
    }
}

void GLContext::bind_image(RHITexture* texture, uint32_t unit, ImageAccess access) {
    if (texture) {
        auto* gl_texture = static_cast<GLTexture*>(texture);
        gl_texture->bind_as_image(unit, access);
    }
}

void GLContext::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count) {
    if (!m_current_pipeline) return;

    GLenum mode = primitive_topology_to_gl(m_current_pipeline->primitive_type());

    if (instance_count > 1) {
        glDrawArraysInstanced(mode, static_cast<GLint>(first_vertex),
                              static_cast<GLsizei>(vertex_count),
                              static_cast<GLsizei>(instance_count));
    } else {
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
    }
}

void GLContext::draw_indexed(uint32_t index_count, uint32_t first_index,
                             int vertex_offset, uint32_t instance_count) {
    if (!m_current_pipeline) return;

    GLenum mode = primitive_topology_to_gl(m_current_pipeline->primitive_type());
    GLenum type = (m_index_type == 2) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    const void* offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(first_index) * m_index_type);

    if (instance_count > 1) {
        glDrawElementsInstancedBaseVertex(mode, static_cast<GLsizei>(index_count), type,
                                          offset, static_cast<GLsizei>(instance_count),
                                          vertex_offset);
    } else {
        glDrawElementsBaseVertex(mode, static_cast<GLsizei>(index_count), type,
                                 offset, vertex_offset);
    }
}

void GLContext::dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    glDispatchCompute(groups_x, groups_y, groups_z);
}

void GLContext::copy_texture_to_buffer(
    const RHITexture* src_texture,
    int x, int y, int w, int h,
    RHIBuffer* dst_buffer,
    size_t dst_offset)
{
    if (!src_texture || !dst_buffer) return;

    auto* gl_tex = static_cast<const GLTexture*>(src_texture);
    auto* gl_buf = static_cast<GLBuffer*>(dst_buffer);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, gl_buf->handle());

    glGetTextureSubImage(
        gl_tex->handle(), 0,
        x, y, 0,
        w, h, 1,
        gl_tex->gl_format(), gl_tex->gl_type(),
        static_cast<GLsizei>(gl_buf->size() - dst_offset),
        reinterpret_cast<void*>(static_cast<uintptr_t>(dst_offset)));

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void GLContext::memory_barrier(BarrierFlags flags) {
    GLbitfield gl_flags = 0;

    if (has_flag(flags, BarrierFlags::VertexBuffer)) {
        gl_flags |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::IndexBuffer)) {
        gl_flags |= GL_ELEMENT_ARRAY_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::UniformBuffer)) {
        gl_flags |= GL_UNIFORM_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::StorageBuffer)) {
        gl_flags |= GL_SHADER_STORAGE_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::TextureRead)) {
        gl_flags |= GL_TEXTURE_FETCH_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::TextureWrite) || has_flag(flags, BarrierFlags::ImageAccess)) {
        gl_flags |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::Framebuffer)) {
        gl_flags |= GL_FRAMEBUFFER_BARRIER_BIT;
    }
    if (has_flag(flags, BarrierFlags::All)) {
        gl_flags = GL_ALL_BARRIER_BITS;
    }

    if (gl_flags != 0) {
        glMemoryBarrier(gl_flags);
    }
}

void GLContext::submit(RHICommandBuffer* cmd_buffer, RHIFence* signal_fence) {
    if (!cmd_buffer) return;

    auto* gl_cmd_buffer = static_cast<GLCommandBuffer*>(cmd_buffer);
    gl_cmd_buffer->execute();

    if (signal_fence) {
        auto* gl_fence = static_cast<GLFence*>(signal_fence);
        gl_fence->insert();
    }
}

void GLContext::bind_descriptor_set(RHIDescriptorSet* set, uint32_t /*index*/) {
    if (!set) return;

    auto* gl_set = static_cast<GLDescriptorSet*>(set);
    gl_set->apply();
}

bool GLContext::check_error(const char* context) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        if (context) {
            Logger::instance().error("OpenGL", "Error in %s: %s (0x%x)", context, gl_error_to_string(err), err);
        } else {
            Logger::instance().error("OpenGL", "Error: %s (0x%x)", gl_error_to_string(err), err);
        }
        return true;
    }
    return false;
}

}
