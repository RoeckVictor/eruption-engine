#pragma once

#include "Panel.h"
#include <vector>
#include <string>
#include <mutex>

namespace editor {

// Log message entry for the console
struct LogEntry {
    enum class Level { Info, Warning, Error };

    Level level;
    std::string message;
    std::string source;
    int count = 1;
};

// Console panel for displaying log messages
// Automatically hooks into the engine's Logger system
class ConsolePanel : public Panel {
public:
    ConsolePanel();
    ~ConsolePanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void log(LogEntry::Level level, const std::string& message, const std::string& source = "");

    void clear();

    void log_info(const std::string& message, const std::string& source = "");
    void log_warning(const std::string& message, const std::string& source = "");
    void log_error(const std::string& message, const std::string& source = "");

private:
    void render_toolbar();
    void render_messages();
    void hook_logger();
    void unhook_logger();

    void copy_selected_to_clipboard(const std::vector<LogEntry>& visible_entries);

    std::vector<LogEntry> m_entries;
    std::mutex m_entries_mutex;
    char m_filter[128] = "";

    bool m_show_info = true;
    bool m_show_warnings = true;
    bool m_show_errors = true;
    bool m_collapse_duplicates = true;
    bool m_auto_scroll = true;

    std::vector<bool> m_selected;
    int m_last_clicked = -1;

    size_t m_logger_sink_id = 0;
    bool m_hooked = false;
};

}