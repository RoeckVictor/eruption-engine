#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/platform/IImGuiBackend.h"
#include "engine/platform/Window.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/backends/vulkan/VKDevice.h"
#include "engine/rhi/backends/vulkan/VKContext.h"
#include "engine/rhi/backends/vulkan/VKTexture.h"
#include "engine/core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace engine::platform {

class ImGuiBackendVulkan : public IImGuiBackend {
public:
    ~ImGuiBackendVulkan() override {
        if (m_initialized) {
            shutdown();
        }
    }

    bool init(Window& window) override {
        GLFWwindow* glfw_window = window.glfw_handle();
        if (!glfw_window) {
            ENGINE_ERR("ImGuiBackendVulkan: Window has no GLFW handle");
            return false;
        }

        // Get the VKDevice from the global RHI device
        auto* rhi_device = rhi::get_current_device();
        if (!rhi_device || rhi_device->backend() != rhi::Backend::Vulkan) {
            ENGINE_ERR("ImGuiBackendVulkan: No Vulkan RHI device available");
            return false;
        }
        m_vk_device = static_cast<rhi::VKDevice*>(rhi_device);

        // Initialize GLFW backend for Vulkan
        if (!ImGui_ImplGlfw_InitForVulkan(glfw_window, true)) {
            ENGINE_ERR("ImGuiBackendVulkan: Failed to initialize GLFW backend");
            return false;
        }

        // Fill Vulkan init info
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_vk_device->instance();
        init_info.PhysicalDevice = m_vk_device->physical_device();
        init_info.Device = m_vk_device->device();
        init_info.QueueFamily = m_vk_device->graphics_queue_family();
        init_info.Queue = m_vk_device->graphics_queue();
        init_info.RenderPass = m_vk_device->swapchain_render_pass();
        init_info.MinImageCount = rhi::VKDevice::MAX_FRAMES_IN_FLIGHT;
        init_info.ImageCount = static_cast<uint32_t>(m_vk_device->swapchain_image_views().size());
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.DescriptorPoolSize = 100; // Let ImGui create its own descriptor pool

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            ENGINE_ERR("ImGuiBackendVulkan: Failed to initialize Vulkan backend");
            ImGui_ImplGlfw_Shutdown();
            return false;
        }

        // Upload font textures
        ImGui_ImplVulkan_CreateFontsTexture();

        m_initialized = true;
        ENGINE_LOG("ImGuiBackendVulkan: Initialized");
        return true;
    }

    void shutdown() override {
        if (m_initialized) {
            if (m_vk_device) {
                vkDeviceWaitIdle(m_vk_device->device());
            }
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            m_initialized = false;
        }
    }

    void new_frame() override {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void render_draw_data() override {
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (!draw_data) return;

        // Get the active command buffer from the VKContext
        auto* ctx = static_cast<rhi::VKContext*>(m_vk_device->context());
        VkCommandBuffer cmd = ctx->active_command_buffer();
        if (!cmd) return;

        // Ensure we're in a render pass (should already be from engine's clear())
        if (!ctx->in_render_pass()) {
            ctx->begin_swapchain_render_pass(0.0f, 0.0f, 0.0f, 1.0f);
        }

        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    }

    void update_platform_windows() override {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    bool supports_viewports() const override {
        return true;
    }

    void* register_texture(const rhi::RHITexture* texture) override {
        if (!texture) return nullptr;

        auto* vk_tex = static_cast<const rhi::VKTexture*>(texture);
        if (!vk_tex->image_view() || !vk_tex->sampler()) return nullptr;

        // Create a VkDescriptorSet that ImGui can use as an ImTextureID
        VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
            vk_tex->sampler(),
            vk_tex->image_view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return static_cast<void*>(ds);
    }

    void unregister_texture(void* imgui_texture_id) override {
        if (!imgui_texture_id || !m_vk_device) return;
        VkDescriptorSet ds = static_cast<VkDescriptorSet>(imgui_texture_id);
        m_vk_device->defer_deletion([ds]() {
            ImGui_ImplVulkan_RemoveTexture(ds);
        });
    }

private:
    rhi::VKDevice* m_vk_device = nullptr;
    bool m_initialized = false;
};

// Factory function — called from ImGuiBackendGLFW.cpp
std::unique_ptr<IImGuiBackend> create_imgui_backend_vulkan() {
    return std::make_unique<ImGuiBackendVulkan>();
}

} // namespace engine::platform

#endif // ERUPTION_VULKAN_SUPPORT
