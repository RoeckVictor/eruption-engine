#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIFramebuffer.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace engine::rhi {

class VKDevice;
class VKTexture;

class VKFramebuffer : public RHIFramebuffer {
public:
    VKFramebuffer() = default;
    ~VKFramebuffer() override;

    bool init(VKDevice* device, const FramebufferDesc& desc);
    bool init_simple(VKDevice* device, int width, int height,
                     TextureFormat color_format, bool create_depth);

    void bind() override;
    void unbind() override;
    bool resize(int width, int height) override;

    RHITexture* color_attachment(uint32_t index) override;
    RHITexture* depth_stencil_attachment() override;
    void* native_handle() const override { return m_framebuffer; }

    VkRenderPass render_pass() const { return m_render_pass; }
    VkRenderPass render_pass_load() const { return m_render_pass_load; }
    VkFramebuffer vk_framebuffer() const { return m_framebuffer; }
    VkFramebuffer vk_framebuffer_load() const { return m_framebuffer_load; }
    void begin_render_pass(VkCommandBuffer cmd, float r, float g, float b, float a,
                           float depth = 1.0f, int stencil = 0);
    // Resume a render pass with LOAD_OP_LOAD (preserves previous content)
    void begin_render_pass_load(VkCommandBuffer cmd);

    // Update tracked layout on owned textures after render pass ends.
    // The render pass implicitly transitions attachments via finalLayout.
    void sync_attachment_layouts();

private:
    void destroy();

    // Shared helpers to avoid duplicated render pass / framebuffer creation between
    // init_simple() and init().
    bool create_render_pass_internal(
        const VkFormat* color_formats, uint32_t color_count,
        VkFormat depth_format, bool has_depth);
    bool create_framebuffer_internal(
        const VkImageView* views, uint32_t view_count,
        uint32_t width, uint32_t height);

    VKDevice* m_device = nullptr;
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkRenderPass m_render_pass_load = VK_NULL_HANDLE; // LOAD_OP_LOAD variant for resumption
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer_load = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<VKTexture>> m_owned_color_textures;
    std::unique_ptr<VKTexture> m_owned_depth_texture;
    std::vector<RHITexture*> m_color_attachments;
    RHITexture* m_depth_attachment = nullptr;

    TextureFormat m_color_format = TextureFormat::RGBA8;
    TextureFormat m_depth_format = TextureFormat::Depth24Stencil8;
    bool m_owns_textures = false;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
