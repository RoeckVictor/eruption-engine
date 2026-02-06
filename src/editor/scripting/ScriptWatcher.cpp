#include "ScriptWatcher.h"
#include "engine/core/Logger.h"

namespace fs = std::filesystem;

namespace editor {

ScriptWatcher::ScriptWatcher() = default;

ScriptWatcher::~ScriptWatcher() {
    stop();
}

void ScriptWatcher::set_watch_path(const std::string& path) {
    m_watch_path = path;
    m_file_times.clear();
}

void ScriptWatcher::start() {
    if (m_watch_path.empty() || !fs::exists(m_watch_path)) {
        engine::Logger::instance().warning("ScriptWatcher", "Watch path does not exist: %s", m_watch_path.c_str());
        return;
    }

    m_watching = true;
    m_changes_pending = false;

    // Initial scan to populate file times
    scan_directory();

    engine::Logger::instance().info("ScriptWatcher", "Started watching: %s (%zu files)", m_watch_path.c_str(), m_file_times.size());
}

void ScriptWatcher::stop() {
    m_watching = false;
    m_file_times.clear();
}

bool ScriptWatcher::poll() {
    if (!m_watching || m_watch_path.empty()) {
        return false;
    }

    bool changes_detected = false;

    try {
        // Check for modified or new files
        for (auto& entry : fs::recursive_directory_iterator(m_watch_path)) {
            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();
            if (!file_matches(path)) continue;

            std::string path_str = path.string();
            auto write_time = fs::last_write_time(path);

            auto it = m_file_times.find(path_str);
            if (it == m_file_times.end()) {
                // New file
                m_file_times[path_str] = write_time;
                changes_detected = true;
                engine::Logger::instance().info("ScriptWatcher", "New file: %s", path_str.c_str());
            } else if (it->second != write_time) {
                // Modified file
                it->second = write_time;
                changes_detected = true;
                engine::Logger::instance().info("ScriptWatcher", "Modified file: %s", path_str.c_str());
            }
        }

        // Check for deleted files
        std::vector<std::string> to_remove;
        for (const auto& [path_str, _] : m_file_times) {
            if (!fs::exists(path_str)) {
                to_remove.push_back(path_str);
                changes_detected = true;
                engine::Logger::instance().info("ScriptWatcher", "Deleted file: %s", path_str.c_str());
            }
        }
        for (const auto& path_str : to_remove) {
            m_file_times.erase(path_str);
        }

    } catch (const fs::filesystem_error& e) {
        engine::Logger::instance().error("ScriptWatcher", "Filesystem error: %s", e.what());
    }

    if (changes_detected) {
        m_changes_pending = true;
        m_last_change_time = std::chrono::steady_clock::now();
    }

    // Check if debounce delay has passed
    if (m_changes_pending) {
        float elapsed = time_since_change();
        if (elapsed >= m_debounce_delay) {
            m_changes_pending = false;

            if (m_changed_callback) {
                m_changed_callback();
            }

            return true;
        }
    }

    return false;
}

float ScriptWatcher::time_since_change() const {
    if (!m_changes_pending) {
        return 0.0f;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_change_time);
    return duration.count() / 1000.0f;
}

void ScriptWatcher::scan_directory() {
    m_file_times.clear();

    if (!fs::exists(m_watch_path)) {
        return;
    }

    try {
        for (auto& entry : fs::recursive_directory_iterator(m_watch_path)) {
            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();
            if (!file_matches(path)) continue;

            m_file_times[path.string()] = fs::last_write_time(path);
        }
    } catch (const fs::filesystem_error& e) {
        engine::Logger::instance().error("ScriptWatcher", "Error scanning directory: %s", e.what());
    }
}

bool ScriptWatcher::file_matches(const std::filesystem::path& path) const {
    std::string ext = path.extension().string();

    // Watch C++ source and header files
    return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".cxx" || ext == ".cc";
}

} // namespace editor
