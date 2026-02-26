#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Forward declarations
class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIFramebuffer;

// Abstract rendering context for command submission
// Manages state and issues draw/dispatch commands
class RHIContext {
public:
    virtual ~RHIContext() = default;

    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;

    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;

    virtual void set_viewport(int x, int y, int w, int h) = 0;
    virtual void set_scissor(int x, int y, int w, int h) = 0;
    virtual void clear(float r, float g, float b, float a = 1.0f) = 0;
    virtual void clear_depth(float depth = 1.0f) = 0;
    virtual void clear_stencil(int stencil = 0) = 0;

    virtual void bind_pipeline(RHIPipeline* pipeline) = 0;
    virtual void bind_framebuffer(RHIFramebuffer* fb) = 0;
    virtual void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0, size_t offset = 0) = 0;
    virtual void bind_index_buffer(RHIBuffer* buffer, uint32_t index_type = 4) = 0;
    virtual void bind_texture(const RHITexture* texture, uint32_t unit) = 0;
    virtual void bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) = 0;
    virtual void bind_image(RHITexture* texture, uint32_t unit, ImageAccess access) = 0;

    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t first_index = 0,
                              int vertex_offset = 0, uint32_t instance_count = 1) = 0;

    virtual void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) = 0;
    virtual void copy_texture_to_buffer(
        const RHITexture* src_texture,
        int x, int y, int w, int h,
        RHIBuffer* dst_buffer,
        size_t dst_offset = 0) = 0;

    virtual void memory_barrier(BarrierFlags flags) = 0;

    virtual bool check_error(const char* context = nullptr) = 0;

protected:
    RHIContext() = default;
};

}
