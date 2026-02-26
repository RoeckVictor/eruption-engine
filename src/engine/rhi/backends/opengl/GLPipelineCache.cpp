#include "GLPipelineCache.h"

namespace engine::rhi {

bool GLPipelineCache::init(const PipelineCacheDesc& desc) {
    m_data = desc.initial_data;
    return true;
}

bool GLPipelineCache::get_data(std::vector<uint8_t>& out) const {
    out.clear();
    return true;
}

bool GLPipelineCache::empty() const {
    return true;
}

void GLPipelineCache::merge(const RHIPipelineCache& /*other*/) {
    // No-op for OpenGL
}

size_t GLPipelineCache::size() const {
    return 0;
}

}
