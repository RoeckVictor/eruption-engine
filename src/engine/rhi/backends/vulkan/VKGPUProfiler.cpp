#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKGPUProfiler.h"
#include "VKCommon.h"
#include "VKDevice.h"
#include "VKContext.h"

namespace engine::rhi {

VKGPUProfiler::~VKGPUProfiler() {
    shutdown();
}

bool VKGPUProfiler::init(VKDevice* device) {
    m_device = device;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device->physical_device(), &props);

    if (props.limits.timestampComputeAndGraphics == VK_FALSE) {
        ENGINE_LOG_WARN("GPU timestamp queries not supported");
        m_supported = false;
        return true;
    }

    m_timestamp_period = props.limits.timestampPeriod;

    VkQueryPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = RING_SIZE * MAX_QUERIES_PER_FRAME * 2;

    if (!VK_CHECK(vkCreateQueryPool(device->device(), &pool_info, nullptr, &m_query_pool))) {
        return false;
    }

    m_supported = true;
    return true;
}

void VKGPUProfiler::shutdown() {
    if (m_query_pool && m_device) {
        vkDestroyQueryPool(m_device->device(), m_query_pool, nullptr);
        m_query_pool = VK_NULL_HANDLE;
    }
    m_supported = false;
}

uint32_t VKGPUProfiler::allocate_query(uint32_t frame_index) {
    uint32_t base = frame_index * MAX_QUERIES_PER_FRAME * 2;
    auto& frame = m_frames[frame_index];
    if (frame.next_query >= MAX_QUERIES_PER_FRAME * 2) return UINT32_MAX;
    return base + frame.next_query++;
}

void VKGPUProfiler::begin_frame() {
    if (!m_supported) return;

    auto& frame = m_frames[m_write_index];
    frame.scopes.clear();
    frame.next_query = 0;
    frame.submitted = false;
    m_current_depth = 0;

    // Reset queries for this frame's range
    auto* ctx = static_cast<VKContext*>(m_device->context());
    VkCommandBuffer cmd = ctx->active_command_buffer();
    if (cmd) {
        uint32_t base = m_write_index * MAX_QUERIES_PER_FRAME * 2;
        vkCmdResetQueryPool(cmd, m_query_pool, base, MAX_QUERIES_PER_FRAME * 2);
    }

    m_in_frame = true;
}

void VKGPUProfiler::end_frame() {
    if (!m_supported || !m_in_frame) return;
    m_frames[m_write_index].submitted = true;
    m_write_index = (m_write_index + 1) % RING_SIZE;
    m_in_frame = false;
}

uint32_t VKGPUProfiler::begin_scope(const char* name) {
    if (!m_supported || !m_in_frame) return UINT32_MAX;

    auto* ctx = static_cast<VKContext*>(m_device->context());
    VkCommandBuffer cmd = ctx->active_command_buffer();
    if (!cmd) return UINT32_MAX;

    uint32_t begin_query = allocate_query(m_write_index);
    uint32_t end_query = allocate_query(m_write_index);
    if (begin_query == UINT32_MAX || end_query == UINT32_MAX) return UINT32_MAX;

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_query_pool, begin_query);

    auto& frame = m_frames[m_write_index];
    uint32_t handle = static_cast<uint32_t>(frame.scopes.size());
    frame.scopes.push_back({name ? name : "", begin_query, end_query, m_current_depth});
    m_current_depth++;

    return handle;
}

void VKGPUProfiler::end_scope(uint32_t handle) {
    if (!m_supported || !m_in_frame || handle == UINT32_MAX) return;

    auto* ctx = static_cast<VKContext*>(m_device->context());
    VkCommandBuffer cmd = ctx->active_command_buffer();
    if (!cmd) return;

    auto& frame = m_frames[m_write_index];
    if (handle >= frame.scopes.size()) return;

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        m_query_pool, frame.scopes[handle].end_query);

    if (m_current_depth > 0) m_current_depth--;
}

bool VKGPUProfiler::collect_results(std::vector<profiler::ProfileNode>& out_nodes,
                                     double& out_total_gpu_time_ms) {
    out_nodes.clear();
    out_total_gpu_time_ms = 0.0;

    if (!m_supported) return false;

    // Read from the oldest submitted frame
    uint32_t read_idx = (m_write_index + 1) % RING_SIZE;
    auto& frame = m_frames[read_idx];
    if (!frame.submitted || frame.scopes.empty()) return false;

    // Fetch all timestamp results for this frame
    uint32_t base = read_idx * MAX_QUERIES_PER_FRAME * 2;
    uint32_t count = frame.next_query;
    if (count == 0) return false;

    std::vector<uint64_t> timestamps(count);
    VkResult result = vkGetQueryPoolResults(
        m_device->device(), m_query_pool,
        base, count,
        count * sizeof(uint64_t), timestamps.data(),
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    if (result != VK_SUCCESS) return false;

    double ns_per_tick = static_cast<double>(m_timestamp_period);

    for (const auto& scope : frame.scopes) {
        uint32_t bi = scope.begin_query - base;
        uint32_t ei = scope.end_query - base;
        if (bi >= count || ei >= count) continue;

        double dt_ns = static_cast<double>(timestamps[ei] - timestamps[bi]) * ns_per_tick;
        double dt_ms = dt_ns / 1'000'000.0;

        profiler::ProfileNode node;
        node.name = scope.name;
        node.duration_ms = dt_ms;
        node.depth = scope.depth;
        out_nodes.push_back(node);

        if (scope.depth == 0) out_total_gpu_time_ms += dt_ms;
    }

    return true;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
