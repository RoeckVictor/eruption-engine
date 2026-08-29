#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKDescriptorSet.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKBuffer.h"
#include "VKTexture.h"

namespace engine::rhi {

// --- VKDescriptorSetLayout ---

VKDescriptorSetLayout::~VKDescriptorSetLayout() {
    if (m_layout && m_device) {
        vkDestroyDescriptorSetLayout(m_device->device(), m_layout, nullptr);
    }
}

bool VKDescriptorSetLayout::init(VKDevice* device, const DescriptorSetLayoutDesc& desc) {
    m_device = device;
    m_bindings = desc.bindings;

    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    for (const auto& b : desc.bindings) {
        VkDescriptorSetLayoutBinding vk_b = {};
        vk_b.binding = b.binding;
        vk_b.descriptorType = to_vk_descriptor_type(b.type);
        vk_b.descriptorCount = b.count;
        vk_b.stageFlags = VK_SHADER_STAGE_ALL; // Simplified — could map from b.stage
        vk_bindings.push_back(vk_b);
    }

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(vk_bindings.size());
    layout_info.pBindings = vk_bindings.data();

    return VK_CHECK(vkCreateDescriptorSetLayout(device->device(), &layout_info, nullptr, &m_layout));
}

// --- VKDescriptorSet ---

VKDescriptorSet::~VKDescriptorSet() {
    if (m_pool && m_device) {
        vkDestroyDescriptorPool(m_device->device(), m_pool, nullptr);
    }
}

bool VKDescriptorSet::init(VKDevice* device, const VKDescriptorSetLayout* layout) {
    m_device = device;
    m_layout = layout;

    // Count descriptor types for pool sizes
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (const auto& b : layout->bindings()) {
        VkDescriptorPoolSize ps = {};
        ps.type = to_vk_descriptor_type(b.type);
        ps.descriptorCount = b.count;
        pool_sizes.push_back(ps);
    }

    if (pool_sizes.empty()) {
        // Need at least one pool size
        VkDescriptorPoolSize ps = {};
        ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ps.descriptorCount = 1;
        pool_sizes.push_back(ps);
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    if (!VK_CHECK(vkCreateDescriptorPool(device->device(), &pool_info, nullptr, &m_pool))) {
        return false;
    }

    VkDescriptorSetLayout vk_layout = layout->vk_layout();
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &vk_layout;

    return VK_CHECK(vkAllocateDescriptorSets(device->device(), &alloc_info, &m_set));
}

void VKDescriptorSet::update(const DescriptorWrite* writes, uint32_t count) {
    std::vector<VkWriteDescriptorSet> vk_writes;
    std::vector<VkDescriptorBufferInfo> buffer_infos(count);
    std::vector<VkDescriptorImageInfo> image_infos(count);

    for (uint32_t i = 0; i < count; ++i) {
        const auto& w = writes[i];

        VkWriteDescriptorSet vk_w = {};
        vk_w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vk_w.dstSet = m_set;
        vk_w.dstBinding = w.binding;
        vk_w.dstArrayElement = w.array_element;
        vk_w.descriptorCount = 1;
        vk_w.descriptorType = to_vk_descriptor_type(w.type);

        if (w.buffer) {
            buffer_infos[i].buffer = static_cast<VkBuffer>(w.buffer->native_handle());
            buffer_infos[i].offset = w.buffer_offset;
            buffer_infos[i].range = w.buffer_range > 0 ? w.buffer_range : VK_WHOLE_SIZE;
            vk_w.pBufferInfo = &buffer_infos[i];
        }

        if (w.texture) {
            auto* vk_tex = static_cast<VKTexture*>(w.texture);
            image_infos[i].imageView = vk_tex->image_view();
            image_infos[i].sampler = vk_tex->sampler();
            image_infos[i].imageLayout = (w.type == DescriptorType::StorageImage)
                ? VK_IMAGE_LAYOUT_GENERAL
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vk_w.pImageInfo = &image_infos[i];
        }

        vk_writes.push_back(vk_w);
    }

    if (!vk_writes.empty()) {
        vkUpdateDescriptorSets(m_device->device(),
            static_cast<uint32_t>(vk_writes.size()), vk_writes.data(),
            0, nullptr);
    }
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
