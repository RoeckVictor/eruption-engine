#pragma once

#include "engine/rhi/RHITypes.h"

namespace engine::rhi {
class RHIContext;
}

namespace engine::graphics {

/// Render context wrapper that delegates to RHI.
/// Provides a simplified interface for common rendering operations.
class RenderContext {
public:
    void clear(float r, float g, float b, float a = 1.0f);
    void set_viewport(int x, int y, int w, int h);
    void memory_barrier(rhi::BarrierFlags flags);
    void memory_barrier_image_access();
    void memory_barrier_buffer();

    /// Dispatch compute shader with barrier flags
    void dispatch_compute(int groups_x, int groups_y, int groups_z, rhi::BarrierFlags barrier_flags);

    /// Dispatch compute shader with no barrier
    void dispatch_compute(int groups_x, int groups_y, int groups_z);

    bool check_error(const char* context = nullptr);

    /// Set the RHI context (called by Engine after RHI init)
    void set_rhi_context(rhi::RHIContext* ctx) { m_rhi_context = ctx; }

    /// Check if RHI context is available
    bool has_rhi_context() const { return m_rhi_context != nullptr; }

private:
    rhi::RHIContext* m_rhi_context = nullptr;
};

} // namespace engine::graphics
