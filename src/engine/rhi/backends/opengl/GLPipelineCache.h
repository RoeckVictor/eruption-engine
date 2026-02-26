#pragma once

#include "engine/rhi/RHIPipelineCache.h"

namespace engine::rhi {

// OpenGL doesn't support pipeline caching, so this is a no-op implementation
// The class exists for API compatibility with Vulkan
class GLPipelineCache : public RHIPipelineCache {
public:
    GLPipelineCache() = default;
    ~GLPipelineCache() override = default;

    bool init(const PipelineCacheDesc& desc);

    bool get_data(std::vector<uint8_t>& out) const override;
    bool empty() const override;
    void merge(const RHIPipelineCache& other) override;
    size_t size() const override;

private:
    std::vector<uint8_t> m_data;
};

}
