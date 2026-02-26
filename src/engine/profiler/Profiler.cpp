#include "engine/profiler/Profiler.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace engine::profiler {

Profiler& Profiler::instance() {
    static Profiler s_instance;
    return s_instance;
}

void Profiler::begin_frame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for play state changes if callback is set
    if (m_play_state_callback) {
        bool is_playing = m_play_state_callback();
        if (is_playing != m_was_playing) {
            // Play state changed - handle without lock (we already have it)
            if (is_playing && m_config.auto_start_on_play) {
                // Entering play mode - auto start capture
                m_capture_state = CaptureState::Recording;
                m_snapshot.clear();
                m_snapshot.start_frame = m_frame_counter;
            } else if (!is_playing && m_config.auto_stop_on_play_end) {
                // Exiting play mode - auto stop capture
                if (m_capture_state == CaptureState::Recording) {
                    m_capture_state = CaptureState::Stopped;
                }
            }
            m_was_playing = is_playing;
        }
    }

    m_current_frame = FrameData{};
    m_current_frame.frame_number = m_frame_counter++;
    m_frame_start = Clock::now();

    // Clear scope stack (should be empty, but safety check)
    while (!m_scope_stack.empty()) m_scope_stack.pop();
}

void Profiler::end_frame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = Clock::now();
    m_current_frame.total_frame_time_ms =
        std::chrono::duration<double, std::milli>(now - m_frame_start).count();

    // Calculate total CPU time from root nodes
    m_current_frame.cpu_time_ms = 0.0;
    for (const auto& node : m_current_frame.cpu_nodes) {
        if (node.depth == 0) {
            m_current_frame.cpu_time_ms += node.duration_ms;
        }
    }

    // Add to snapshot if capturing
    if (m_capture_state == CaptureState::Recording) {
        if (m_snapshot.frames.size() < m_config.max_frames) {
            m_snapshot.frames.push_back(m_current_frame);
            m_snapshot.total_duration_ms += m_current_frame.total_frame_time_ms;
        } else {
            // Max frames reached - auto stop
            m_capture_state = CaptureState::Stopped;
        }
    }
}

void Profiler::begin_scope(const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = Clock::now();
    double start_ms = std::chrono::duration<double, std::milli>(
        now - m_frame_start).count();

    ProfileNode node;
    node.name = name;
    node.start_time_ms = start_ms;
    node.depth = static_cast<uint32_t>(m_scope_stack.size());

    uint32_t node_index = static_cast<uint32_t>(m_current_frame.cpu_nodes.size());

    if (!m_scope_stack.empty()) {
        node.parent_index = m_scope_stack.top();
    }

    m_current_frame.cpu_nodes.push_back(std::move(node));
    m_scope_stack.push(node_index);
}

void Profiler::end_scope() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_scope_stack.empty()) return;

    auto now = Clock::now();
    double end_ms = std::chrono::duration<double, std::milli>(
        now - m_frame_start).count();

    uint32_t node_index = m_scope_stack.top();
    m_scope_stack.pop();

    auto& node = m_current_frame.cpu_nodes[node_index];
    node.duration_ms = end_ms - node.start_time_ms;
}

void Profiler::start_capture() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
    m_snapshot.start_frame = m_frame_counter;
    m_capture_state = CaptureState::Recording;
}

void Profiler::stop_capture() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_capture_state == CaptureState::Recording) {
        m_capture_state = CaptureState::Stopped;
    }
}

void Profiler::clear_capture() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
    m_capture_state = CaptureState::Idle;
}

void Profiler::on_play_state_changed(bool is_playing) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (is_playing && !m_was_playing && m_config.auto_start_on_play) {
        m_snapshot.clear();
        m_snapshot.start_frame = m_frame_counter;
        m_capture_state = CaptureState::Recording;
    } else if (!is_playing && m_was_playing && m_config.auto_stop_on_play_end) {
        if (m_capture_state == CaptureState::Recording) {
            m_capture_state = CaptureState::Stopped;
        }
    }

    m_was_playing = is_playing;
}

bool Profiler::export_to_json(const char* filepath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_snapshot.empty()) return false;

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << std::fixed << std::setprecision(4);
    file << "{\n";
    file << "  \"name\": \"" << m_snapshot.name << "\",\n";
    file << "  \"start_frame\": " << m_snapshot.start_frame << ",\n";
    file << "  \"total_duration_ms\": " << m_snapshot.total_duration_ms << ",\n";
    file << "  \"frame_count\": " << m_snapshot.frames.size() << ",\n";
    file << "  \"frames\": [\n";

    for (size_t i = 0; i < m_snapshot.frames.size(); ++i) {
        const auto& frame = m_snapshot.frames[i];
        file << "    {\n";
        file << "      \"frame_number\": " << frame.frame_number << ",\n";
        file << "      \"total_frame_time_ms\": " << frame.total_frame_time_ms << ",\n";
        file << "      \"cpu_time_ms\": " << frame.cpu_time_ms << ",\n";
        file << "      \"gpu_time_ms\": " << frame.gpu_time_ms << ",\n";
        file << "      \"cpu_nodes\": [\n";

        for (size_t j = 0; j < frame.cpu_nodes.size(); ++j) {
            const auto& node = frame.cpu_nodes[j];
            file << "        {\n";
            file << "          \"name\": \"" << node.name << "\",\n";
            file << "          \"start_time_ms\": " << node.start_time_ms << ",\n";
            file << "          \"duration_ms\": " << node.duration_ms << ",\n";
            file << "          \"depth\": " << node.depth << ",\n";
            file << "          \"parent_index\": " << node.parent_index << "\n";
            file << "        }" << (j + 1 < frame.cpu_nodes.size() ? "," : "") << "\n";
        }

        file << "      ],\n";
        file << "      \"gpu_nodes\": [\n";

        for (size_t j = 0; j < frame.gpu_nodes.size(); ++j) {
            const auto& node = frame.gpu_nodes[j];
            file << "        {\n";
            file << "          \"name\": \"" << node.name << "\",\n";
            file << "          \"start_time_ms\": " << node.start_time_ms << ",\n";
            file << "          \"duration_ms\": " << node.duration_ms << ",\n";
            file << "          \"depth\": " << node.depth << ",\n";
            file << "          \"parent_index\": " << node.parent_index << "\n";
            file << "        }" << (j + 1 < frame.gpu_nodes.size() ? "," : "") << "\n";
        }

        file << "      ]\n";
        file << "    }" << (i + 1 < m_snapshot.frames.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    return true;
}

bool Profiler::import_from_json(const char* filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;

        // Clear current snapshot
        m_snapshot.clear();

        // Parse snapshot metadata
        if (j.contains("name")) {
            m_snapshot.name = j["name"].get<std::string>();
        }
        if (j.contains("start_frame")) {
            m_snapshot.start_frame = j["start_frame"].get<uint64_t>();
        }
        if (j.contains("total_duration_ms")) {
            m_snapshot.total_duration_ms = j["total_duration_ms"].get<double>();
        }

        // Parse frames
        if (j.contains("frames") && j["frames"].is_array()) {
            for (const auto& frame_json : j["frames"]) {
                FrameData frame;

                if (frame_json.contains("frame_number")) {
                    frame.frame_number = frame_json["frame_number"].get<uint64_t>();
                }
                if (frame_json.contains("total_frame_time_ms")) {
                    frame.total_frame_time_ms = frame_json["total_frame_time_ms"].get<double>();
                }
                if (frame_json.contains("cpu_time_ms")) {
                    frame.cpu_time_ms = frame_json["cpu_time_ms"].get<double>();
                }
                if (frame_json.contains("gpu_time_ms")) {
                    frame.gpu_time_ms = frame_json["gpu_time_ms"].get<double>();
                }

                // Parse CPU nodes
                if (frame_json.contains("cpu_nodes") && frame_json["cpu_nodes"].is_array()) {
                    for (const auto& node_json : frame_json["cpu_nodes"]) {
                        ProfileNode node;
                        if (node_json.contains("name")) {
                            node.name = node_json["name"].get<std::string>();
                        }
                        if (node_json.contains("start_time_ms")) {
                            node.start_time_ms = node_json["start_time_ms"].get<double>();
                        }
                        if (node_json.contains("duration_ms")) {
                            node.duration_ms = node_json["duration_ms"].get<double>();
                        }
                        if (node_json.contains("depth")) {
                            node.depth = node_json["depth"].get<uint32_t>();
                        }
                        if (node_json.contains("parent_index")) {
                            node.parent_index = node_json["parent_index"].get<uint32_t>();
                        }
                        frame.cpu_nodes.push_back(std::move(node));
                    }
                }

                // Parse GPU nodes
                if (frame_json.contains("gpu_nodes") && frame_json["gpu_nodes"].is_array()) {
                    for (const auto& node_json : frame_json["gpu_nodes"]) {
                        ProfileNode node;
                        if (node_json.contains("name")) {
                            node.name = node_json["name"].get<std::string>();
                        }
                        if (node_json.contains("start_time_ms")) {
                            node.start_time_ms = node_json["start_time_ms"].get<double>();
                        }
                        if (node_json.contains("duration_ms")) {
                            node.duration_ms = node_json["duration_ms"].get<double>();
                        }
                        if (node_json.contains("depth")) {
                            node.depth = node_json["depth"].get<uint32_t>();
                        }
                        if (node_json.contains("parent_index")) {
                            node.parent_index = node_json["parent_index"].get<uint32_t>();
                        }
                        frame.gpu_nodes.push_back(std::move(node));
                    }
                }

                m_snapshot.frames.push_back(std::move(frame));
            }
        }

        // Set capture state to stopped since we have loaded data
        m_capture_state = CaptureState::Stopped;

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

std::vector<ProfileSummary> Profiler::get_summaries() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::unordered_map<std::string, ProfileSummary> summaries;

    const auto& frames = m_snapshot.empty() ? std::vector<FrameData>{m_current_frame} : m_snapshot.frames;

    for (const auto& frame : frames) {
        for (const auto& node : frame.cpu_nodes) {
            auto& summary = summaries[node.name];
            summary.name = node.name;
            summary.call_count++;
            summary.last_ms = node.duration_ms;

            if (summary.call_count == 1) {
                summary.min_ms = summary.max_ms = summary.avg_ms = node.duration_ms;
            } else {
                summary.min_ms = std::min(summary.min_ms, node.duration_ms);
                summary.max_ms = std::max(summary.max_ms, node.duration_ms);
                summary.avg_ms += (node.duration_ms - summary.avg_ms) / summary.call_count;
            }
        }
    }

    std::vector<ProfileSummary> result;
    result.reserve(summaries.size());
    for (const auto& [name, summary] : summaries) {
        result.push_back(summary);
    }

    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.avg_ms > b.avg_ms; });

    return result;
}

double Profiler::average_frame_time_ms() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto& frames = m_snapshot.empty() ? std::vector<FrameData>{m_current_frame} : m_snapshot.frames;

    if (frames.empty()) return 0.0;

    double sum = 0.0;
    for (const auto& frame : frames) {
        sum += frame.total_frame_time_ms;
    }
    return sum / static_cast<double>(frames.size());
}

double Profiler::fps() const {
    double avg = average_frame_time_ms();
    return avg > 0.0 ? 1000.0 / avg : 0.0;
}

} // namespace engine::profiler
