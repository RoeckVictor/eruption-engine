#pragma once

#include "engine/profiler/GPUProfiler.h"
#include <vector>
#include <array>
#include <string>
#include <cstdint>

namespace engine::rhi {

// OpenGL implementation of GPU profiling using timer queries.
class GLGPUProfiler : public engine::profiler::GPUProfiler {
public:
    GLGPUProfiler();
    ~GLGPUProfiler() override;

    bool init() override;
    void shutdown() override;

    void begin_frame() override;
    void end_frame() override;

    uint32_t begin_scope(const char* name) override;
    void end_scope(uint32_t handle) override;

    bool collect_results(std::vector<engine::profiler::ProfileNode>& out_nodes,
                         double& out_total_gpu_time_ms) override;

    bool is_supported() const override { return m_supported; }
    uint32_t result_latency() const override { return RING_BUFFER_SIZE; }

private:
    static constexpr uint32_t RING_BUFFER_SIZE = 3;
    static constexpr uint32_t MAX_QUERIES_PER_FRAME = 64;

    struct QueryPair {
        uint32_t start_query = 0;
        uint32_t end_query = 0;
        std::string name;
        uint32_t depth = 0;
        uint32_t parent_index = UINT32_MAX;
    };

    struct FrameQueries {
        std::vector<QueryPair> queries;
        uint32_t frame_start_query = 0;
        uint32_t frame_end_query = 0;
        bool submitted = false;
    };

    std::array<FrameQueries, RING_BUFFER_SIZE> m_frames;
    uint32_t m_write_index = 0;
    uint32_t m_read_index = 0;
    uint32_t m_current_depth = 0;
    std::vector<uint32_t> m_scope_stack;

    // Query pool
    std::vector<uint32_t> m_query_pool;
    size_t m_pool_next = 0;

    bool m_supported = false;
    bool m_in_frame = false;

    uint32_t allocate_query();
    void reset_frame(FrameQueries& frame);
};

}
