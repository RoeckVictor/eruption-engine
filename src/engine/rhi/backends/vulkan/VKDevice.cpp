#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKDevice.h"
#include "VKCommon.h"
#include "VKBuffer.h"
#include "VKTexture.h"
#include "VKShader.h"
#include "VKPipeline.h"
#include "VKFramebuffer.h"
#include "VKCommandBuffer.h"
#include "VKDescriptorSet.h"
#include "VKPipelineCache.h"
#include "VKSynchronization.h"
#include "VKGPUProfiler.h"
#include "engine/core/Log.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

namespace engine::rhi {

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ENGINE_ERR("[Vulkan] %s", callback_data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        ENGINE_LOG_WARN("[Vulkan] %s", callback_data->pMessage);
    }
    return VK_FALSE;
}

VKDevice::~VKDevice() {
    shutdown();
}

bool VKDevice::init(void* window_handle) {
    if (!window_handle) {
        ENGINE_ERR("VKDevice::init: null window handle");
        return false;
    }

    // --- 1. Create Vulkan instance ---
    vkb::InstanceBuilder instance_builder;
    instance_builder
        .set_app_name("Eruption Engine")
        .set_engine_name("Eruption")
        .require_api_version(1, 2, 0);

#ifndef NDEBUG
    instance_builder
        .request_validation_layers(true)
        .set_debug_callback(vk_debug_callback);
#endif

    auto instance_result = instance_builder.build();
    if (!instance_result) {
        ENGINE_ERR("Failed to create Vulkan instance: %s", instance_result.error().message().c_str());
        return false;
    }
    auto vkb_instance = instance_result.value();
    m_instance = vkb_instance.instance;
    m_debug_messenger = vkb_instance.debug_messenger;

    // --- 2. Create surface ---
    m_window_handle = window_handle;
    auto* glfw_window = static_cast<GLFWwindow*>(window_handle);
    VkResult surface_result = glfwCreateWindowSurface(m_instance, glfw_window, nullptr, &m_surface);
    if (surface_result != VK_SUCCESS) {
        ENGINE_ERR("Failed to create Vulkan surface (VkResult %d)", static_cast<int>(surface_result));
        return false;
    }

    // --- 3. Select physical device ---
    vkb::PhysicalDeviceSelector phys_selector(vkb_instance);
    phys_selector
        .set_surface(m_surface)
        .set_minimum_version(1, 2)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

    auto phys_result = phys_selector.select();
    if (!phys_result) {
        ENGINE_ERR("Failed to select Vulkan physical device: %s", phys_result.error().message().c_str());
        return false;
    }
    auto vkb_physical = phys_result.value();
    m_physical_device = vkb_physical.physical_device;

    // --- 4. Create logical device ---
    vkb::DeviceBuilder device_builder(vkb_physical);
    auto device_result = device_builder.build();
    if (!device_result) {
        ENGINE_ERR("Failed to create Vulkan logical device: %s", device_result.error().message().c_str());
        return false;
    }
    auto vkb_device = device_result.value();
    m_device = vkb_device.device;

    // Get queues
    auto gq = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!gq) {
        ENGINE_ERR("Failed to get graphics queue");
        return false;
    }
    m_graphics_queue = gq.value();
    m_graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // Try to get a dedicated compute queue, fall back to graphics
    auto cq = vkb_device.get_queue(vkb::QueueType::compute);
    m_compute_queue = cq ? cq.value() : m_graphics_queue;

    // --- 5. Create VMA allocator ---
    VmaAllocatorCreateInfo alloc_info = {};
    alloc_info.physicalDevice = m_physical_device;
    alloc_info.device = m_device;
    alloc_info.instance = m_instance;
    alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
    if (vmaCreateAllocator(&alloc_info, &m_allocator) != VK_SUCCESS) {
        ENGINE_ERR("Failed to create VMA allocator");
        return false;
    }

    // --- 6. Create swapchain + render pass + framebuffers ---
    if (!create_swapchain_resources()) {
        return false;
    }

    // --- 7. Create command pools ---
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_graphics_queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (!VK_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool))) {
        return false;
    }

    // Dedicated pool for single-time transfer commands (separate from frame pool
    // to avoid concurrent pool access when uploads happen during frame recording).
    VkCommandPoolCreateInfo transfer_pool_info = {};
    transfer_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transfer_pool_info.queueFamilyIndex = m_graphics_queue_family;
    transfer_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (!VK_CHECK(vkCreateCommandPool(m_device, &transfer_pool_info, nullptr, &m_transfer_command_pool))) {
        return false;
    }

    // --- 8. Allocate per-frame command buffers ---
    VkCommandBufferAllocateInfo cmd_alloc_info = {};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.commandPool = m_command_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (!VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, m_command_buffers.data()))) {
        return false;
    }

    // --- 9. Create initial per-frame descriptor pools (one per frame slot) ---
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorPool initial_pool = VK_NULL_HANDLE;
        if (!create_frame_descriptor_pool(initial_pool)) return false;
        m_frame_pools[i].pools.push_back(initial_pool);
        m_frame_pools[i].active_index = 0;
    }

    // --- 10. Create per-frame sync objects ---
    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first wait doesn't hang

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (!VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &m_image_available_semaphores[i])) ||
            !VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &m_render_finished_semaphores[i])) ||
            !VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]))) {
            return false;
        }
    }

    // Transfer fence (starts unsignaled)
    VkFenceCreateInfo transfer_fence_info = {};
    transfer_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!VK_CHECK(vkCreateFence(m_device, &transfer_fence_info, nullptr, &m_transfer_fence))) {
        return false;
    }

    // --- 10. Query device properties ---
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical_device, &props);

    m_backend_name = std::string("Vulkan ") +
        std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
        std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." +
        std::to_string(VK_VERSION_PATCH(props.apiVersion));
    m_renderer_name = props.deviceName;

    switch (props.vendorID) {
        case 0x1002: m_vendor_name = "AMD"; break;
        case 0x10DE: m_vendor_name = "NVIDIA"; break;
        case 0x8086: m_vendor_name = "Intel"; break;
        default: m_vendor_name = "Unknown (0x" + std::to_string(props.vendorID) + ")"; break;
    }

    m_max_texture_size = static_cast<int>(props.limits.maxImageDimension2D);
    m_max_texture_units = static_cast<int>(props.limits.maxDescriptorSetSampledImages);
    m_max_storage_buffer_bindings = static_cast<int>(props.limits.maxDescriptorSetStorageBuffers);
    m_supports_compute = true; // Vulkan always supports compute

    m_max_compute_workgroup_size[0] = static_cast<int>(props.limits.maxComputeWorkGroupSize[0]);
    m_max_compute_workgroup_size[1] = static_cast<int>(props.limits.maxComputeWorkGroupSize[1]);
    m_max_compute_workgroup_size[2] = static_cast<int>(props.limits.maxComputeWorkGroupSize[2]);

    // Wire up context
    m_context.set_device(this);

    ENGINE_LOG("Vulkan initialized: %s (%s, %s)", m_backend_name.c_str(), m_renderer_name.c_str(), m_vendor_name.c_str());
    return true;
}

bool VKDevice::create_swapchain_resources() {
    // Create swapchain via vk-bootstrap
    vkb::SwapchainBuilder swapchain_builder(m_physical_device, m_device, m_surface);
    swapchain_builder
        .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // VSync
        .set_desired_extent(m_swapchain_extent.width, m_swapchain_extent.height);

    if (m_swapchain != VK_NULL_HANDLE) {
        swapchain_builder.set_old_swapchain(m_swapchain);
    }

    auto swap_result = swapchain_builder.build();
    if (!swap_result) {
        ENGINE_ERR("Failed to create Vulkan swapchain: %s", swap_result.error().message().c_str());
        return false;
    }
    auto vkb_swapchain = swap_result.value();

    // Destroy old swapchain if recreating
    if (m_swapchain != VK_NULL_HANDLE) {
        destroy_swapchain_resources();
    }

    m_swapchain = vkb_swapchain.swapchain;
    m_swapchain_format = vkb_swapchain.image_format;
    m_swapchain_extent = vkb_swapchain.extent;
    m_swapchain_images = vkb_swapchain.get_images().value();
    m_swapchain_image_views = vkb_swapchain.get_image_views().value();

    // --- Create shared depth buffer for the swapchain ---
    m_swapchain_depth_format = find_supported_depth_format();

    VkImageCreateInfo depth_img_info = {};
    depth_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_img_info.imageType = VK_IMAGE_TYPE_2D;
    depth_img_info.format = m_swapchain_depth_format;
    depth_img_info.extent = {m_swapchain_extent.width, m_swapchain_extent.height, 1};
    depth_img_info.mipLevels = 1;
    depth_img_info.arrayLayers = 1;
    depth_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_img_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depth_img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depth_alloc_info = {};
    depth_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (!VK_CHECK(vmaCreateImage(m_allocator, &depth_img_info, &depth_alloc_info,
                                 &m_swapchain_depth_image, &m_swapchain_depth_alloc, nullptr))) {
        return false;
    }

    VkImageViewCreateInfo depth_view_info = {};
    depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_info.image = m_swapchain_depth_image;
    depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_info.format = m_swapchain_depth_format;
    depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_info.subresourceRange.baseMipLevel = 0;
    depth_view_info.subresourceRange.levelCount = 1;
    depth_view_info.subresourceRange.baseArrayLayer = 0;
    depth_view_info.subresourceRange.layerCount = 1;

    // Include stencil aspect if the format has it
    if (m_swapchain_depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
        m_swapchain_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        depth_view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (!VK_CHECK(vkCreateImageView(m_device, &depth_view_info, nullptr, &m_swapchain_depth_view))) {
        return false;
    }

    // --- Create swapchain render pass (color + depth) ---
    VkAttachmentDescription attachments[2] = {};

    // Color attachment
    attachments[0].format = m_swapchain_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format = m_swapchain_depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref = {};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dependency;

    if (!VK_CHECK(vkCreateRenderPass(m_device, &rp_info, nullptr, &m_swapchain_render_pass))) {
        return false;
    }

    // --- Create continuation render pass (LOAD_OP_LOAD) for resuming after interruptions ---
    {
        VkAttachmentDescription load_attachments[2] = {};

        // Color attachment — LOAD instead of CLEAR, starts from COLOR_ATTACHMENT_OPTIMAL
        load_attachments[0].format = m_swapchain_format;
        load_attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        load_attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        load_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        load_attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        load_attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        load_attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        load_attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment — LOAD instead of CLEAR, starts from DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        load_attachments[1].format = m_swapchain_depth_format;
        load_attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        load_attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        load_attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        load_attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        load_attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        load_attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        load_attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference load_color_ref = {};
        load_color_ref.attachment = 0;
        load_color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference load_depth_ref = {};
        load_depth_ref.attachment = 1;
        load_depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription load_subpass = {};
        load_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        load_subpass.colorAttachmentCount = 1;
        load_subpass.pColorAttachments = &load_color_ref;
        load_subpass.pDepthStencilAttachment = &load_depth_ref;

        VkSubpassDependency load_dep = {};
        load_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        load_dep.dstSubpass = 0;
        load_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        load_dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_SHADER_WRITE_BIT;
        load_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        load_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                               | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo load_rp_info = {};
        load_rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        load_rp_info.attachmentCount = 2;
        load_rp_info.pAttachments = load_attachments;
        load_rp_info.subpassCount = 1;
        load_rp_info.pSubpasses = &load_subpass;
        load_rp_info.dependencyCount = 1;
        load_rp_info.pDependencies = &load_dep;

        if (!VK_CHECK(vkCreateRenderPass(m_device, &load_rp_info, nullptr, &m_swapchain_render_pass_load))) {
            return false;
        }
    }

    // --- Create framebuffers for each swapchain image (color + shared depth) ---
    m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
    m_swapchain_framebuffers_load.resize(m_swapchain_image_views.size());
    for (size_t i = 0; i < m_swapchain_image_views.size(); ++i) {
        VkImageView fb_views[] = {m_swapchain_image_views[i], m_swapchain_depth_view};

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = m_swapchain_render_pass;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = fb_views;
        fb_info.width = m_swapchain_extent.width;
        fb_info.height = m_swapchain_extent.height;
        fb_info.layers = 1;

        if (!VK_CHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_swapchain_framebuffers[i]))) {
            return false;
        }

        // Framebuffer for the LOAD render pass (same views, different render pass)
        fb_info.renderPass = m_swapchain_render_pass_load;
        if (!VK_CHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_swapchain_framebuffers_load[i]))) {
            return false;
        }
    }

    return true;
}

void VKDevice::destroy_swapchain_resources() {
    for (auto fb : m_swapchain_framebuffers) {
        if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_swapchain_framebuffers.clear();

    for (auto fb : m_swapchain_framebuffers_load) {
        if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_swapchain_framebuffers_load.clear();

    if (m_swapchain_render_pass) {
        vkDestroyRenderPass(m_device, m_swapchain_render_pass, nullptr);
        m_swapchain_render_pass = VK_NULL_HANDLE;
    }

    if (m_swapchain_render_pass_load) {
        vkDestroyRenderPass(m_device, m_swapchain_render_pass_load, nullptr);
        m_swapchain_render_pass_load = VK_NULL_HANDLE;
    }

    // Destroy shared swapchain depth buffer
    if (m_swapchain_depth_view) {
        vkDestroyImageView(m_device, m_swapchain_depth_view, nullptr);
        m_swapchain_depth_view = VK_NULL_HANDLE;
    }
    if (m_swapchain_depth_image && m_swapchain_depth_alloc) {
        vmaDestroyImage(m_allocator, m_swapchain_depth_image, m_swapchain_depth_alloc);
        m_swapchain_depth_image = VK_NULL_HANDLE;
        m_swapchain_depth_alloc = VK_NULL_HANDLE;
    }

    for (auto view : m_swapchain_image_views) {
        if (view) vkDestroyImageView(m_device, view, nullptr);
    }
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

bool VKDevice::recreate_swapchain(int width, int height) {
    // Query actual framebuffer size from GLFW if not provided
    if ((width <= 0 || height <= 0) && m_window_handle) {
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(m_window_handle), &width, &height);
    }
    if (width <= 0 || height <= 0) return false; // Minimized

    vkDeviceWaitIdle(m_device);
    m_swapchain_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    return create_swapchain_resources();
}

void VKDevice::shutdown() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
    }

    // Flush all pending deferred deletions now that the GPU is idle
    for (auto& entry : m_deletion_queue) {
        entry.deleter();
    }
    m_deletion_queue.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_image_available_semaphores[i]) vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
        if (m_render_finished_semaphores[i]) vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
        if (m_in_flight_fences[i]) vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
    }

    if (m_transfer_fence) {
        vkDestroyFence(m_device, m_transfer_fence, nullptr);
        m_transfer_fence = VK_NULL_HANDLE;
    }

    for (auto& fps : m_frame_pools) {
        for (auto pool : fps.pools) {
            if (pool) vkDestroyDescriptorPool(m_device, pool, nullptr);
        }
        fps.pools.clear();
    }

    if (m_transfer_command_pool) {
        vkDestroyCommandPool(m_device, m_transfer_command_pool, nullptr);
        m_transfer_command_pool = VK_NULL_HANDLE;
    }

    if (m_command_pool) {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_command_pool = VK_NULL_HANDLE;
    }

    destroy_swapchain_resources();

    if (m_allocator) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_debug_messenger) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(m_instance, m_debug_messenger, nullptr);
        m_debug_messenger = VK_NULL_HANDLE;
    }

    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VkCommandBuffer VKDevice::begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_transfer_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    return cmd;
}

void VKDevice::end_single_time_commands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    vkResetFences(m_device, 1, &m_transfer_fence);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_transfer_fence);
    VkResult wait_result = vkWaitForFences(m_device, 1, &m_transfer_fence, VK_TRUE, 1000000000ULL);
    if (wait_result == VK_TIMEOUT) {
        ENGINE_ERR("[VKDevice] Transfer fence timeout (5s) — possible GPU hang!");
    }

    vkFreeCommandBuffers(m_device, m_transfer_command_pool, 1, &cmd);
}

void VKDevice::upload_buffer_staged(VkBuffer dst, size_t dst_offset, size_t size, const void* data) {
    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc = {};
    staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VmaAllocationInfo staging_result = {};
    if (vmaCreateBuffer(m_allocator, &staging_info, &staging_alloc,
                        &staging_buffer, &staging_allocation, &staging_result) != VK_SUCCESS) {
        ENGINE_ERR("Failed to create staging buffer for upload");
        return;
    }

    memcpy(staging_result.pMappedData, data, size);
    vmaFlushAllocation(m_allocator, staging_allocation, 0, size);

    auto cmd = begin_single_time_commands();
    VkBufferCopy copy_region = {};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = dst_offset;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, staging_buffer, dst, 1, &copy_region);
    end_single_time_commands(cmd);

    vmaDestroyBuffer(m_allocator, staging_buffer, staging_allocation);
}

void VKDevice::readback_buffer_staged(VkBuffer src, size_t src_offset, size_t size, void* dst) {
    VkBufferCreateInfo staging_info = {};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo staging_alloc = {};
    staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    VmaAllocationInfo staging_result = {};
    if (vmaCreateBuffer(m_allocator, &staging_info, &staging_alloc,
                        &staging_buffer, &staging_allocation, &staging_result) != VK_SUCCESS) {
        ENGINE_ERR("Failed to create staging buffer for readback");
        return;
    }

    auto cmd = begin_single_time_commands();
    VkBufferCopy copy_region = {};
    copy_region.srcOffset = src_offset;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd, src, staging_buffer, 1, &copy_region);
    end_single_time_commands(cmd);

    vmaInvalidateAllocation(m_allocator, staging_allocation, 0, size);
    memcpy(dst, staging_result.pMappedData, size);

    vmaDestroyBuffer(m_allocator, staging_buffer, staging_allocation);
}

VkFormat VKDevice::find_supported_depth_format() const {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (VkFormat fmt : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physical_device, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return fmt;
        }
    }
    return VK_FORMAT_D32_SFLOAT; // fallback
}

bool VKDevice::create_frame_descriptor_pool(VkDescriptorPool& out_pool) {
    static constexpr uint32_t POOL_MAX_SETS = 1024;
    static constexpr uint32_t POOL_MAX_SAMPLERS = 1024;
    static constexpr uint32_t POOL_MAX_BUFFERS = 256;

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_MAX_SAMPLERS },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, POOL_MAX_BUFFERS },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, POOL_MAX_BUFFERS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, POOL_MAX_BUFFERS },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = 0; // No FREE_DESCRIPTOR_SET_BIT — we reset the entire pool each frame
    pool_info.maxSets = POOL_MAX_SETS;
    pool_info.poolSizeCount = 4;
    pool_info.pPoolSizes = pool_sizes;

    return VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &out_pool));
}

void VKDevice::reset_frame_descriptors() {
    // Only reset this frame slot's pools — the other slot's command buffer may still be in flight.
    auto& fps = m_frame_pools[m_current_frame];
    for (auto pool : fps.pools) {
        vkResetDescriptorPool(m_device, pool, 0);
    }
    fps.active_index = 0;
}

VkDescriptorSet VKDevice::allocate_frame_descriptor_set(VkDescriptorSetLayout layout) {
    auto& fps = m_frame_pools[m_current_frame];

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = fps.pools[fps.active_index];
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(m_device, &alloc_info, &set);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        // Current pool is exhausted — add a new pool (keep old ones alive for this frame)
        fps.active_index++;
        if (fps.active_index >= fps.pools.size()) {
            VkDescriptorPool new_pool = VK_NULL_HANDLE;
            if (!create_frame_descriptor_pool(new_pool)) return VK_NULL_HANDLE;
            fps.pools.push_back(new_pool);
        }

        alloc_info.descriptorPool = fps.pools[fps.active_index];
        result = vkAllocateDescriptorSets(m_device, &alloc_info, &set);
    }

    if (result != VK_SUCCESS) {
        ENGINE_ERR("Failed to allocate frame descriptor set (VkResult %d)", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    return set;
}

void VKDevice::defer_deletion(std::function<void()> fn) {
    // Wait MAX_FRAMES_IN_FLIGHT + 1 active frames before executing.
    // flush_deletion_queue() is only called from begin_frame() when a frame actually proceeds,
    // so minimized windows (where begin_frame early-returns) won't tick the countdown —
    // resources stay alive until the GPU is truly done with them.
    m_deletion_queue.push_back({MAX_FRAMES_IN_FLIGHT + 1, std::move(fn)});
}

void VKDevice::flush_deletion_queue() {
    // Partition: collect expired deleters, keep the rest.
    // Deleters may push new entries (e.g., VKTexture destructor from a VKFramebuffer deleter),
    // so we swap the queue out before invoking them.
    std::vector<std::function<void()>> expired;

    size_t write = 0;
    for (size_t read = 0; read < m_deletion_queue.size(); ++read) {
        if (--m_deletion_queue[read].frames_remaining == 0) {
            expired.push_back(std::move(m_deletion_queue[read].deleter));
        } else {
            if (write != read) {
                m_deletion_queue[write] = std::move(m_deletion_queue[read]);
            }
            ++write;
        }
    }
    m_deletion_queue.resize(write);

    // Execute expired deleters — they may push new entries into m_deletion_queue
    for (auto& fn : expired) {
        fn();
    }
}

void VKDevice::max_compute_workgroup_size(int& x, int& y, int& z) const {
    x = m_max_compute_workgroup_size[0];
    y = m_max_compute_workgroup_size[1];
    z = m_max_compute_workgroup_size[2];
}

// --- Factory methods ---

std::unique_ptr<RHIBuffer> VKDevice::create_buffer(const BufferDesc& desc) {
    auto buffer = std::make_unique<VKBuffer>();
    if (!buffer->init(this, desc)) return nullptr;
    return buffer;
}

std::unique_ptr<RHITexture> VKDevice::create_texture(const TextureDesc& desc) {
    auto texture = std::make_unique<VKTexture>();
    if (!texture->init(this, desc)) return nullptr;
    return texture;
}

std::unique_ptr<RHIShader> VKDevice::create_shader(const ShaderDesc& desc) {
    auto shader = std::make_unique<VKShader>();
    if (!shader->init(this, desc)) return nullptr;
    return shader;
}

std::unique_ptr<RHIShader> VKDevice::create_graphics_shader(const GraphicsShaderDesc& desc) {
    auto shader = std::make_unique<VKShader>();
    if (!shader->init_graphics(this, desc.vertex_path, desc.fragment_path)) return nullptr;
    return shader;
}

std::unique_ptr<RHIShader> VKDevice::create_compute_shader(const ComputeShaderDesc& desc) {
    auto shader = std::make_unique<VKShader>();
    if (!shader->init_compute(this, desc.compute_path)) return nullptr;
    return shader;
}

std::unique_ptr<RHIPipeline> VKDevice::create_pipeline(const PipelineDesc& desc) {
    auto pipeline = std::make_unique<VKPipeline>();
    if (!pipeline->init(this, desc)) return nullptr;
    return pipeline;
}

std::unique_ptr<RHIFramebuffer> VKDevice::create_framebuffer(const FramebufferDesc& desc) {
    auto fb = std::make_unique<VKFramebuffer>();
    if (!fb->init(this, desc)) return nullptr;
    return fb;
}

std::unique_ptr<RHIFramebuffer> VKDevice::create_simple_framebuffer(
    int width, int height, TextureFormat color_format, bool create_depth)
{
    auto fb = std::make_unique<VKFramebuffer>();
    if (!fb->init_simple(this, width, height, color_format, create_depth)) return nullptr;
    return fb;
}

std::unique_ptr<RHICommandBuffer> VKDevice::create_command_buffer() {
    auto cmd = std::make_unique<VKCommandBuffer>();
    if (!cmd->init(this)) return nullptr;
    return cmd;
}

std::unique_ptr<RHIDescriptorSetLayout> VKDevice::create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) {
    auto layout = std::make_unique<VKDescriptorSetLayout>();
    if (!layout->init(this, desc)) return nullptr;
    return layout;
}

std::unique_ptr<RHIDescriptorSet> VKDevice::create_descriptor_set(const RHIDescriptorSetLayout* layout) {
    auto set = std::make_unique<VKDescriptorSet>();
    if (!set->init(this, static_cast<const VKDescriptorSetLayout*>(layout))) return nullptr;
    return set;
}

std::unique_ptr<RHIPipelineCache> VKDevice::create_pipeline_cache(const PipelineCacheDesc& desc) {
    auto cache = std::make_unique<VKPipelineCache>();
    if (!cache->init(this, desc)) return nullptr;
    return cache;
}

std::unique_ptr<RHIFence> VKDevice::create_fence() {
    auto fence = std::make_unique<VKFence>();
    if (!fence->init(this)) return nullptr;
    return fence;
}

std::unique_ptr<RHIEvent> VKDevice::create_event() {
    auto event = std::make_unique<VKEvent>();
    if (!event->init(this)) return nullptr;
    return event;
}

std::unique_ptr<RHISemaphore> VKDevice::create_semaphore() {
    auto sem = std::make_unique<VKSemaphore>();
    if (!sem->init(this)) return nullptr;
    return sem;
}

std::unique_ptr<RHITimelineSemaphore> VKDevice::create_timeline_semaphore() {
    auto sem = std::make_unique<VKTimelineSemaphore>();
    if (!sem->init(this)) return nullptr;
    return sem;
}

std::unique_ptr<profiler::GPUProfiler> VKDevice::create_gpu_profiler() {
    auto profiler = std::make_unique<VKGPUProfiler>();
    if (!profiler->init(this)) return nullptr;
    return profiler;
}

std::unique_ptr<RHIDevice> create_vulkan_device(void* window_handle) {
    auto device = std::make_unique<VKDevice>();
    if (!device->init(window_handle)) {
        return nullptr;
    }
    return device;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
