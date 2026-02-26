#include "GLCommandBuffer.h"
#include "GLPipeline.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLFramebuffer.h"
#include "GLDescriptorSet.h"
#include <glad/gl.h>

namespace engine::rhi {

bool GLCommandBuffer::init() {
    return true;
}

void GLCommandBuffer::begin() {
    m_commands.clear();
    m_recording = true;
}

void GLCommandBuffer::end() {
    m_recording = false;
}

void GLCommandBuffer::reset() {
    m_commands.clear();
    m_recording = false;
}

void GLCommandBuffer::bind_pipeline(RHIPipeline* pipeline) {
    if (m_recording) {
        m_commands.push_back(cmd::BindPipeline{pipeline});
    }
}

void GLCommandBuffer::bind_framebuffer(RHIFramebuffer* framebuffer) {
    if (m_recording) {
        m_commands.push_back(cmd::BindFramebuffer{framebuffer});
    }
}

void GLCommandBuffer::set_viewport(int x, int y, int width, int height) {
    if (m_recording) {
        m_commands.push_back(cmd::SetViewport{x, y, width, height});
    }
}

void GLCommandBuffer::set_scissor(int x, int y, int width, int height) {
    if (m_recording) {
        m_commands.push_back(cmd::SetScissor{x, y, width, height});
    }
}

void GLCommandBuffer::bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding) {
    if (m_recording) {
        m_commands.push_back(cmd::BindVertexBuffer{buffer, binding});
    }
}

void GLCommandBuffer::bind_index_buffer(RHIBuffer* buffer, IndexType type) {
    if (m_recording) {
        m_commands.push_back(cmd::BindIndexBuffer{buffer, type});
    }
}

void GLCommandBuffer::bind_texture(const RHITexture* texture, uint32_t unit) {
    if (m_recording) {
        m_commands.push_back(cmd::BindTexture{texture, unit});
    }
}

void GLCommandBuffer::bind_descriptor_set(RHIDescriptorSet* set, uint32_t index) {
    if (m_recording) {
        m_commands.push_back(cmd::BindDescriptorSet{set, index});
    }
}

void GLCommandBuffer::bind_uniform_buffer(RHIBuffer* buffer, uint32_t binding) {
    if (m_recording) {
        m_commands.push_back(cmd::BindUniformBuffer{buffer, binding});
    }
}

void GLCommandBuffer::bind_storage_buffer(RHIBuffer* buffer, uint32_t binding, BufferAccess access) {
    if (m_recording) {
        m_commands.push_back(cmd::BindStorageBuffer{buffer, binding, access});
    }
}

void GLCommandBuffer::bind_storage_image(RHITexture* texture, uint32_t binding, BufferAccess access) {
    if (m_recording) {
        m_commands.push_back(cmd::BindStorageImage{texture, binding, access});
    }
}

void GLCommandBuffer::clear_color(float r, float g, float b, float a) {
    if (m_recording) {
        m_commands.push_back(cmd::ClearColor{r, g, b, a});
    }
}

void GLCommandBuffer::clear_depth(float depth) {
    if (m_recording) {
        m_commands.push_back(cmd::ClearDepth{depth});
    }
}

void GLCommandBuffer::clear_stencil(int value) {
    if (m_recording) {
        m_commands.push_back(cmd::ClearStencil{value});
    }
}

void GLCommandBuffer::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count) {
    if (m_recording) {
        m_commands.push_back(cmd::Draw{vertex_count, first_vertex, instance_count});
    }
}

void GLCommandBuffer::draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count) {
    if (m_recording) {
        m_commands.push_back(cmd::DrawIndexed{index_count, first_index, instance_count});
    }
}

void GLCommandBuffer::dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (m_recording) {
        m_commands.push_back(cmd::DispatchCompute{groups_x, groups_y, groups_z});
    }
}

void GLCommandBuffer::memory_barrier(BarrierFlags flags) {
    if (m_recording) {
        m_commands.push_back(cmd::MemoryBarrier{flags});
    }
}

void GLCommandBuffer::execute() {
    for (const auto& command : m_commands) {
        execute_command(command);
    }
}

void GLCommandBuffer::execute_command(const cmd::Command& command) {
    // Track current state for draw commands
    static thread_local GLPipeline* current_pipeline = nullptr;
    static thread_local IndexType current_index_type = IndexType::UInt32;

    std::visit([&](auto&& cmd) {
        using T = std::decay_t<decltype(cmd)>;

        if constexpr (std::is_same_v<T, cmd::BindPipeline>) {
            current_pipeline = static_cast<GLPipeline*>(cmd.pipeline);
            if (current_pipeline) {
                current_pipeline->bind();
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindFramebuffer>) {
            if (cmd.framebuffer) {
                auto* gl_fb = static_cast<GLFramebuffer*>(cmd.framebuffer);
                gl_fb->bind();
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::SetViewport>) {
            glViewport(cmd.x, cmd.y, cmd.width, cmd.height);
        }
        else if constexpr (std::is_same_v<T, cmd::SetScissor>) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(cmd.x, cmd.y, cmd.width, cmd.height);
        }
        else if constexpr (std::is_same_v<T, cmd::BindVertexBuffer>) {
            if (cmd.buffer) {
                auto* gl_buffer = static_cast<GLBuffer*>(cmd.buffer);
                uint32_t stride = current_pipeline ? current_pipeline->vertex_stride() : 0;
                glBindVertexBuffer(cmd.binding, gl_buffer->handle(), 0, stride);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindIndexBuffer>) {
            current_index_type = cmd.type;
            if (cmd.buffer) {
                auto* gl_buffer = static_cast<GLBuffer*>(cmd.buffer);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buffer->handle());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindTexture>) {
            if (cmd.texture) {
                auto* gl_texture = static_cast<const GLTexture*>(cmd.texture);
                gl_texture->bind(cmd.unit);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindDescriptorSet>) {
            if (cmd.set) {
                auto* gl_set = static_cast<GLDescriptorSet*>(cmd.set);
                gl_set->apply();
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindUniformBuffer>) {
            if (cmd.buffer) {
                auto* gl_buffer = static_cast<GLBuffer*>(cmd.buffer);
                glBindBufferBase(GL_UNIFORM_BUFFER, cmd.binding, gl_buffer->handle());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindStorageBuffer>) {
            if (cmd.buffer) {
                auto* gl_buffer = static_cast<GLBuffer*>(cmd.buffer);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, cmd.binding, gl_buffer->handle());
            }
        }
        else if constexpr (std::is_same_v<T, cmd::BindStorageImage>) {
            if (cmd.texture) {
                auto* gl_texture = static_cast<GLTexture*>(cmd.texture);
                ImageAccess img_access = ImageAccess::ReadWrite;
                switch (cmd.access) {
                    case BufferAccess::ReadOnly:  img_access = ImageAccess::ReadOnly;  break;
                    case BufferAccess::WriteOnly: img_access = ImageAccess::WriteOnly; break;
                    case BufferAccess::ReadWrite: break; // Already set as default
                }
                gl_texture->bind_as_image(cmd.binding, img_access);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::ClearColor>) {
            glClearColor(cmd.r, cmd.g, cmd.b, cmd.a);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else if constexpr (std::is_same_v<T, cmd::ClearDepth>) {
            glClearDepth(cmd.depth);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        else if constexpr (std::is_same_v<T, cmd::ClearStencil>) {
            glClearStencil(cmd.value);
            glClear(GL_STENCIL_BUFFER_BIT);
        }
        else if constexpr (std::is_same_v<T, cmd::Draw>) {
            if (!current_pipeline) return;

            GLenum mode = GL_TRIANGLES;
            switch (current_pipeline->primitive_type()) {
                case PrimitiveTopology::Points: mode = GL_POINTS; break;
                case PrimitiveTopology::Lines: mode = GL_LINES; break;
                case PrimitiveTopology::LineStrip: mode = GL_LINE_STRIP; break;
                case PrimitiveTopology::Triangles: mode = GL_TRIANGLES; break;
                case PrimitiveTopology::TriangleStrip: mode = GL_TRIANGLE_STRIP; break;
                case PrimitiveTopology::TriangleFan: mode = GL_TRIANGLE_FAN; break;
            }

            if (cmd.instance_count > 1) {
                glDrawArraysInstanced(mode, static_cast<GLint>(cmd.first_vertex),
                                      static_cast<GLsizei>(cmd.vertex_count),
                                      static_cast<GLsizei>(cmd.instance_count));
            } else {
                glDrawArrays(mode, static_cast<GLint>(cmd.first_vertex),
                             static_cast<GLsizei>(cmd.vertex_count));
            }
        }
        else if constexpr (std::is_same_v<T, cmd::DrawIndexed>) {
            if (!current_pipeline) return;

            GLenum mode = GL_TRIANGLES;
            switch (current_pipeline->primitive_type()) {
                case PrimitiveTopology::Points: mode = GL_POINTS; break;
                case PrimitiveTopology::Lines: mode = GL_LINES; break;
                case PrimitiveTopology::LineStrip: mode = GL_LINE_STRIP; break;
                case PrimitiveTopology::Triangles: mode = GL_TRIANGLES; break;
                case PrimitiveTopology::TriangleStrip: mode = GL_TRIANGLE_STRIP; break;
                case PrimitiveTopology::TriangleFan: mode = GL_TRIANGLE_FAN; break;
            }

            GLenum type = (current_index_type == IndexType::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            size_t index_size = (current_index_type == IndexType::UInt16) ? 2 : 4;
            // Cast first_index to size_t BEFORE multiplication to prevent overflow
            const void* offset = reinterpret_cast<const void*>(static_cast<size_t>(cmd.first_index) * index_size);

            if (cmd.instance_count > 1) {
                glDrawElementsInstanced(mode, static_cast<GLsizei>(cmd.index_count), type,
                                        offset, static_cast<GLsizei>(cmd.instance_count));
            } else {
                glDrawElements(mode, static_cast<GLsizei>(cmd.index_count), type, offset);
            }
        }
        else if constexpr (std::is_same_v<T, cmd::DispatchCompute>) {
            glDispatchCompute(cmd.groups_x, cmd.groups_y, cmd.groups_z);
        }
        else if constexpr (std::is_same_v<T, cmd::MemoryBarrier>) {
            GLbitfield gl_flags = 0;

            if (has_flag(cmd.flags, BarrierFlags::VertexBuffer)) {
                gl_flags |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::IndexBuffer)) {
                gl_flags |= GL_ELEMENT_ARRAY_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::UniformBuffer)) {
                gl_flags |= GL_UNIFORM_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::StorageBuffer)) {
                gl_flags |= GL_SHADER_STORAGE_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::TextureRead)) {
                gl_flags |= GL_TEXTURE_FETCH_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::TextureWrite) || has_flag(cmd.flags, BarrierFlags::ImageAccess)) {
                gl_flags |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::Framebuffer)) {
                gl_flags |= GL_FRAMEBUFFER_BARRIER_BIT;
            }
            if (has_flag(cmd.flags, BarrierFlags::All)) {
                gl_flags = GL_ALL_BARRIER_BITS;
            }

            if (gl_flags != 0) {
                glMemoryBarrier(gl_flags);
            }
        }
    }, command);
}

} // namespace engine::rhi
