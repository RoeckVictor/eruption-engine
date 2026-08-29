#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHICommandBuffer.h"
#include "VKCommon.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

class VKDevice;
class VKPipeline;
class VKShader;
class VKTexture;
class VKFramebuffer;

class VKCommandBuffer : public RHICommandBuffer {
public:
    VKCommandBuffer() = default;
    ~VKCommandBuffer() override;

    bool init(VKDevice* device);

    void begin() override;
    void end() override;
    void reset() override;
    bool is_recording() const override { return m_recording; }

    void bind_pipeline(RHIPipeline* pipeline) override;
    void bind_framebuffer(RHIFramebuffer* framebuffer) override;
    void set_viewport(int x, int y, int width, int height) override;
    void set_scissor(int x, int y, int width, int height) override;

    void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0) override;
    void bind_index_buffer(RHIBuffer* buffer, IndexType type) override;
    void bind_texture(const RHITexture* texture, uint32_t unit) override;
    void bind_descriptor_set(RHIDescriptorSet* set, uint32_t index = 0) override;
    void bind_uniform_buffer(RHIBuffer* buffer, uint32_t binding) override;
    void bind_storage_buffer(RHIBuffer* buffer, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) override;
    void bind_storage_image(RHITexture* texture, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) override;

    void clear_color(float r, float g, float b, float a) override;
    void clear_depth(float depth = 1.0f) override;
    void clear_stencil(int value = 0) override;

    void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) override;
    void draw_indexed(uint32_t index_count, uint32_t first_index = 0, uint32_t instance_count = 1) override;
    void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;
    void memory_barrier(BarrierFlags flags) override;

    VkCommandBuffer handle() const { return m_cmd; }

private:
    void end_current_render_pass();
    void flush_descriptors();
    void flush_push_constants();

    VKDevice* m_device = nullptr;
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VKPipeline* m_current_pipeline = nullptr;
    VKShader* m_current_compute_shader = nullptr;
    VKFramebuffer* m_current_framebuffer = nullptr; // Active FBO for render pass resumption
    bool m_recording = false;
    bool m_in_render_pass = false;

    // Shared descriptor binding state (flush logic shared with VKContext)
    VKDescriptorState m_desc_state;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
