#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::profiler {

/// Represents a single profiling node in the hierarchy.
struct ProfileNode {
    std::string name;
    double start_time_ms = 0.0;      // Relative to frame start
    double duration_ms = 0.0;
    uint32_t depth = 0;              // Nesting level
    uint32_t parent_index = UINT32_MAX;
};

/// Complete profiling data for one frame.
struct FrameData {
    uint64_t frame_number = 0;
    double total_frame_time_ms = 0.0;
    double cpu_time_ms = 0.0;
    double gpu_time_ms = 0.0;
    std::vector<ProfileNode> cpu_nodes;
    std::vector<ProfileNode> gpu_nodes;
};

/// A captured profiling session containing multiple frames.
struct ProfilerSnapshot {
    std::string name;
    std::vector<FrameData> frames;
    uint64_t start_frame = 0;
    double total_duration_ms = 0.0;

    void clear() {
        frames.clear();
        start_frame = 0;
        total_duration_ms = 0.0;
    }

    bool empty() const { return frames.empty(); }
    size_t frame_count() const { return frames.size(); }
};

/// Summary statistics for a named profile scope.
struct ProfileSummary {
    std::string name;
    double avg_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double last_ms = 0.0;
    uint32_t call_count = 0;
};

/// Profiler capture state.
enum class CaptureState {
    Idle,       // Not capturing
    Recording,  // Actively recording frames
    Stopped     // Capture finished, data available
};

/// Configuration for the profiler.
struct ProfilerConfig {
    bool capture_gpu = true;
    bool auto_start_on_play = true;
    bool auto_stop_on_play_end = true;
    size_t max_frames = 10000;  // Prevent unbounded memory growth
};

} // namespace engine::profiler
