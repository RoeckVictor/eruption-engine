#pragma once

#include "ProfilerTypes.h"
#include <vector>
#include <cstdint>

namespace engine::profiler {

/// Abstract interface for GPU profiling.
class GPUProfiler {
public:
    virtual ~GPUProfiler() = default;

    /// Initialize the profiler.
    virtual bool init() = 0;

    /// Shutdown and release resources.
    virtual void shutdown() = 0;

    /// Begin GPU profiling for a new frame.
    virtual void begin_frame() = 0;

    /// End GPU profiling for current frame.
    virtual void end_frame() = 0;

    /// Begin a named GPU profile scope.
    /// @return Scope handle for matching end_scope call.
    virtual uint32_t begin_scope(const char* name) = 0;

    /// End a GPU profile scope.
    virtual void end_scope(uint32_t handle) = 0;

    /// Collect results from previous frames (async retrieval).
    /// Call once per frame, returns data from N frames ago.
    virtual bool collect_results(std::vector<ProfileNode>& out_nodes,
                                  double& out_total_gpu_time_ms) = 0;

    /// Check if GPU profiling is supported.
    virtual bool is_supported() const = 0;

    /// Get the frame latency (how many frames old the results are).
    virtual uint32_t result_latency() const = 0;

protected:
    GPUProfiler() = default;
};

/// RAII helper for GPU scopes.
class GPUProfileScope {
public:
    GPUProfileScope(GPUProfiler* profiler, const char* name)
        : m_profiler(profiler)
        , m_handle(profiler ? profiler->begin_scope(name) : UINT32_MAX)
    {}

    ~GPUProfileScope() {
        if (m_profiler && m_handle != UINT32_MAX) {
            m_profiler->end_scope(m_handle);
        }
    }

    GPUProfileScope(const GPUProfileScope&) = delete;
    GPUProfileScope& operator=(const GPUProfileScope&) = delete;

private:
    GPUProfiler* m_profiler;
    uint32_t m_handle;
};

} // namespace engine::profiler

// GPU profiling macros (require profiler pointer)
#define GPU_PROFILE_SCOPE(profiler, name) \
    ::engine::profiler::GPUProfileScope _gpu_scope_##__LINE__(profiler, name)
