#include "engine/graphics/RenderContext.h"
#include "engine/rhi/RHIContext.h"
#include "engine/core/Log.h"

namespace engine::graphics {

void RenderContext::clear(float r, float g, float b, float a) {
    if (m_rhi_context) {
        m_rhi_context->clear(r, g, b, a);
    }
}

void RenderContext::set_viewport(int x, int y, int w, int h) {
    if (m_rhi_context) {
        m_rhi_context->set_viewport(x, y, w, h);
    }
}

void RenderContext::memory_barrier(rhi::BarrierFlags flags) {
    if (m_rhi_context) {
        m_rhi_context->memory_barrier(flags);
    }
}

void RenderContext::memory_barrier_image_access() {
    memory_barrier(rhi::BarrierFlags::ImageAccess);
}

void RenderContext::memory_barrier_buffer() {
    memory_barrier(rhi::BarrierFlags::StorageBuffer);
}

void RenderContext::dispatch_compute(int groups_x, int groups_y, int groups_z, rhi::BarrierFlags barrier_flags) {
    if (m_rhi_context) {
        m_rhi_context->dispatch_compute(
            static_cast<uint32_t>(groups_x),
            static_cast<uint32_t>(groups_y),
            static_cast<uint32_t>(groups_z)
        );

        if (barrier_flags != rhi::BarrierFlags::None) {
            m_rhi_context->memory_barrier(barrier_flags);
        }
    }
}

void RenderContext::dispatch_compute(int groups_x, int groups_y, int groups_z) {
    dispatch_compute(groups_x, groups_y, groups_z, rhi::BarrierFlags::None);
}

bool RenderContext::check_error(const char* context) {
    if (m_rhi_context) {
        return m_rhi_context->check_error(context);
    }
    return false;
}

} // namespace engine::graphics
