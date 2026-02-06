#pragma once

#include "Panel.h"
#include <vector>
#include <string>
#include <mutex>

namespace editor {

/// Log message entry for the console.
struct LogEntry {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string message;
    std::string source;
    int count = 1;  // For collapsing duplicates
};

/// Console panel for displaying log messages.
/// Automatically hooks into the engine's Logger system.
class ConsolePanel : public Panel {
public:
    ConsolePanel();
    ~ConsolePanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    /// Add a log message.
    void log(LogEntry::Level level, const std::string& message, const std::string& source = "");

    /// Clear all messages.
    void clear();

    /// Convenience methods.
    void log_info(const std::string& message, const std::string& source = "");
    void log_warning(const std::string& message, const std::string& source = "");
    void log_error(const std::string& message, const std::string& source = "");

private:
    void render_toolbar();
    void render_messages();
    void hook_logger();
    void unhook_logger();

    std::vector<LogEntry> m_entries;
    std::mutex m_entries_mutex;  // For thread-safe logging
    char m_filter[128] = "";

    bool m_show_info = true;
    bool m_show_warnings = true;
    bool m_show_errors = true;
    bool m_collapse_duplicates = true;
    bool m_auto_scroll = true;

    size_t m_logger_sink_id = 0;
    bool m_hooked = false;
};

} // namespace editor
