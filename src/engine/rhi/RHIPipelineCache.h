#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace engine::rhi {

// Pipeline cache for serializing and caching compiled pipeline state
// OpenGL: No direct equivalent - provides a no-op implementation
class RHIPipelineCache {
public:
    virtual ~RHIPipelineCache() = default;

    RHIPipelineCache(const RHIPipelineCache&) = delete;
    RHIPipelineCache& operator=(const RHIPipelineCache&) = delete;

    virtual bool get_data(std::vector<uint8_t>& out) const = 0;

    virtual bool empty() const = 0;

    virtual void merge(const RHIPipelineCache& other) = 0;

    virtual size_t size() const = 0;

protected:
    RHIPipelineCache() = default;
};

struct PipelineCacheDesc {
    std::vector<uint8_t> initial_data;
};

}
