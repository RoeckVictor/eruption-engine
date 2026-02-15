#include "engine/graphics/PixelReadback.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/GLFormatInfo.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <cstring>

namespace engine::graphics {

using detail::GLFormatInfo;
using detail::gl_format;

PixelReadback::~PixelReadback() {
    shutdown();
}

void PixelReadback::init(int max_bytes) {
    shutdown();
    m_max_bytes = max_bytes;

    glGenBuffers(2, m_pbos);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, max_bytes, nullptr, GL_STREAM_READ);
        m_has_data[i] = false;
        m_data_size[i] = 0;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_write_idx = 0;
}

void PixelReadback::shutdown() {
    if (m_pbos[0] || m_pbos[1]) {
        glDeleteBuffers(2, m_pbos);
        m_pbos[0] = m_pbos[1] = 0;
    }
    m_has_data[0] = m_has_data[1] = false;
    m_data_size[0] = m_data_size[1] = 0;
    m_max_bytes = 0;
}

void PixelReadback::begin(const Texture& tex, int x, int y, int w, int h) {
    int bytes = w * h * 4;
    if (bytes > m_max_bytes) {
        ENGINE_ERR("PBO readback too large: %d > %d", bytes, m_max_bytes);
        return;
    }

    GLFormatInfo fi = gl_format(tex.format());

    // Bind the write PBO as pack target. glGetTextureSubImage will write
    // into the PBO instead of CPU memory (offset 0).
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[m_write_idx]);
    glGetTextureSubImage(tex.handle(), 0,
                         x, y, 0, w, h, 1,
                         fi.pixel_format, fi.pixel_type,
                         m_max_bytes, nullptr);
#ifndef NDEBUG
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_ERR("glGetTextureSubImage failed (GL error=0x%X)", err);
    }
#endif
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_has_data[m_write_idx] = true;
    m_data_size[m_write_idx] = bytes;

    // Swap: next begin() writes to the other PBO
    m_write_idx = 1 - m_write_idx;
}

void PixelReadback::begin_split(const Texture& tex,
                                 int x1, int y1, int w1, int h1,
                                 int x2, int y2, int w2, int h2) {
    int bytes1 = w1 * h1 * 4;
    int bytes2 = w2 * h2 * 4;
    int total = bytes1 + bytes2;
    if (total > m_max_bytes) {
        ENGINE_ERR("PBO split readback too large: %d > %d", total, m_max_bytes);
        return;
    }

    GLFormatInfo fi = gl_format(tex.format());

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[m_write_idx]);

    // First region at offset 0
    glGetTextureSubImage(tex.handle(), 0,
                         x1, y1, 0, w1, h1, 1,
                         fi.pixel_format, fi.pixel_type,
                         m_max_bytes, nullptr);

    // Second region immediately after the first in the PBO.
    // When a PBO is bound, the pointer argument is treated as a byte offset into the buffer.
    glGetTextureSubImage(tex.handle(), 0,
                         x2, y2, 0, w2, h2, 1,
                         fi.pixel_format, fi.pixel_type,
                         m_max_bytes - bytes1,
                         reinterpret_cast<void*>(static_cast<uintptr_t>(bytes1)));

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_has_data[m_write_idx] = true;
    m_data_size[m_write_idx] = total;

    m_write_idx = 1 - m_write_idx;
}

bool PixelReadback::read(void* dst, int dst_size) {
    int read_idx = 1 - m_write_idx;
    if (!m_has_data[read_idx]) return false;

    int copy_size = m_data_size[read_idx];
    if (copy_size > dst_size) copy_size = dst_size;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[read_idx]);
    void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, copy_size, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(dst, ptr, copy_size);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    } else {
        GLenum err = glGetError();
        ENGINE_ERR("PBO map failed (size=%d, GL error=0x%X)", copy_size, err);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // Mark as consumed so stale data isn't returned on subsequent reads
    m_has_data[read_idx] = false;

    return ptr != nullptr;
}

} // namespace engine::graphics
