#pragma once

#include "RHITypes.h"
#include <cstdint>

namespace engine::rhi {

class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIFramebuffer;
class RHIDescriptorSet;

// Abstract command buffer for deferred command recording
// Commands are recorded into the buffer and later submitted for execution
// This is the primary abstraction for Vulkan-style command recording
class RHICommandBuffer {
public:
    virtual ~RHICommandBuffer() = default;

    RHICommandBuffer(const RHICommandBuffer&) = delete;
    RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;

    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    virtual bool is_recording() const = 0;

    virtual void bind_pipeline(RHIPipeline* pipeline) = 0;
    virtual void bind_framebuffer(RHIFramebuffer* framebuffer) = 0;
    virtual void set_viewport(int x, int y, int width, int height) = 0;
    virtual void set_scissor(int x, int y, int width, int height) = 0;

    virtual void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0) = 0;
    virtual void bind_index_buffer(RHIBuffer* buffer, IndexType type) = 0;
    virtual void bind_texture(const RHITexture* texture, uint32_t unit) = 0;
    virtual void bind_descriptor_set(RHIDescriptorSet* set, uint32_t index = 0) = 0;
    virtual void bind_uniform_buffer(RHIBuffer* buffer, uint32_t binding) = 0;
    virtual void bind_storage_buffer(RHIBuffer* buffer, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) = 0;
    virtual void bind_storage_image(RHITexture* texture, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) = 0;

    virtual void clear_color(float r, float g, float b, float a) = 0;
    virtual void clear_depth(float depth = 1.0f) = 0;
    virtual void clear_stencil(int value = 0) = 0;

    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t first_index = 0, uint32_t instance_count = 1) = 0;

    virtual void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) = 0;

    virtual void memory_barrier(BarrierFlags flags) = 0;

protected:
    RHICommandBuffer() = default;
};

}
