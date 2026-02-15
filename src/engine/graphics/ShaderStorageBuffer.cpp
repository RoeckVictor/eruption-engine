#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <cstring>

namespace engine::graphics {

namespace {

GLenum gl_usage(BufferUsage usage) {
    switch (usage) {
        case BufferUsage::StaticDraw:  return GL_STATIC_DRAW;
        case BufferUsage::DynamicDraw: return GL_DYNAMIC_DRAW;
        case BufferUsage::StreamRead:  return GL_STREAM_READ;
    }
    return GL_STATIC_DRAW;
}

} // anonymous namespace

ShaderStorageBuffer::~ShaderStorageBuffer() {
    destroy();
}

ShaderStorageBuffer::ShaderStorageBuffer(ShaderStorageBuffer&& other) noexcept
    : m_handle(other.m_handle)
    , m_size(other.m_size)
{
    other.m_handle = 0;
    other.m_size = 0;
}

ShaderStorageBuffer& ShaderStorageBuffer::operator=(ShaderStorageBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_handle = other.m_handle;
        m_size = other.m_size;
        other.m_handle = 0;
        other.m_size = 0;
    }
    return *this;
}

bool ShaderStorageBuffer::create(size_t size_bytes, const void* data, BufferUsage usage) {
    destroy();

    glGenBuffers(1, &m_handle);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size_bytes, data, gl_usage(usage));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_ERR("GL error 0x%X creating SSBO (%zu bytes)", err, size_bytes);
        destroy();
        return false;
    }

    m_size = size_bytes;
    return true;
}

void ShaderStorageBuffer::destroy() {
    if (m_handle) {
        glDeleteBuffers(1, &m_handle);
        m_handle = 0;
        m_size = 0;
    }
}

void ShaderStorageBuffer::bind_base(int binding_point) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, m_handle);
}

void ShaderStorageBuffer::update(size_t offset, size_t size, const void* data) {
    if (!m_handle || !data || offset + size > m_size) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
#ifndef NDEBUG
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_ERR("GL error 0x%X in SSBO update (offset=%zu, size=%zu)", err, offset, size);
    }
#endif
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

bool ShaderStorageBuffer::readback(size_t offset, size_t size, void* dst) const {
    if (!m_handle || !dst || offset + size > m_size) return false;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(dst, ptr, size);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    } else {
        GLenum err = glGetError();
        ENGINE_ERR("SSBO map failed (offset=%zu, size=%zu, GL error=0x%X)", offset, size, err);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return ptr != nullptr;
}

} // namespace engine::graphics
