#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace engine::asset {

/// Tracks file modification times and fires callbacks on change.
class FileWatcher {
public:
    using Callback = std::function<void(const std::string& path)>;

    /// Start watching a file. The callback fires when modification time changes.
    void watch(const std::string& physical_path, Callback on_changed);

    /// Remove all watches for a specific path.
    void unwatch(const std::string& physical_path);

    /// Poll all watched files. Call once per frame.
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
};

} // namespace engine::asset
