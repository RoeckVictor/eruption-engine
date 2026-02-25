#include "engine/graphics/PixelReadback.h"
#include "engine/graphics/Texture.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIBuffer.h"
#include "engine/rhi/RHIContext.h"
#include "engine/core/Log.h"
#include <cstring>

namespace engine::graphics {

PixelReadback::~PixelReadback() {
    shutdown();
}

void PixelReadback::init(int max_bytes) {
    shutdown();
    m_max_bytes = max_bytes;

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("PixelReadback::init failed: no RHI device available");
        return;
    }

    rhi::BufferDesc desc;
    desc.type = rhi::BufferType::PixelPack;
    desc.usage = rhi::BufferUsage::Stream;
    desc.size = static_cast<size_t>(max_bytes);
    desc.initial_data = nullptr;

    for (int i = 0; i < 2; i++) {
        m_buffers[i] = device->create_buffer(desc);
        if (!m_buffers[i]) {
            ENGINE_ERR("PixelReadback::init failed to create buffer %d", i);
        }
        m_has_data[i] = false;
        m_data_size[i] = 0;
    }

    m_write_idx = 0;
}

void PixelReadback::shutdown() {
    m_buffers[0].reset();
    m_buffers[1].reset();
    m_has_data[0] = m_has_data[1] = false;
    m_data_size[0] = m_data_size[1] = 0;
    m_max_bytes = 0;
}

void PixelReadback::begin(const Texture& tex, int x, int y, int w, int h) {
    int bytes = w * h * 4;
    if (bytes > m_max_bytes) {
        ENGINE_ERR("PixelReadback too large: %d > %d", bytes, m_max_bytes);
        return;
    }

    auto* context = rhi::get_current_context();
    if (!context || !m_buffers[m_write_idx]) {
        ENGINE_ERR("PixelReadback::begin failed: no context or buffer");
        return;
    }

    context->copy_texture_to_buffer(
        tex.rhi_texture(),
        x, y, w, h,
        m_buffers[m_write_idx].get(),
        0);

#ifndef NDEBUG
    context->check_error("PixelReadback::begin");
#endif

    m_has_data[m_write_idx] = true;
    m_data_size[m_write_idx] = bytes;

    // Swap: next begin() writes to the other buffer
    m_write_idx = 1 - m_write_idx;
}

void PixelReadback::begin_split(const Texture& tex,
                                 int x1, int y1, int w1, int h1,
                                 int x2, int y2, int w2, int h2) {
    int bytes1 = w1 * h1 * 4;
    int bytes2 = w2 * h2 * 4;
    int total = bytes1 + bytes2;
    if (total > m_max_bytes) {
        ENGINE_ERR("PixelReadback split too large: %d > %d", total, m_max_bytes);
        return;
    }

    auto* context = rhi::get_current_context();
    if (!context || !m_buffers[m_write_idx]) {
        ENGINE_ERR("PixelReadback::begin_split failed: no context or buffer");
        return;
    }

    // First region at offset 0
    context->copy_texture_to_buffer(
        tex.rhi_texture(),
        x1, y1, w1, h1,
        m_buffers[m_write_idx].get(),
        0);

    // Second region immediately after the first
    context->copy_texture_to_buffer(
        tex.rhi_texture(),
        x2, y2, w2, h2,
        m_buffers[m_write_idx].get(),
        static_cast<size_t>(bytes1));

    m_has_data[m_write_idx] = true;
    m_data_size[m_write_idx] = total;

    m_write_idx = 1 - m_write_idx;
}

bool PixelReadback::read(void* dst, int dst_size) {
    int read_idx = 1 - m_write_idx;
    if (!m_has_data[read_idx]) return false;
    if (!m_buffers[read_idx]) return false;

    int copy_size = m_data_size[read_idx];
    if (copy_size > dst_size) copy_size = dst_size;

    void* ptr = m_buffers[read_idx]->map_read(0, static_cast<size_t>(copy_size));
    if (ptr) {
        memcpy(dst, ptr, copy_size);
        m_buffers[read_idx]->unmap();
    } else {
        ENGINE_ERR("PixelReadback::read map failed (size=%d)", copy_size);
    }

    // Mark as consumed so stale data isn't returned on subsequent reads
    m_has_data[read_idx] = false;

    return ptr != nullptr;
}

} // namespace engine::graphics
