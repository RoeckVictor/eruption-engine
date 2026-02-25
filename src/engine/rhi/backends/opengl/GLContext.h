#pragma once

#include "engine/rhi/RHIContext.h"

namespace engine::rhi {

class GLPipeline;

/// OpenGL implementation of RHIContext
class GLContext : public RHIContext {
public:
    GLContext() = default;
    ~GLContext() override = default;

    // Frame management
    void begin_frame() override;
    void end_frame() override;

    // State management
    void set_viewport(int x, int y, int w, int h) override;
    void set_scissor(int x, int y, int w, int h) override;
    void clear(float r, float g, float b, float a = 1.0f) override;
    void clear_depth(float depth = 1.0f) override;
    void clear_stencil(int stencil = 0) override;

    // Resource binding
    void bind_pipeline(RHIPipeline* pipeline) override;
    void bind_framebuffer(RHIFramebuffer* fb) override;
    void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0, size_t offset = 0) override;
    void bind_index_buffer(RHIBuffer* buffer, uint32_t index_type = 4) override;
    void bind_texture(const RHITexture* texture, uint32_t unit) override;
    void bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) override;
    void bind_image(RHITexture* texture, uint32_t unit, ImageAccess access) override;

    // Draw commands
    void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) override;
    void draw_indexed(uint32_t index_count, uint32_t first_index = 0,
                      int vertex_offset = 0, uint32_t instance_count = 1) override;

    // Compute commands
    void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;

    // Data transfer
    void copy_texture_to_buffer(
        const RHITexture* src_texture,
        int x, int y, int w, int h,
        RHIBuffer* dst_buffer,
        size_t dst_offset = 0) override;

    // Synchronization
    void memory_barrier(BarrierFlags flags) override;

    // Debug
    bool check_error(const char* context = nullptr) override;

private:
    GLPipeline* m_current_pipeline = nullptr;
    uint32_t m_index_type = 4;  // Default to 32-bit indices
};

} // namespace engine::rhi
