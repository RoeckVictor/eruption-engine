#pragma once

#include "engine/rhi/RHITypes.h"
#include "engine/rhi/RHITexture.h"
#include <memory>

namespace engine::platform {

class Window;

// Abstract interface for ImGui platform/renderer backends.
// This abstraction allows the editor to use ImGui without directly depending
// on GLFW, OpenGL, or any other specific backend implementation.
class IImGuiBackend {
public:
    virtual ~IImGuiBackend() = default;

    virtual bool init(Window& window) = 0;
    virtual void shutdown() = 0;

    virtual void new_frame() = 0;
    virtual void render_draw_data() = 0;

    virtual void update_platform_windows() = 0;

    virtual bool supports_viewports() const = 0;

    // Convert an RHI texture to an ImTextureID suitable for ImGui::Image().
    // OpenGL: returns the GL texture handle directly.
    // Vulkan: creates/returns a VkDescriptorSet via ImGui_ImplVulkan_AddTexture().
    virtual void* register_texture(const rhi::RHITexture* texture) = 0;

    // Release a previously registered texture (Vulkan only — frees descriptor set).
    virtual void unregister_texture(void* imgui_texture_id) = 0;
};

std::unique_ptr<IImGuiBackend> create_imgui_backend(rhi::Backend backend = rhi::Backend::OpenGL);

// Global access to the active ImGui backend (set by the editor during init)
void set_current_imgui_backend(IImGuiBackend* backend);
IImGuiBackend* get_current_imgui_backend();

}
