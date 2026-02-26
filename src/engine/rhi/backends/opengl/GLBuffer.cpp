#include "GLBuffer.h"
#include <glad/gl.h>
#include <cstring>

namespace engine::rhi {

namespace {

GLenum buffer_type_to_gl(BufferType type) {
    switch (type) {
        case BufferType::Vertex:    return GL_ARRAY_BUFFER;
        case BufferType::Index:     return GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::Uniform:   return GL_UNIFORM_BUFFER;
        case BufferType::Storage:   return GL_SHADER_STORAGE_BUFFER;
        case BufferType::PixelPack: return GL_PIXEL_PACK_BUFFER;
        default: return GL_ARRAY_BUFFER;
    }
}

GLenum buffer_usage_to_gl(BufferUsage usage, BufferType type) {
    if (type == BufferType::PixelPack) {
        return GL_STREAM_READ;
    }
    switch (usage) {
        case BufferUsage::Static:  return GL_STATIC_DRAW;
        case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case BufferUsage::Stream:  return GL_STREAM_DRAW;
        default: return GL_STATIC_DRAW;
    }
}

}

GLBuffer::~GLBuffer() {
    destroy();
}

GLBuffer::GLBuffer(GLBuffer&& other) noexcept
    : RHIBuffer()
{
    m_handle = other.m_handle;
    m_type = other.m_type;
    m_usage = other.m_usage;
    m_size = other.m_size;
    m_valid = other.m_valid;

    other.m_handle = 0;
    other.m_valid = false;
}

GLBuffer& GLBuffer::operator=(GLBuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        m_handle = other.m_handle;
        m_type = other.m_type;
        m_usage = other.m_usage;
        m_size = other.m_size;
        m_valid = other.m_valid;

        other.m_handle = 0;
        other.m_valid = false;
    }
    return *this;
}

bool GLBuffer::init(const BufferDesc& desc) {
    if (m_handle != 0) {
        destroy();
    }

    m_type = desc.type;
    m_usage = desc.usage;
    m_size = desc.size;

    glGenBuffers(1, &m_handle);
    if (m_handle == 0) {
        return false;
    }

    GLenum target = buffer_type_to_gl(m_type);
    GLenum usage = buffer_usage_to_gl(m_usage, m_type);

    glBindBuffer(target, m_handle);
    glBufferData(target, static_cast<GLsizeiptr>(m_size), desc.initial_data, usage);
    glBindBuffer(target, 0);

    m_valid = true;
    return true;
}

void GLBuffer::destroy() {
    if (m_handle != 0) {
        glDeleteBuffers(1, &m_handle);
        m_handle = 0;
    }
    m_valid = false;
    m_size = 0;
}

void GLBuffer::update(size_t offset, size_t size, const void* data) {
    if (!m_valid || !data) return;

    GLenum target = buffer_type_to_gl(m_type);
    glBindBuffer(target, m_handle);
    glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
    glBindBuffer(target, 0);
}

bool GLBuffer::resize(size_t new_size, const void* new_data) {
    if (!m_valid) return false;

    GLenum target = buffer_type_to_gl(m_type);
    GLenum usage = buffer_usage_to_gl(m_usage, m_type);

    glBindBuffer(target, m_handle);
    glBufferData(target, static_cast<GLsizeiptr>(new_size), new_data, usage);
    glBindBuffer(target, 0);

    m_size = new_size;
    return true;
}

bool GLBuffer::readback(size_t offset, size_t size, void* dst) const {
    if (!m_valid || !dst) return false;
    if (offset + size > m_size) return false;

    GLenum target = buffer_type_to_gl(m_type);
    glBindBuffer(target, m_handle);

    void* mapped = glMapBufferRange(target, static_cast<GLintptr>(offset),
                                    static_cast<GLsizeiptr>(size), GL_MAP_READ_BIT);
    if (!mapped) {
        glBindBuffer(target, 0);
        return false;
    }

    std::memcpy(dst, mapped, size);
    glUnmapBuffer(target);
    glBindBuffer(target, 0);

    return true;
}

void GLBuffer::bind(uint32_t slot) {
    if (!m_valid) return;

    if (m_type == BufferType::Storage) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, m_handle);
    } else if (m_type == BufferType::Uniform) {
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, m_handle);
    } else {
        GLenum target = buffer_type_to_gl(m_type);
        glBindBuffer(target, m_handle);
    }
}

void* GLBuffer::map_read(size_t offset, size_t size) {
    if (!m_valid) return nullptr;
    if (offset + size > m_size) return nullptr;

    GLenum target = buffer_type_to_gl(m_type);
    glBindBuffer(target, m_handle);

    void* ptr = glMapBufferRange(target, static_cast<GLintptr>(offset),
                                 static_cast<GLsizeiptr>(size), GL_MAP_READ_BIT);
    if (!ptr) {
        glBindBuffer(target, 0);
    }
    return ptr;
}

void GLBuffer::unmap() {
    if (!m_valid) return;

    GLenum target = buffer_type_to_gl(m_type);
    glUnmapBuffer(target);
    glBindBuffer(target, 0);
}

}
