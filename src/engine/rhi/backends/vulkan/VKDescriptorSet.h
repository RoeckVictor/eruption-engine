#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIDescriptorSet.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

class VKDevice;

class VKDescriptorSetLayout : public RHIDescriptorSetLayout {
public:
    VKDescriptorSetLayout() = default;
    ~VKDescriptorSetLayout() override;

    bool init(VKDevice* device, const DescriptorSetLayoutDesc& desc);

    const std::vector<DescriptorBinding>& bindings() const override { return m_bindings; }
    VkDescriptorSetLayout vk_layout() const { return m_layout; }

private:
    VKDevice* m_device = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    std::vector<DescriptorBinding> m_bindings;
};

class VKDescriptorSet : public RHIDescriptorSet {
public:
    VKDescriptorSet() = default;
    ~VKDescriptorSet() override;

    bool init(VKDevice* device, const VKDescriptorSetLayout* layout);

    void update(const DescriptorWrite* writes, uint32_t count) override;
    const RHIDescriptorSetLayout* layout() const override { return m_layout; }

    VkDescriptorSet handle() const { return m_set; }

private:
    VKDevice* m_device = nullptr;
    const VKDescriptorSetLayout* m_layout = nullptr;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;  // Each set owns its pool for simplicity
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
