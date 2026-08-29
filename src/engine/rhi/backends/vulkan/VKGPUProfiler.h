#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/profiler/GPUProfiler.h"
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <string>

namespace engine::rhi {

class VKDevice;

class VKGPUProfiler : public profiler::GPUProfiler {
public:
    VKGPUProfiler() = default;
    ~VKGPUProfiler() override;

    bool init(VKDevice* device);

    bool init() override { return m_supported; }
    void shutdown() override;
    void begin_frame() override;
    void end_frame() override;
    uint32_t begin_scope(const char* name) override;
    void end_scope(uint32_t handle) override;
    bool collect_results(std::vector<profiler::ProfileNode>& out_nodes,
                         double& out_total_gpu_time_ms) override;
    bool is_supported() const override { return m_supported; }
    uint32_t result_latency() const override { return RING_SIZE; }

private:
    static constexpr uint32_t RING_SIZE = 3;
    static constexpr uint32_t MAX_QUERIES_PER_FRAME = 64;

    struct ScopeEntry {
        std::string name;
        uint32_t begin_query;
        uint32_t end_query;
        uint32_t depth;
    };

    struct FrameData {
        std::vector<ScopeEntry> scopes;
        uint32_t next_query = 0;
        bool submitted = false;
    };

    uint32_t allocate_query(uint32_t frame_index);

    VKDevice* m_device = nullptr;
    VkQueryPool m_query_pool = VK_NULL_HANDLE;
    float m_timestamp_period = 1.0f;
    bool m_supported = false;
    bool m_in_frame = false;
    uint32_t m_write_index = 0;
    uint32_t m_read_index = 0;
    uint32_t m_current_depth = 0;

    std::array<FrameData, RING_SIZE> m_frames = {};
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
