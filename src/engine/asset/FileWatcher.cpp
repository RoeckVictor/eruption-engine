#include "engine/asset/FileWatcher.h"
#include "engine/core/Log.h"
#include <algorithm>

namespace engine::asset {

namespace fs = std::filesystem;

void FileWatcher::watch(const std::string& physical_path, Callback on_changed) {
    // If already watching this path, update the callback instead of adding a duplicate
    for (auto& existing : m_entries) {
        if (existing.path == physical_path) {
            existing.callback = std::move(on_changed);
            return;
        }
    }

    WatchEntry entry;
    entry.path = physical_path;
    entry.callback = std::move(on_changed);

    std::error_code ec;
    entry.last_modified = fs::last_write_time(physical_path, ec);

    m_entries.push_back(std::move(entry));
}

void FileWatcher::unwatch(const std::string& physical_path) {
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [&](const WatchEntry& e) { return e.path == physical_path; }),
        m_entries.end());
}

void FileWatcher::poll() {
    auto now = std::chrono::steady_clock::now();
    if (now - m_last_poll < m_poll_interval) return;
    m_last_poll = now;

    for (auto& entry : m_entries) {
        std::error_code ec;
        auto current_time = fs::last_write_time(entry.path, ec);
        if (ec) continue;

        if (current_time != entry.last_modified) {
            entry.last_modified = current_time;
            entry.callback(entry.path);
        }
    }
}

void FileWatcher::clear() {
    m_entries.clear();
}

} // namespace engine::asset
