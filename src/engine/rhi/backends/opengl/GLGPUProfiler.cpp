#include "GLGPUProfiler.h"
#include <glad/gl.h>

namespace engine::rhi {

GLGPUProfiler::GLGPUProfiler() = default;

GLGPUProfiler::~GLGPUProfiler() {
    shutdown();
}

bool GLGPUProfiler::init() {
    // Check for timer query support (core since GL 3.3)
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    m_supported = (major > 3) || (major == 3 && minor >= 3);

    if (!m_supported) return false;

    // Pre-allocate query pool
    // Each frame needs: 2 for frame start/end + MAX_QUERIES_PER_FRAME * 2 for scopes
    size_t queries_per_frame = 2 + MAX_QUERIES_PER_FRAME * 2;
    size_t total_queries = RING_BUFFER_SIZE * queries_per_frame;
    m_query_pool.resize(total_queries);
    glGenQueries(static_cast<GLsizei>(total_queries), m_query_pool.data());

    // Initialize frame structures
    for (auto& frame : m_frames) {
        frame.frame_start_query = allocate_query();
        frame.frame_end_query = allocate_query();
        frame.queries.reserve(MAX_QUERIES_PER_FRAME);
    }

    return true;
}

void GLGPUProfiler::shutdown() {
    if (!m_query_pool.empty()) {
        glDeleteQueries(static_cast<GLsizei>(m_query_pool.size()),
                        m_query_pool.data());
        m_query_pool.clear();
    }

    for (auto& frame : m_frames) {
        frame.queries.clear();
        frame.submitted = false;
    }

    m_supported = false;
    m_pool_next = 0;
}

uint32_t GLGPUProfiler::allocate_query() {
    if (m_pool_next >= m_query_pool.size()) {
        // Pool exhausted - this shouldn't happen with proper sizing
        return 0;
    }
    return m_query_pool[m_pool_next++];
}

void GLGPUProfiler::reset_frame(FrameQueries& frame) {
    frame.queries.clear();
    frame.submitted = false;
}

void GLGPUProfiler::begin_frame() {
    if (!m_supported) return;

    auto& frame = m_frames[m_write_index];
    reset_frame(frame);

    // Reset pool allocation for this frame's scope queries.
    // Pre-allocated frame start/end queries use indices 0 to (RING_BUFFER_SIZE * 2 - 1).
    // Each frame slot gets its own section of scope queries to avoid conflicts.
    size_t pre_allocated = RING_BUFFER_SIZE * 2;
    size_t scope_queries_per_frame = MAX_QUERIES_PER_FRAME * 2;
    m_pool_next = pre_allocated + m_write_index * scope_queries_per_frame;

    // Record frame start timestamp
    glQueryCounter(frame.frame_start_query, GL_TIMESTAMP);

    m_current_depth = 0;
    m_scope_stack.clear();
    m_in_frame = true;
}

void GLGPUProfiler::end_frame() {
    if (!m_supported || !m_in_frame) return;

    auto& frame = m_frames[m_write_index];

    // Record frame end timestamp
    glQueryCounter(frame.frame_end_query, GL_TIMESTAMP);

    frame.submitted = true;
    m_write_index = (m_write_index + 1) % RING_BUFFER_SIZE;
    m_in_frame = false;
}

uint32_t GLGPUProfiler::begin_scope(const char* name) {
    if (!m_supported || !m_in_frame) return UINT32_MAX;

    auto& frame = m_frames[m_write_index];

    if (frame.queries.size() >= MAX_QUERIES_PER_FRAME) {
        return UINT32_MAX;  // Limit reached
    }

    QueryPair query;
    query.name = name;
    query.depth = m_current_depth;
    query.start_query = allocate_query();
    query.end_query = allocate_query();

    // Check if pool allocation failed
    if (query.start_query == 0 || query.end_query == 0) {
        return UINT32_MAX;
    }

    if (!m_scope_stack.empty()) {
        query.parent_index = m_scope_stack.back();
    }

    // Record start timestamp
    glQueryCounter(query.start_query, GL_TIMESTAMP);

    uint32_t index = static_cast<uint32_t>(frame.queries.size());
    frame.queries.push_back(std::move(query));

    m_scope_stack.push_back(index);
    m_current_depth++;

    return index;
}

void GLGPUProfiler::end_scope(uint32_t handle) {
    if (!m_supported || !m_in_frame || handle == UINT32_MAX) return;

    auto& frame = m_frames[m_write_index];

    if (handle >= frame.queries.size()) return;

    // Record end timestamp
    glQueryCounter(frame.queries[handle].end_query, GL_TIMESTAMP);

    if (!m_scope_stack.empty()) {
        m_scope_stack.pop_back();
    }
    if (m_current_depth > 0) {
        m_current_depth--;
    }
}

bool GLGPUProfiler::collect_results(
    std::vector<engine::profiler::ProfileNode>& out_nodes,
    double& out_total_gpu_time_ms)
{
    out_nodes.clear();
    out_total_gpu_time_ms = 0.0;

    if (!m_supported) return false;

    auto& frame = m_frames[m_read_index];

    if (!frame.submitted) {
        m_read_index = (m_read_index + 1) % RING_BUFFER_SIZE;
        return false;
    }

    // Check if results are available (non-blocking)
    GLint available = 0;
    glGetQueryObjectiv(frame.frame_end_query, GL_QUERY_RESULT_AVAILABLE, &available);

    if (!available) {
        return false;
    }

    // Get frame timestamps
    GLuint64 frame_start_ns = 0, frame_end_ns = 0;
    glGetQueryObjectui64v(frame.frame_start_query, GL_QUERY_RESULT, &frame_start_ns);
    glGetQueryObjectui64v(frame.frame_end_query, GL_QUERY_RESULT, &frame_end_ns);

    out_total_gpu_time_ms = static_cast<double>(frame_end_ns - frame_start_ns) / 1000000.0;

    // Collect individual scope timings
    out_nodes.reserve(frame.queries.size());

    for (const auto& query : frame.queries) {
        GLuint64 start_ns = 0, end_ns = 0;
        glGetQueryObjectui64v(query.start_query, GL_QUERY_RESULT, &start_ns);
        glGetQueryObjectui64v(query.end_query, GL_QUERY_RESULT, &end_ns);

        engine::profiler::ProfileNode node;
        node.name = query.name;
        node.depth = query.depth;
        node.parent_index = query.parent_index;
        node.start_time_ms = static_cast<double>(start_ns - frame_start_ns) / 1000000.0;
        node.duration_ms = static_cast<double>(end_ns - start_ns) / 1000000.0;

        out_nodes.push_back(std::move(node));
    }

    // Advance read pointer
    frame.submitted = false;
    m_read_index = (m_read_index + 1) % RING_BUFFER_SIZE;

    return true;
}

}