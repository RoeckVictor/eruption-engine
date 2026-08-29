#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHITexture.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace engine::rhi {

class VKDevice;

class VKTexture : public RHITexture {
public:
    VKTexture() = default;
    ~VKTexture() override;

    bool init(VKDevice* device, const TextureDesc& desc);

    void upload(int x, int y, int w, int h, const void* data) override;
    void readback(int x, int y, int w, int h, void* dst, size_t dst_size) const override;
    void bind(uint32_t unit) const override;
    void bind_as_image(uint32_t unit, ImageAccess access) override;
    void generate_mipmaps() override;
    void* native_handle() const override { return m_image; }

    VkImage image() const { return m_image; }
    VkImageView image_view() const { return m_image_view; }
    VkSampler sampler() const { return m_sampler; }
    VkImageLayout current_layout() const { return m_current_layout; }

    void transition_layout(VkCommandBuffer cmd, VkImageLayout new_layout) const;

    // Force-update tracked layout without issuing a barrier.
    // Used when the render pass implicitly transitions the layout via finalLayout.
    void override_tracked_layout(VkImageLayout layout) const { m_current_layout = layout; }

private:
    VKDevice* m_device = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_image_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkFormat m_vk_format = VK_FORMAT_UNDEFINED;
    // Mutable: layout tracking is internal bookkeeping, not part of the texture's logical state.
    // This allows transition_layout to be called from const methods like readback().
    mutable VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
