#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIPipelineCache.h"
#include <vulkan/vulkan.h>

namespace engine::rhi {

class VKDevice;

class VKPipelineCache : public RHIPipelineCache {
public:
    VKPipelineCache() = default;
    ~VKPipelineCache() override;

    bool init(VKDevice* device, const PipelineCacheDesc& desc);

    bool get_data(std::vector<uint8_t>& out) const override;
    bool empty() const override;
    void merge(const RHIPipelineCache& other) override;
    size_t size() const override;

    VkPipelineCache handle() const { return m_cache; }

private:
    VKDevice* m_device = nullptr;
    VkPipelineCache m_cache = VK_NULL_HANDLE;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
