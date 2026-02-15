#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace engine::asset {

/// Tracks file modification times and fires callbacks on change.
/// Polls at a configurable interval (default 500ms) to avoid per-frame stat calls.
///
/// Thread safety: NOT thread-safe. All watch/unwatch/poll calls must happen
/// on the same thread (typically the main thread).
class FileWatcher {
public:
    using Callback = std::function<void(const std::string& path)>;

    /// Set poll interval. Files are only checked this often, not every frame.
    void set_poll_interval(std::chrono::milliseconds interval) { m_poll_interval = interval; }

    /// Start watching a file. The callback fires when modification time changes.
    /// If the path is already watched, the callback is updated instead of adding a duplicate.
    void watch(const std::string& physical_path, Callback on_changed);

    /// Remove all watches for a specific path.
    void unwatch(const std::string& physical_path);

    /// Poll all watched files. Call once per frame; actual stat calls are throttled
    /// to the configured poll interval.
    void poll();

    /// Remove all watches.
    void clear();

private:
    struct WatchEntry {
        std::string path;
        std::filesystem::file_time_type last_modified;
        Callback callback;
    };
    std::vector<WatchEntry> m_entries;
    std::chrono::steady_clock::time_point m_last_poll{};
    std::chrono::milliseconds m_poll_interval{500};
};

} // namespace engine::asset
