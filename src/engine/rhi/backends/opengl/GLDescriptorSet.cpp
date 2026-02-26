#include "GLDescriptorSet.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include <glad/gl.h>

namespace engine::rhi {

bool GLDescriptorSetLayout::init(const DescriptorSetLayoutDesc& desc) {
    m_bindings = desc.bindings;
    return true;
}

bool GLDescriptorSet::init(const GLDescriptorSetLayout* layout) {
    if (!layout) return false;

    m_layout = layout;

    // Pre-allocate resources for each binding
    const auto& bindings = layout->bindings();
    m_resources.clear();
    m_resources.resize(bindings.size());

    for (size_t i = 0; i < bindings.size(); ++i) {
        m_resources[i].type = bindings[i].type;
    }

    return true;
}

void GLDescriptorSet::update(const DescriptorWrite* writes, uint32_t count) {
    if (!m_layout) return;

    const auto& bindings = m_layout->bindings();

    for (uint32_t i = 0; i < count; ++i) {
        const auto& write = writes[i];

        size_t binding_index = SIZE_MAX;
        for (size_t j = 0; j < bindings.size(); ++j) {
            if (bindings[j].binding == write.binding) {
                binding_index = j;
                break;
            }
        }

        if (binding_index == SIZE_MAX) continue;

        auto& res = m_resources[binding_index];
        res.type = write.type;
        res.buffer = write.buffer;
        res.buffer_offset = write.buffer_offset;
        res.buffer_range = write.buffer_range;
        res.texture = write.texture;
        res.mip_level = write.mip_level;
        res.access = write.access;
    }
}

void GLDescriptorSet::apply() const {
    if (!m_layout) return;

    const auto& bindings = m_layout->bindings();

    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& binding = bindings[i];
        const auto& res = m_resources[i];

        switch (res.type) {
            case DescriptorType::UniformBuffer: {
                if (res.buffer) {
                    auto* gl_buffer = static_cast<GLBuffer*>(res.buffer);
                    GLuint handle = gl_buffer->handle();
                    if (res.buffer_range > 0) {
                        glBindBufferRange(GL_UNIFORM_BUFFER, binding.binding, handle,
                                          static_cast<GLintptr>(res.buffer_offset),
                                          static_cast<GLsizeiptr>(res.buffer_range));
                    } else {
                        glBindBufferBase(GL_UNIFORM_BUFFER, binding.binding, handle);
                    }
                }
                break;
            }

            case DescriptorType::StorageBuffer: {
                if (res.buffer) {
                    auto* gl_buffer = static_cast<GLBuffer*>(res.buffer);
                    GLuint handle = gl_buffer->handle();
                    if (res.buffer_range > 0) {
                        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding.binding, handle,
                                          static_cast<GLintptr>(res.buffer_offset),
                                          static_cast<GLsizeiptr>(res.buffer_range));
                    } else {
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding.binding, handle);
                    }
                }
                break;
            }

            case DescriptorType::Texture:
            case DescriptorType::CombinedImageSampler: {
                if (res.texture) {
                    auto* gl_texture = static_cast<GLTexture*>(res.texture);
                    glActiveTexture(GL_TEXTURE0 + binding.binding);
                    glBindTexture(GL_TEXTURE_2D, gl_texture->handle());
                }
                break;
            }

            case DescriptorType::StorageImage: {
                if (res.texture) {
                    auto* gl_texture = static_cast<GLTexture*>(res.texture);
                    GLenum gl_access = GL_READ_WRITE;
                    switch (res.access) {
                        case BufferAccess::ReadOnly:  gl_access = GL_READ_ONLY;  break;
                        case BufferAccess::WriteOnly: gl_access = GL_WRITE_ONLY; break;
                        case BufferAccess::ReadWrite: gl_access = GL_READ_WRITE; break;
                    }
                    glBindImageTexture(binding.binding, gl_texture->handle(),
                                       res.mip_level, GL_FALSE, 0,
                                       gl_access, gl_texture->gl_internal_format());
                }
                break;
            }

            case DescriptorType::Sampler: {
                // Standalone samplers are typically combined with textures in OpenGL
                // This would require sampler objects (glGenSamplers, glBindSampler)
                // For now, this is a no-op as most usage is CombinedImageSampler
                break;
            }
        }
    }
}

}
