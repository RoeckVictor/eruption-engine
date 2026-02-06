#pragma once

#include <string>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <chrono>

namespace editor {

/// Watches the Scripts folder for file changes.
/// Triggers recompilation when source files are modified.
class ScriptWatcher {
public:
    ScriptWatcher();
    ~ScriptWatcher();

    /// Set the scripts directory to watch.
    void set_watch_path(const std::string& path);

    /// Start watching for changes.
    void start();

    /// Stop watching.
    void stop();

    /// Check if currently watching.
    bool is_watching() const { return m_watching; }

    /// Poll for changes (call from main thread).
    /// Returns true if changes were detected.
    bool poll();

    /// Callback when files have changed.
    using ChangedCallback = std::function<void()>;
    void set_changed_callback(ChangedCallback callback) { m_changed_callback = callback; }

    /// Get time since last change was detected.
    float time_since_change() const;

    /// Debounce delay - wait this long after last change before triggering callback.
    void set_debounce_delay(float seconds) { m_debounce_delay = seconds; }
    float debounce_delay() const { return m_debounce_delay; }

private:
    void scan_directory();
    bool file_matches(const std::filesystem::path& path) const;

    std::string m_watch_path;
    bool m_watching = false;

    // File modification times
    std::unordered_map<std::string, std::filesystem::file_time_type> m_file_times;

    // Change tracking
    bool m_changes_pending = false;
    std::chrono::steady_clock::time_point m_last_change_time;
    float m_debounce_delay = 0.5f; // Wait 500ms after last change

    ChangedCallback m_changed_callback;
};

} // namespace editor
