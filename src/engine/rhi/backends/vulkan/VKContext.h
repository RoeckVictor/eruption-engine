#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIContext.h"
#include "VKCommon.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

class VKDevice;
class VKFramebuffer;
class VKPipeline;
class VKShader;
class VKTexture;

class VKContext : public RHIContext {
public:
    VKContext() = default;
    ~VKContext() override = default;

    void set_device(VKDevice* device) { m_device = device; }

    void begin_frame() override;
    void end_frame() override;

    void set_viewport(int x, int y, int w, int h) override;
    void set_scissor(int x, int y, int w, int h) override;
    void enable_scissor_test(bool enable) override;
    void clear(float r, float g, float b, float a = 1.0f) override;
    void clear_depth(float depth = 1.0f) override;
    void clear_stencil(int stencil = 0) override;

    void bind_pipeline(RHIPipeline* pipeline) override;
    void bind_framebuffer(RHIFramebuffer* fb) override;
    void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0, size_t offset = 0) override;
    void bind_index_buffer(RHIBuffer* buffer, uint32_t index_type = 4) override;
    void bind_texture(const RHITexture* texture, uint32_t unit) override;
    void bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) override;
    void bind_image(RHITexture* texture, uint32_t unit, ImageAccess access) override;

    void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) override;
    void draw_indexed(uint32_t index_count, uint32_t first_index = 0,
                      int vertex_offset = 0, uint32_t instance_count = 1) override;

    void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;
    void copy_texture_to_buffer(
        const RHITexture* src_texture,
        int x, int y, int w, int h,
        RHIBuffer* dst_buffer,
        size_t dst_offset = 0) override;

    void memory_barrier(BarrierFlags flags) override;

    void submit(RHICommandBuffer* cmd_buffer, RHIFence* signal_fence = nullptr) override;
    void bind_descriptor_set(RHIDescriptorSet* set, uint32_t index = 0) override;

    bool check_error(const char* context = nullptr) override;

    // Access the active command buffer (for VK resource classes that need to record commands)
    VkCommandBuffer active_command_buffer() const { return m_active_cmd; }
    bool in_render_pass() const { return m_in_render_pass; }
    void end_current_render_pass();
    void begin_swapchain_render_pass(float r, float g, float b, float a);
    void begin_swapchain_render_pass_load();

    // Bind a uniform buffer to a slot (deferred until flush_descriptors)
    void bind_uniform_buffer(RHIBuffer* buffer, uint32_t slot);

    // Bind a compute shader's cached pipeline (called from VKShader::bind())
    void bind_compute_shader(class VKShader* shader);

private:
    void flush_push_constants();
    void flush_descriptors();
    void resume_render_pass();

    VKDevice* m_device = nullptr;
    VkCommandBuffer m_active_cmd = VK_NULL_HANDLE;
    uint32_t m_current_image_index = 0;
    bool m_in_render_pass = false;
    bool m_frame_begun = false;

    VKPipeline* m_current_pipeline = nullptr;
    VKShader* m_current_compute_shader = nullptr; // For compute dispatches without explicit pipeline
    VkIndexType m_index_type = VK_INDEX_TYPE_UINT32;

    // Track active framebuffer so we can restore the correct render pass after interruptions
    VKFramebuffer* m_active_framebuffer = nullptr; // null = swapchain

    // Deferred clear values (set by clear(), applied when render pass begins)
    float m_clear_r = 0.0f, m_clear_g = 0.0f, m_clear_b = 0.0f, m_clear_a = 1.0f;
    float m_clear_depth = 1.0f;
    int m_clear_stencil = 0;
    bool m_swapchain_needs_recreation = false; // Deferred recreation (e.g., after SUBOPTIMAL)
    // True once the initial CLEAR render pass has been used for the current target.
    // After this, resume_render_pass uses LOAD_OP_LOAD to preserve content.
    bool m_initial_clear_done = false;

    // Shared descriptor binding state (flush logic shared with VKCommandBuffer)
    VKDescriptorState m_desc_state;
    bool m_push_constants_dirty = false;

    // Context-specific: tracks storage image textures for post-dispatch layout transitions.
    // Not part of VKDescriptorState because VKCommandBuffer doesn't need this.
    VKTexture* m_bound_image_textures[VKDescriptorState::MAX_SLOTS] = {};
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
