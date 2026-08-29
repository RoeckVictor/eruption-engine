#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIDevice.h"
#include "VKContext.h"
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace engine::rhi {

class VKDevice : public RHIDevice {
public:
    VKDevice() = default;
    ~VKDevice() override;

    bool init(void* window_handle);
    void shutdown();

    // --- RHIDevice factory methods ---
    std::unique_ptr<RHIBuffer> create_buffer(const BufferDesc& desc) override;
    std::unique_ptr<RHITexture> create_texture(const TextureDesc& desc) override;
    std::unique_ptr<RHIShader> create_shader(const ShaderDesc& desc) override;
    std::unique_ptr<RHIShader> create_graphics_shader(const GraphicsShaderDesc& desc) override;
    std::unique_ptr<RHIShader> create_compute_shader(const ComputeShaderDesc& desc) override;
    std::unique_ptr<RHIPipeline> create_pipeline(const PipelineDesc& desc) override;
    std::unique_ptr<RHIFramebuffer> create_framebuffer(const FramebufferDesc& desc) override;
    std::unique_ptr<RHIFramebuffer> create_simple_framebuffer(
        int width, int height,
        TextureFormat color_format = TextureFormat::RGBA8,
        bool create_depth = true) override;

    std::unique_ptr<RHICommandBuffer> create_command_buffer() override;
    std::unique_ptr<RHIDescriptorSetLayout> create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) override;
    std::unique_ptr<RHIDescriptorSet> create_descriptor_set(const RHIDescriptorSetLayout* layout) override;
    std::unique_ptr<RHIPipelineCache> create_pipeline_cache(const PipelineCacheDesc& desc) override;
    std::unique_ptr<RHIFence> create_fence() override;
    std::unique_ptr<RHIEvent> create_event() override;
    std::unique_ptr<RHISemaphore> create_semaphore() override;
    std::unique_ptr<RHITimelineSemaphore> create_timeline_semaphore() override;

    RHIContext* context() override { return &m_context; }

    std::unique_ptr<profiler::GPUProfiler> create_gpu_profiler() override;

    Backend backend() const override { return Backend::Vulkan; }
    const char* backend_name() const override { return m_backend_name.c_str(); }
    const char* renderer_name() const override { return m_renderer_name.c_str(); }
    const char* vendor_name() const override { return m_vendor_name.c_str(); }

    bool supports_compute() const override { return m_supports_compute; }
    int max_texture_size() const override { return m_max_texture_size; }
    int max_texture_units() const override { return m_max_texture_units; }
    int max_storage_buffer_bindings() const override { return m_max_storage_buffer_bindings; }
    void max_compute_workgroup_size(int& x, int& y, int& z) const override;
    bool uv_origin_top_left() const override { return true; } // Vulkan uses top-left UV origin

    // --- Vulkan-specific accessors (used by VK resource classes) ---
    VkInstance instance() const { return m_instance; }
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physical_device() const { return m_physical_device; }
    VmaAllocator allocator() const { return m_allocator; }
    VkQueue graphics_queue() const { return m_graphics_queue; }
    uint32_t graphics_queue_family() const { return m_graphics_queue_family; }
    VkCommandPool command_pool() const { return m_command_pool; }
    VkFormat swapchain_format() const { return m_swapchain_format; }
    VkExtent2D swapchain_extent() const { return m_swapchain_extent; }
    const std::vector<VkImageView>& swapchain_image_views() const { return m_swapchain_image_views; }
    const std::vector<VkImage>& swapchain_images() const { return m_swapchain_images; }

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t current_frame() const { return m_current_frame; }
    VkSemaphore image_available_semaphore() const { return m_image_available_semaphores[m_current_frame]; }
    VkSemaphore render_finished_semaphore() const { return m_render_finished_semaphores[m_current_frame]; }
    VkFence in_flight_fence() const { return m_in_flight_fences[m_current_frame]; }
    void advance_frame() { m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT; }
    VkCommandBuffer frame_command_buffer() const { return m_command_buffers[m_current_frame]; }
    VkSwapchainKHR swapchain() const { return m_swapchain; }

    // Execute a short-lived command buffer (for transfers, layout transitions, etc.)
    // Uses a dedicated fence instead of vkQueueWaitIdle for better GPU utilization.
    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer cmd);

    // Upload data to a GPU-only buffer via a temporary staging buffer.
    // Handles staging allocation, copy, and cleanup.
    void upload_buffer_staged(VkBuffer dst, size_t dst_offset, size_t size, const void* data);

    // Readback data from a GPU buffer to CPU via staging.
    void readback_buffer_staged(VkBuffer src, size_t src_offset, size_t size, void* dst);

    // Swapchain render pass (for rendering to the swapchain images)
    VkRenderPass swapchain_render_pass() const { return m_swapchain_render_pass; }
    // Continuation render pass: same attachments but LOAD_OP_LOAD instead of CLEAR.
    // Used when resuming a render pass after an interruption (barrier, layout transition).
    VkRenderPass swapchain_render_pass_load() const { return m_swapchain_render_pass_load; }
    VkFramebuffer swapchain_framebuffer(uint32_t image_index) const { return m_swapchain_framebuffers[image_index]; }
    VkFramebuffer swapchain_framebuffer_load(uint32_t image_index) const { return m_swapchain_framebuffers_load[image_index]; }

    bool recreate_swapchain(int width, int height);

    // Find a supported depth format (prefers D32F_S8, falls back to D32F, then D16)
    VkFormat find_supported_depth_format() const;

    // Per-frame descriptor pool for transient descriptor sets (e.g., texture bindings)
    void reset_frame_descriptors();
    VkDescriptorSet allocate_frame_descriptor_set(VkDescriptorSetLayout layout);

    // Deferred deletion queue: schedule a cleanup lambda to run after all in-flight
    // frames have finished using it (MAX_FRAMES_IN_FLIGHT frames from now).
    void defer_deletion(std::function<void()> fn);
    void flush_deletion_queue();

private:
    bool create_swapchain_resources();
    void destroy_swapchain_resources();
    bool create_frame_descriptor_pool(VkDescriptorPool& out_pool);

    VKContext m_context;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family = 0;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    VkFormat m_swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchain_extent = {0, 0};

    // Swapchain render pass and framebuffers (includes a shared depth buffer)
    VkRenderPass m_swapchain_render_pass = VK_NULL_HANDLE;
    VkRenderPass m_swapchain_render_pass_load = VK_NULL_HANDLE; // LOAD_OP_LOAD variant for resumption
    std::vector<VkFramebuffer> m_swapchain_framebuffers;
    std::vector<VkFramebuffer> m_swapchain_framebuffers_load; // Framebuffers for load render pass
    VkImage m_swapchain_depth_image = VK_NULL_HANDLE;
    VkImageView m_swapchain_depth_view = VK_NULL_HANDLE;
    VmaAllocation m_swapchain_depth_alloc = VK_NULL_HANDLE;
    VkFormat m_swapchain_depth_format = VK_FORMAT_UNDEFINED;

    // Per-frame synchronization
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_image_available_semaphores = {};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_render_finished_semaphores = {};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences = {};
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_command_buffers = {};
    uint32_t m_current_frame = 0;

    void* m_window_handle = nullptr; // GLFWwindow* for framebuffer size queries

    // Dedicated command pool and fence for single-time transfer commands.
    // Separate from m_command_pool so transfers can be submitted while frame
    // command buffers are recording without violating Vulkan's external
    // synchronization requirement on command pools.
    VkCommandPool m_transfer_command_pool = VK_NULL_HANDLE;
    VkFence m_transfer_fence = VK_NULL_HANDLE;

    // Per-frame-slot descriptor pools — each frame slot has its own pools to avoid
    // resetting pools whose descriptor sets are still referenced by an in-flight command buffer.
    struct FramePoolSet {
        std::vector<VkDescriptorPool> pools;
        uint32_t active_index = 0;
    };
    std::array<FramePoolSet, MAX_FRAMES_IN_FLIGHT> m_frame_pools;

    // Deferred deletion queue: each entry has a frame countdown and a cleanup lambda
    struct DeletionEntry {
        uint32_t frames_remaining;
        std::function<void()> deleter;
    };
    std::vector<DeletionEntry> m_deletion_queue;

    // Device info
    std::string m_backend_name;
    std::string m_renderer_name;
    std::string m_vendor_name;
    bool m_supports_compute = false;
    int m_max_texture_size = 0;
    int m_max_texture_units = 0;
    int m_max_storage_buffer_bindings = 0;
    int m_max_compute_workgroup_size[3] = {0, 0, 0};
};

std::unique_ptr<RHIDevice> create_vulkan_device(void* window_handle);

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
