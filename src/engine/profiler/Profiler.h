#pragma once

#include "ProfilerTypes.h"
#include <chrono>
#include <mutex>
#include <stack>
#include <functional>

namespace engine::profiler {

/// Callback for play state changes (set by editor to notify profiler).
using PlayStateCallback = std::function<bool()>;  // Returns true if playing

/// CPU profiler singleton for hierarchical scope-based profiling.
/// Works in a snapshot-based capture workflow for analyzing play mode performance.
class Profiler {
public:
    static Profiler& instance();

    // Frame lifecycle (called every frame by engine)
    void begin_frame();
    void end_frame();

    // Scope management (called by RAII helper)
    void begin_scope(const char* name);
    void end_scope();

    // Capture control
    void start_capture();
    void stop_capture();
    void clear_capture();
    CaptureState capture_state() const { return m_capture_state; }
    bool is_capturing() const { return m_capture_state == CaptureState::Recording; }

    // Configuration
    ProfilerConfig& config() { return m_config; }
    const ProfilerConfig& config() const { return m_config; }

    // Captured data access
    const ProfilerSnapshot& snapshot() const { return m_snapshot; }
    const FrameData& current_frame() const { return m_current_frame; }

    // Play state integration
    void set_play_state_callback(PlayStateCallback callback) { m_play_state_callback = callback; }
    void on_play_state_changed(bool is_playing);

    // Export captured data
    bool export_to_json(const char* filepath) const;
    bool import_from_json(const char* filepath);

    // Statistics for captured data
    std::vector<ProfileSummary> get_summaries() const;
    double average_frame_time_ms() const;
    double fps() const;

private:
    Profiler() = default;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    ProfilerConfig m_config;
    CaptureState m_capture_state = CaptureState::Idle;

    // Current frame state (always updated for live preview)
    FrameData m_current_frame;
    TimePoint m_frame_start;
    std::stack<uint32_t> m_scope_stack;
    uint64_t m_frame_counter = 0;

    // Captured snapshot
    ProfilerSnapshot m_snapshot;

    // Play state
    PlayStateCallback m_play_state_callback;
    bool m_was_playing = false;

    mutable std::mutex m_mutex;
};

/// RAII helper for automatic scope tracking.
class ProfileScope {
public:
    explicit ProfileScope(const char* name) {
        Profiler::instance().begin_scope(name);
    }
    ~ProfileScope() {
        Profiler::instance().end_scope();
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

} // namespace engine::profiler

// Convenience macros
// Helper macros for proper __LINE__ expansion in concatenation
#define PROFILE_CONCAT_INNER(a, b) a ## b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)

#define PROFILE_SCOPE(name) \
    ::engine::profiler::ProfileScope PROFILE_CONCAT(_profile_scope_, __LINE__)(name)

#define PROFILE_FUNCTION() \
    PROFILE_SCOPE(__FUNCTION__)

// Conditional profiling (can be disabled in release builds)
#ifndef ERUPTION_DISABLE_PROFILER
    #define PROFILE_SCOPE_IF(name) PROFILE_SCOPE(name)
    #define PROFILE_FUNCTION_IF() PROFILE_FUNCTION()
#else
    #define PROFILE_SCOPE_IF(name) (void)0
    #define PROFILE_FUNCTION_IF() (void)0
#endif
