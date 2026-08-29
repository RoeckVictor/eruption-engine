#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKCommandBuffer.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKPipeline.h"
#include "VKShader.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKFramebuffer.h"
#include "VKDescriptorSet.h"

#include <cstring>

namespace engine::rhi {

VKCommandBuffer::~VKCommandBuffer() {
    if (m_cmd && m_device) {
        vkFreeCommandBuffers(m_device->device(), m_device->command_pool(), 1, &m_cmd);
    }
}

bool VKCommandBuffer::init(VKDevice* device) {
    m_device = device;

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = device->command_pool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    return VK_CHECK(vkAllocateCommandBuffers(device->device(), &alloc_info, &m_cmd));
}

void VKCommandBuffer::begin() {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_cmd, &begin_info));
    m_recording = true;
    m_in_render_pass = false;
    m_current_pipeline = nullptr;
    m_current_compute_shader = nullptr;
    m_current_framebuffer = nullptr;
    m_desc_state.clear();
}

void VKCommandBuffer::end() {
    end_current_render_pass();
    VK_CHECK(vkEndCommandBuffer(m_cmd));
    m_recording = false;
}

void VKCommandBuffer::reset() {
    vkResetCommandBuffer(m_cmd, 0);
    m_recording = false;
    m_in_render_pass = false;
    m_current_pipeline = nullptr;
    m_current_compute_shader = nullptr;
}

void VKCommandBuffer::end_current_render_pass() {
    if (m_in_render_pass) {
        vkCmdEndRenderPass(m_cmd);
        m_in_render_pass = false;
    }
}

void VKCommandBuffer::bind_pipeline(RHIPipeline* pipeline) {
    if (!pipeline) return;
    auto* vk_pipeline = static_cast<VKPipeline*>(pipeline);
    vk_pipeline->ensure_up_to_date();

    m_current_pipeline = vk_pipeline;
    m_current_compute_shader = nullptr;

    // Mark descriptors dirty so they get re-flushed with the new pipeline's layout.
    // Don't clear the actual bindings — resources bound before bind_pipeline should persist.
    m_desc_state.dirty = true;

    VkPipelineBindPoint bp = pipeline->is_compute()
        ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(m_cmd, bp, static_cast<VkPipeline>(vk_pipeline->native_handle()));
}

void VKCommandBuffer::bind_framebuffer(RHIFramebuffer* framebuffer) {
    end_current_render_pass();

    if (framebuffer) {
        auto* vk_fb = static_cast<VKFramebuffer*>(framebuffer);
        m_current_framebuffer = vk_fb;
        vk_fb->begin_render_pass(m_cmd, 0.0f, 0.0f, 0.0f, 1.0f);
        m_in_render_pass = true;
    } else {
        m_current_framebuffer = nullptr;
    }
}

void VKCommandBuffer::set_viewport(int x, int y, int w, int h) {
    VkViewport vp = {};
    vp.x = static_cast<float>(x);
    vp.y = static_cast<float>(y);
    vp.width = static_cast<float>(w);
    vp.height = static_cast<float>(h);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(m_cmd, 0, 1, &vp);

    // Vulkan requires scissor to be set when declared as dynamic state.
    // Default to matching the viewport so nothing is clipped unexpectedly.
    VkRect2D scissor = {};
    scissor.offset = {x, y};
    scissor.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void VKCommandBuffer::set_scissor(int x, int y, int w, int h) {
    VkRect2D scissor = {};
    scissor.offset = {x, y};
    scissor.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void VKCommandBuffer::bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding) {
    if (!buffer) return;
    VkBuffer buf = static_cast<VkBuffer>(buffer->native_handle());
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmd, binding, 1, &buf, &offset);
}

void VKCommandBuffer::bind_index_buffer(RHIBuffer* buffer, IndexType type) {
    if (!buffer) return;
    VkBuffer buf = static_cast<VkBuffer>(buffer->native_handle());
    vkCmdBindIndexBuffer(m_cmd, buf, 0, to_vk_index_type(type));
}

void VKCommandBuffer::bind_texture(const RHITexture* texture, uint32_t unit) {
    if (!texture || unit >= VKDescriptorState::MAX_SLOTS) return;

    auto* vk_tex = static_cast<const VKTexture*>(texture);

    // Transition to correct layout immediately (must happen outside render pass)
    if (vk_tex->current_layout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        bool was_in_render_pass = m_in_render_pass;
        end_current_render_pass();
        vk_tex->transition_layout(m_cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // Restore the render pass that was interrupted
        if (was_in_render_pass && m_current_framebuffer) {
            m_current_framebuffer->begin_render_pass_load(m_cmd);
            m_in_render_pass = true;
        }
    }

    m_desc_state.samplers[unit] = vk_tex;
    m_desc_state.dirty = true;
}

void VKCommandBuffer::bind_descriptor_set(RHIDescriptorSet* set, uint32_t index) {
    if (!set || !m_current_pipeline) return;

    auto* vk_set = static_cast<VKDescriptorSet*>(set);
    VkDescriptorSet ds = vk_set->handle();

    VkPipelineBindPoint bp = m_current_pipeline->is_compute()
        ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindDescriptorSets(m_cmd, bp,
        m_current_pipeline->pipeline_layout(), index, 1, &ds, 0, nullptr);
}

void VKCommandBuffer::bind_uniform_buffer(RHIBuffer* buffer, uint32_t binding) {
    if (!buffer || binding >= VKDescriptorState::MAX_SLOTS) return;
    m_desc_state.ubos[binding] = static_cast<VkBuffer>(buffer->native_handle());
    m_desc_state.ubo_sizes[binding] = buffer->size();
    m_desc_state.dirty = true;
}

void VKCommandBuffer::bind_storage_buffer(RHIBuffer* buffer, uint32_t binding, BufferAccess /*access*/) {
    if (!buffer || binding >= VKDescriptorState::MAX_SLOTS) return;
    m_desc_state.ssbos[binding] = static_cast<VkBuffer>(buffer->native_handle());
    m_desc_state.ssbo_sizes[binding] = buffer->size();
    m_desc_state.dirty = true;
}

void VKCommandBuffer::bind_storage_image(RHITexture* texture, uint32_t binding, BufferAccess /*access*/) {
    if (!texture || binding >= VKDescriptorState::MAX_SLOTS) return;

    auto* vk_tex = static_cast<VKTexture*>(texture);

    // Transition to GENERAL layout immediately for storage image access
    if (vk_tex->current_layout() != VK_IMAGE_LAYOUT_GENERAL) {
        bool was_in_render_pass = m_in_render_pass;
        end_current_render_pass();
        vk_tex->transition_layout(m_cmd, VK_IMAGE_LAYOUT_GENERAL);
        if (was_in_render_pass && m_current_framebuffer) {
            m_current_framebuffer->begin_render_pass_load(m_cmd);
            m_in_render_pass = true;
        }
    }

    m_desc_state.images[binding] = vk_tex->image_view();
    m_desc_state.dirty = true;
}

void VKCommandBuffer::flush_descriptors() {
    if (!m_desc_state.dirty) return;

    VKShader* shader = nullptr;
    if (m_current_pipeline) {
        shader = static_cast<VKShader*>(m_current_pipeline->shader());
    } else if (m_current_compute_shader) {
        shader = m_current_compute_shader;
    }
    if (!shader) return;

    bool is_compute = (m_current_compute_shader != nullptr)
                   || (m_current_pipeline && m_current_pipeline->is_compute());
    VkPipelineLayout layout = m_current_pipeline
        ? m_current_pipeline->pipeline_layout()
        : shader->pipeline_layout();

    m_desc_state.flush(m_device, m_cmd, shader, layout, is_compute);
}

void VKCommandBuffer::flush_push_constants() {
    VKShader* shader = nullptr;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    if (m_current_pipeline) {
        shader = static_cast<VKShader*>(m_current_pipeline->shader());
        layout = m_current_pipeline->pipeline_layout();
    } else if (m_current_compute_shader) {
        shader = m_current_compute_shader;
        layout = shader->pipeline_layout();
    }

    if (!shader || !shader->has_push_constants() || layout == VK_NULL_HANDLE) return;
    if (!shader->push_constants_dirty()) return;

    uint32_t size = (shader->push_constant_size() + 3u) & ~3u;
    vkCmdPushConstants(m_cmd, layout, shader->push_constant_stages(), 0, size, shader->push_constant_data());
    shader->clear_push_dirty();
}

void VKCommandBuffer::clear_color(float r, float g, float b, float a) {
    if (!m_in_render_pass) return;

    VkClearAttachment clear = {};
    clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear.colorAttachment = 0;
    clear.clearValue.color = {{r, g, b, a}};

    VkClearRect rect = {};
    rect.layerCount = 1;
    rect.rect.extent = {UINT32_MAX, UINT32_MAX};

    vkCmdClearAttachments(m_cmd, 1, &clear, 1, &rect);
}

void VKCommandBuffer::clear_depth(float depth) {
    if (!m_in_render_pass) return;

    VkClearAttachment clear = {};
    clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear.clearValue.depthStencil.depth = depth;

    VkClearRect rect = {};
    rect.layerCount = 1;
    rect.rect.extent = {UINT32_MAX, UINT32_MAX};

    vkCmdClearAttachments(m_cmd, 1, &clear, 1, &rect);
}

void VKCommandBuffer::clear_stencil(int value) {
    if (!m_in_render_pass) return;

    VkClearAttachment clear = {};
    clear.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    clear.clearValue.depthStencil.stencil = static_cast<uint32_t>(value);

    VkClearRect rect = {};
    rect.layerCount = 1;
    rect.rect.extent = {UINT32_MAX, UINT32_MAX};

    vkCmdClearAttachments(m_cmd, 1, &clear, 1, &rect);
}

void VKCommandBuffer::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count) {
    flush_descriptors();
    flush_push_constants();
    vkCmdDraw(m_cmd, vertex_count, instance_count, first_vertex, 0);
}

void VKCommandBuffer::draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count) {
    flush_descriptors();
    flush_push_constants();
    vkCmdDrawIndexed(m_cmd, index_count, instance_count, first_index, 0, 0);
}

void VKCommandBuffer::dispatch_compute(uint32_t gx, uint32_t gy, uint32_t gz) {
    end_current_render_pass();
    flush_descriptors();
    flush_push_constants();
    vkCmdDispatch(m_cmd, gx, gy, gz);
}

void VKCommandBuffer::memory_barrier(BarrierFlags flags) {
    end_current_render_pass();

    auto info = build_barrier_from_flags(flags);

    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = info.src_access;
    barrier.dstAccessMask = info.dst_access;

    vkCmdPipelineBarrier(m_cmd,
        info.src_stage, info.dst_stage, 0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
