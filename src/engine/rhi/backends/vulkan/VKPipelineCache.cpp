#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKPipelineCache.h"
#include "VKCommon.h"
#include "VKDevice.h"

namespace engine::rhi {

VKPipelineCache::~VKPipelineCache() {
    if (m_cache && m_device) vkDestroyPipelineCache(m_device->device(), m_cache, nullptr);
}

bool VKPipelineCache::init(VKDevice* device, const PipelineCacheDesc& desc) {
    m_device = device;

    VkPipelineCacheCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = desc.initial_data.size();
    info.pInitialData = desc.initial_data.empty() ? nullptr : desc.initial_data.data();

    return VK_CHECK(vkCreatePipelineCache(device->device(), &info, nullptr, &m_cache));
}

bool VKPipelineCache::get_data(std::vector<uint8_t>& out) const {
    size_t data_size = 0;
    vkGetPipelineCacheData(m_device->device(), m_cache, &data_size, nullptr);
    out.resize(data_size);
    return vkGetPipelineCacheData(m_device->device(), m_cache, &data_size, out.data()) == VK_SUCCESS;
}

bool VKPipelineCache::empty() const {
    size_t data_size = 0;
    vkGetPipelineCacheData(m_device->device(), m_cache, &data_size, nullptr);
    return data_size == 0;
}

void VKPipelineCache::merge(const RHIPipelineCache& other) {
    auto* vk_other = static_cast<const VKPipelineCache*>(&other);
    vkMergePipelineCaches(m_device->device(), m_cache, 1, &vk_other->m_cache);
}

size_t VKPipelineCache::size() const {
    size_t data_size = 0;
    vkGetPipelineCacheData(m_device->device(), m_cache, &data_size, nullptr);
    return data_size;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
