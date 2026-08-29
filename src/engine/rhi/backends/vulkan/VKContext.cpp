#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKContext.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKPipeline.h"
#include "VKShader.h"
#include "VKFramebuffer.h"
#include "VKCommandBuffer.h"
#include "VKDescriptorSet.h"
#include "VKSynchronization.h"
#include "engine/core/Log.h"

namespace engine::rhi {

void VKContext::begin_frame() {
    // Skip rendering when minimized (0x0 extent)
    VkExtent2D extent = m_device->swapchain_extent();
    if (extent.width == 0 || extent.height == 0) return;

    // Wait for this frame's fence (ensures the GPU finished the previous use of this frame slot)
    VkFence fence = m_device->in_flight_fence();
    VkResult fence_result = vkWaitForFences(m_device->device(), 1, &fence, VK_TRUE, 1000000000ULL);
    if (fence_result == VK_TIMEOUT) {
        ENGINE_ERR("[VKContext::begin_frame] GPU fence timeout (5s) — possible GPU hang!");
        return;
    }

    // Acquire swapchain image
    VkSemaphore img_sem = m_device->image_available_semaphore();
    VkResult result = vkAcquireNextImageKHR(
        m_device->device(),
        m_device->swapchain(),
        UINT64_MAX,
        img_sem,
        VK_NULL_HANDLE,
        &m_current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Acquire failed so the semaphore was NOT signaled — safe to recreate immediately.
        // The in-flight fence is still signaled from the previous submission, so the next
        // begin_frame() call will pass vkWaitForFences immediately and retry the acquire.
        // m_frame_begun stays false, causing end_frame() to no-op gracefully.
        m_device->recreate_swapchain(0, 0);
        return;
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        // Semaphore WAS signaled — must finish this frame, then recreate
        m_swapchain_needs_recreation = true;
    }

    vkResetFences(m_device->device(), 1, &fence);

    // Reset and begin the command buffer for this frame
    m_active_cmd = m_device->frame_command_buffer();
    vkResetCommandBuffer(m_active_cmd, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(m_active_cmd, &begin_info));

    // Process deferred deletions and reset per-frame descriptor pool
    m_device->flush_deletion_queue();
    m_device->reset_frame_descriptors();
    m_desc_state.clear();
    memset(m_bound_image_textures, 0, sizeof(m_bound_image_textures));

    m_frame_begun = true;
    m_in_render_pass = false;
    m_active_framebuffer = nullptr;
    m_initial_clear_done = false;
}

void VKContext::end_frame() {
    if (!m_frame_begun) return;

    // End any active render pass
    end_current_render_pass();

    VK_CHECK(vkEndCommandBuffer(m_active_cmd));

    // Submit
    VkSemaphore wait_sem = m_device->image_available_semaphore();
    VkSemaphore signal_sem = m_device->render_finished_semaphore();
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_sem;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_active_cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_sem;

    VK_CHECK(vkQueueSubmit(m_device->graphics_queue(), 1, &submit_info, m_device->in_flight_fence()));

    // Present
    VkSwapchainKHR swapchain = m_device->swapchain();
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &signal_sem;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &m_current_image_index;

    VkResult present_result = vkQueuePresentKHR(m_device->graphics_queue(), &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        m_swapchain_needs_recreation = true;
    }

    m_device->advance_frame();
    m_frame_begun = false;
    m_active_cmd = VK_NULL_HANDLE;

    // Deferred swapchain recreation (safe: frame is fully submitted and presented)
    if (m_swapchain_needs_recreation) {
        m_swapchain_needs_recreation = false;
        m_device->recreate_swapchain(0, 0);
    }
}

void VKContext::begin_swapchain_render_pass(float r, float g, float b, float a) {
    if (m_in_render_pass) return;

    VkClearValue clear_values[2] = {};
    clear_values[0].color = {{r, g, b, a}};
    clear_values[1].depthStencil = {m_clear_depth, static_cast<uint32_t>(m_clear_stencil)};

    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = m_device->swapchain_render_pass();
    rp_info.framebuffer = m_device->swapchain_framebuffer(m_current_image_index);
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = m_device->swapchain_extent();
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(m_active_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
    m_in_render_pass = true;
    m_initial_clear_done = true;
}

void VKContext::begin_swapchain_render_pass_load() {
    if (m_in_render_pass) return;

    // LOAD_OP_LOAD ignores clear values, but Vulkan still requires the array
    VkClearValue clear_values[2] = {};

    VkRenderPassBeginInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = m_device->swapchain_render_pass_load();
    rp_info.framebuffer = m_device->swapchain_framebuffer_load(m_current_image_index);
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = m_device->swapchain_extent();
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(m_active_cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
    m_in_render_pass = true;
}

void VKContext::end_current_render_pass() {
    if (m_in_render_pass) {
        vkCmdEndRenderPass(m_active_cmd);
        m_in_render_pass = false;

        // Update tracked layout on FBO textures to match the render pass finalLayout.
        // The render pass implicitly transitions attachments — we must keep our tracking in sync.
        if (m_active_framebuffer) {
            m_active_framebuffer->sync_attachment_layouts();
        }
    }
}

void VKContext::resume_render_pass() {
    if (m_in_render_pass) return;
    if (m_initial_clear_done) {
        // Already cleared once for this target — use LOAD_OP_LOAD to preserve content
        if (m_active_framebuffer) {
            m_active_framebuffer->begin_render_pass_load(m_active_cmd);
            m_in_render_pass = true;
        } else {
            begin_swapchain_render_pass_load();
        }
    } else {
        // First time — use CLEAR
        if (m_active_framebuffer) {
            m_active_framebuffer->begin_render_pass(m_active_cmd, m_clear_r, m_clear_g, m_clear_b, m_clear_a,
                                                     m_clear_depth, m_clear_stencil);
            m_in_render_pass = true;
            m_initial_clear_done = true;
        } else {
            begin_swapchain_render_pass(m_clear_r, m_clear_g, m_clear_b, m_clear_a);
        }
    }
}

void VKContext::set_viewport(int x, int y, int w, int h) {
    if (!m_active_cmd) return;
    VkViewport viewport = {};
    viewport.x = static_cast<float>(x);
    viewport.y = static_cast<float>(y);
    viewport.width = static_cast<float>(w);
    viewport.height = static_cast<float>(h);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_active_cmd, 0, 1, &viewport);

    // Vulkan requires scissor to be set when declared as dynamic state.
    // Default to matching the viewport so nothing is clipped unexpectedly.
    VkRect2D scissor = {};
    scissor.offset = {x, y};
    scissor.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    vkCmdSetScissor(m_active_cmd, 0, 1, &scissor);
}

void VKContext::set_scissor(int x, int y, int w, int h) {
    if (!m_active_cmd) return;
    VkRect2D scissor = {};
    scissor.offset = {x, y};
    scissor.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    vkCmdSetScissor(m_active_cmd, 0, 1, &scissor);
}

void VKContext::enable_scissor_test(bool /*enable*/) {
    // In Vulkan, scissor is always active (set to full framebuffer to "disable")
    // The actual scissor rect is set via set_scissor() or by default to the render area
}

void VKContext::clear(float r, float g, float b, float a) {
    m_clear_r = r;
    m_clear_g = g;
    m_clear_b = b;
    m_clear_a = a;

    if (m_in_render_pass) {
        // Render pass already started — use vkCmdClearAttachments for immediate clearing
        VkClearAttachment clear = {};
        clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear.colorAttachment = 0;
        clear.clearValue.color = {{r, g, b, a}};

        VkClearRect rect = {};
        rect.layerCount = 1;
        VkExtent2D extent = m_active_framebuffer
            ? VkExtent2D{static_cast<uint32_t>(m_active_framebuffer->width()),
                         static_cast<uint32_t>(m_active_framebuffer->height())}
            : m_device->swapchain_extent();
        rect.rect.extent = extent;

        vkCmdClearAttachments(m_active_cmd, 1, &clear, 1, &rect);
    }
    // Values are also stored so the next render pass start (if any) uses them.
}

void VKContext::clear_depth(float depth) {
    m_clear_depth = depth;

    if (m_in_render_pass) {
        VkClearAttachment clear = {};
        clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clear.clearValue.depthStencil.depth = depth;

        VkClearRect rect = {};
        rect.layerCount = 1;
        VkExtent2D extent = m_active_framebuffer
            ? VkExtent2D{static_cast<uint32_t>(m_active_framebuffer->width()),
                         static_cast<uint32_t>(m_active_framebuffer->height())}
            : m_device->swapchain_extent();
        rect.rect.extent = extent;

        vkCmdClearAttachments(m_active_cmd, 1, &clear, 1, &rect);
    }
}

void VKContext::clear_stencil(int stencil) {
    m_clear_stencil = stencil;

    if (m_in_render_pass) {
        VkClearAttachment clear = {};
        clear.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        clear.clearValue.depthStencil.stencil = static_cast<uint32_t>(stencil);

        VkClearRect rect = {};
        rect.layerCount = 1;
        VkExtent2D extent = m_active_framebuffer
            ? VkExtent2D{static_cast<uint32_t>(m_active_framebuffer->width()),
                         static_cast<uint32_t>(m_active_framebuffer->height())}
            : m_device->swapchain_extent();
        rect.rect.extent = extent;

        vkCmdClearAttachments(m_active_cmd, 1, &clear, 1, &rect);
    }
}

void VKContext::bind_pipeline(RHIPipeline* pipeline) {
    if (!m_active_cmd || !pipeline) return;

    m_current_pipeline = static_cast<VKPipeline*>(pipeline);

    // Recreate the VkPipeline if the shader was hot-reloaded since it was created
    m_current_pipeline->ensure_up_to_date();

    // Ensure render pass compatibility for graphics pipelines
    if (!pipeline->is_compute()) {
        VkRenderPass required_rp = m_active_framebuffer
            ? m_active_framebuffer->render_pass()
            : m_device->swapchain_render_pass();

        // Recreate pipeline if it was built against a different render pass
        if (m_current_pipeline->render_pass() != required_rp) {
            m_current_pipeline->recreate_for_render_pass(required_rp);
        }

        if (!m_in_render_pass) {
            resume_render_pass();
        }
    }

    // Mark descriptors dirty so they get re-flushed with the new pipeline's layout.
    // Don't clear the actual bindings — resources bound before bind_pipeline should persist.
    // Storage image tracking is pipeline-specific, so reset that.
    m_desc_state.dirty = true;
    memset(m_bound_image_textures, 0, sizeof(m_bound_image_textures));
    m_push_constants_dirty = true; // New pipeline needs its push constants uploaded

    VkPipelineBindPoint bind_point = pipeline->is_compute()
        ? VK_PIPELINE_BIND_POINT_COMPUTE
        : VK_PIPELINE_BIND_POINT_GRAPHICS;

    vkCmdBindPipeline(m_active_cmd, bind_point,
        static_cast<VkPipeline>(m_current_pipeline->native_handle()));
}

void VKContext::bind_framebuffer(RHIFramebuffer* fb) {
    if (!m_active_cmd) return;

    // End current render pass
    end_current_render_pass();

    // New target — reset clear tracking
    m_initial_clear_done = false;

    if (fb) {
        // Begin render pass for the custom framebuffer with the current clear color
        auto* vk_fb = static_cast<VKFramebuffer*>(fb);
        m_active_framebuffer = vk_fb;
        vk_fb->begin_render_pass(m_active_cmd, m_clear_r, m_clear_g, m_clear_b, m_clear_a,
                                  m_clear_depth, m_clear_stencil);
        m_in_render_pass = true;
        m_initial_clear_done = true;
    } else {
        // Bind back to swapchain
        m_active_framebuffer = nullptr;
        begin_swapchain_render_pass(m_clear_r, m_clear_g, m_clear_b, m_clear_a);
    }
}

void VKContext::bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding, size_t offset) {
    if (!m_active_cmd || !buffer) return;
    VkBuffer vk_buf = static_cast<VkBuffer>(buffer->native_handle());
    VkDeviceSize vk_offset = static_cast<VkDeviceSize>(offset);
    vkCmdBindVertexBuffers(m_active_cmd, binding, 1, &vk_buf, &vk_offset);
}

void VKContext::bind_index_buffer(RHIBuffer* buffer, uint32_t index_type) {
    if (!m_active_cmd || !buffer) return;
    m_index_type = (index_type == sizeof(uint16_t)) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    VkBuffer vk_buf = static_cast<VkBuffer>(buffer->native_handle());
    vkCmdBindIndexBuffer(m_active_cmd, vk_buf, 0, m_index_type);
}

void VKContext::bind_texture(const RHITexture* texture, uint32_t unit) {
    if (!m_active_cmd || !texture || unit >= VKDescriptorState::MAX_SLOTS) return;

    auto* vk_tex = static_cast<const VKTexture*>(texture);

    // Ensure texture is in the correct layout for sampling
    if (vk_tex->current_layout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        bool was_in_render_pass = m_in_render_pass;
        end_current_render_pass(); // barriers must be outside render pass
        vk_tex->transition_layout(m_active_cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // Restore the render pass that was interrupted
        if (was_in_render_pass) {
            resume_render_pass();
        }
    }

    // Defer to flush — store the texture for later descriptor write
    m_desc_state.samplers[unit] = vk_tex;
    m_desc_state.dirty = true;
}

void VKContext::bind_uniform_buffer(RHIBuffer* buffer, uint32_t slot) {
    if (!buffer || slot >= VKDescriptorState::MAX_SLOTS) return;
    m_desc_state.ubos[slot] = static_cast<VkBuffer>(buffer->native_handle());
    m_desc_state.ubo_sizes[slot] = buffer->size();
    m_desc_state.dirty = true;
}

void VKContext::bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) {
    if (!buffer || slot >= VKDescriptorState::MAX_SLOTS) return;
    m_desc_state.ssbos[slot] = static_cast<VkBuffer>(buffer->native_handle());
    m_desc_state.ssbo_sizes[slot] = buffer->size();
    m_desc_state.dirty = true;
}

void VKContext::bind_image(RHITexture* texture, uint32_t unit, ImageAccess /*access*/) {
    if (!texture || !m_active_cmd || unit >= VKDescriptorState::MAX_SLOTS) return;

    auto* vk_tex = static_cast<VKTexture*>(texture);

    // Transition to GENERAL layout for storage image access
    if (vk_tex->current_layout() != VK_IMAGE_LAYOUT_GENERAL) {
        bool was_in_render_pass = m_in_render_pass;
        end_current_render_pass(); // barriers must be outside render pass
        vk_tex->transition_layout(m_active_cmd, VK_IMAGE_LAYOUT_GENERAL);
        if (was_in_render_pass) {
            resume_render_pass();
        }
    }

    m_desc_state.images[unit] = vk_tex->image_view();
    m_bound_image_textures[unit] = vk_tex;
    m_desc_state.dirty = true;
}

void VKContext::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count) {
    if (!m_active_cmd) return;
    if (!m_in_render_pass) {
        begin_swapchain_render_pass(m_clear_r, m_clear_g, m_clear_b, m_clear_a);
    }
    flush_descriptors();
    flush_push_constants();
    vkCmdDraw(m_active_cmd, vertex_count, instance_count, first_vertex, 0);
}

void VKContext::draw_indexed(uint32_t index_count, uint32_t first_index,
                             int vertex_offset, uint32_t instance_count) {
    if (!m_active_cmd) return;
    if (!m_in_render_pass) {
        begin_swapchain_render_pass(m_clear_r, m_clear_g, m_clear_b, m_clear_a);
    }
    flush_descriptors();
    flush_push_constants();
    vkCmdDrawIndexed(m_active_cmd, index_count, instance_count, first_index, vertex_offset, 0);
}

void VKContext::dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
    if (!m_active_cmd) return;
    end_current_render_pass();
    flush_descriptors();
    flush_push_constants();
    vkCmdDispatch(m_active_cmd, groups_x, groups_y, groups_z);
}

void VKContext::copy_texture_to_buffer(
    const RHITexture* src_texture,
    int x, int y, int w, int h,
    RHIBuffer* dst_buffer,
    size_t dst_offset)
{
    if (!m_active_cmd || !src_texture || !dst_buffer) return;
    end_current_render_pass();

    auto* vk_tex = static_cast<const VKTexture*>(src_texture);
    auto* vk_buf = static_cast<VKBuffer*>(dst_buffer);

    // Transition to transfer src if not already
    VkImageLayout old_layout = vk_tex->current_layout();
    if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        vk_tex->transition_layout(m_active_cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

    VkBufferImageCopy region = {};
    region.bufferOffset = dst_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {x, y, 0};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};

    vkCmdCopyImageToBuffer(m_active_cmd,
        vk_tex->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        static_cast<VkBuffer>(vk_buf->native_handle()),
        1, &region);

    // Transition back to original layout
    if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && old_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
        vk_tex->transition_layout(m_active_cmd, old_layout);
    }
}

void VKContext::memory_barrier(BarrierFlags flags) {
    if (!m_active_cmd) return;

    auto info = build_barrier_from_flags(flags);

    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = info.src_access;
    barrier.dstAccessMask = info.dst_access;

    // Must be outside render pass for pipeline barriers
    end_current_render_pass();

    vkCmdPipelineBarrier(m_active_cmd,
        info.src_stage, info.dst_stage, 0,
        1, &barrier,
        0, nullptr,
        0, nullptr);

    // Only transition storage images back to SHADER_READ_ONLY_OPTIMAL when the
    // barrier explicitly signals that image/texture reads will follow.
    // This avoids breaking multi-pass compute chains where images stay in GENERAL.
    bool needs_image_readback = has_flag(flags, BarrierFlags::ImageAccess)
                             || has_flag(flags, BarrierFlags::TextureRead)
                             || has_flag(flags, BarrierFlags::Framebuffer);
    if (needs_image_readback) {
        for (uint32_t slot = 0; slot < VKDescriptorState::MAX_SLOTS; ++slot) {
            if (m_bound_image_textures[slot]) {
                m_bound_image_textures[slot]->transition_layout(m_active_cmd,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                m_bound_image_textures[slot] = nullptr;
            }
        }
    }
}

void VKContext::submit(RHICommandBuffer* cmd_buffer, RHIFence* signal_fence) {
    if (!cmd_buffer) return;

    auto* vk_cmd = static_cast<VKCommandBuffer*>(cmd_buffer);
    VkCommandBuffer cmd = vk_cmd->handle();

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkFence fence = VK_NULL_HANDLE;
    if (signal_fence) {
        fence = static_cast<VkFence>(static_cast<VKFence*>(signal_fence)->native_handle());
    }

    VK_CHECK(vkQueueSubmit(m_device->graphics_queue(), 1, &submit_info, fence));
}

void VKContext::bind_descriptor_set(RHIDescriptorSet* set, uint32_t index) {
    if (!m_active_cmd || !set || !m_current_pipeline) return;

    auto* vk_set = static_cast<VKDescriptorSet*>(set);
    VkDescriptorSet ds = vk_set->handle();

    VkPipelineBindPoint bind_point = m_current_pipeline->is_compute()
        ? VK_PIPELINE_BIND_POINT_COMPUTE
        : VK_PIPELINE_BIND_POINT_GRAPHICS;

    vkCmdBindDescriptorSets(m_active_cmd, bind_point,
        m_current_pipeline->pipeline_layout(), index, 1, &ds, 0, nullptr);
}

void VKContext::bind_compute_shader(VKShader* shader) {
    if (!m_active_cmd || !shader || !shader->is_compute()) return;

    // Clear stale state
    m_current_pipeline = nullptr;
    m_current_compute_shader = shader;
    m_desc_state.clear();
    m_push_constants_dirty = true;

    // Bind the cached compute pipeline
    vkCmdBindPipeline(m_active_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader->compute_pipeline());
}

bool VKContext::check_error(const char* /*context*/) {
    // Vulkan uses validation layers for error checking, not runtime queries
    return false;
}

void VKContext::flush_descriptors() {
    if (!m_desc_state.dirty || !m_active_cmd) return;

    VKShader* shader = nullptr;
    if (m_current_pipeline) {
        shader = static_cast<VKShader*>(m_current_pipeline->shader());
    } else if (m_current_compute_shader) {
        shader = m_current_compute_shader;
    }
    if (!shader) return;

    bool is_compute = m_current_compute_shader != nullptr;
    VkPipelineLayout layout = m_current_pipeline
        ? m_current_pipeline->pipeline_layout()
        : shader->pipeline_layout();

    m_desc_state.flush(m_device, m_active_cmd, shader, layout, is_compute);
}

void VKContext::flush_push_constants() {
    if (!m_active_cmd) return;

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

    // Only push if data changed or pipeline was switched
    if (!m_push_constants_dirty && !shader->push_constants_dirty()) return;

    // Size must match the range declared in the pipeline layout (4-byte aligned)
    uint32_t size = (shader->push_constant_size() + 3u) & ~3u;
    vkCmdPushConstants(m_active_cmd,
        layout,
        shader->push_constant_stages(),
        0,
        size,
        shader->push_constant_data());

    m_push_constants_dirty = false;
    shader->clear_push_dirty();
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
