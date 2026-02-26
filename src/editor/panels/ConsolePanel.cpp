#include "ConsolePanel.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <algorithm>

namespace editor {

ConsolePanel::ConsolePanel()
    : Panel("Console")
{
    // Hook into logger immediately
    hook_logger();
}

ConsolePanel::~ConsolePanel() {
    unhook_logger();
}

void ConsolePanel::on_open() {
    // Already hooked in constructor
}

void ConsolePanel::on_close() {
    // Keep hook active even when panel is hidden
}

void ConsolePanel::on_gui() {
    render_toolbar();
    ImGui::Separator();
    render_messages();
}

void ConsolePanel::log(LogEntry::Level level, const std::string& message, const std::string& source) {
    std::lock_guard<std::mutex> lock(m_entries_mutex);

    // Check for duplicate if collapse is enabled
    if (m_collapse_duplicates && !m_entries.empty()) {
        auto& last = m_entries.back();
        if (last.level == level && last.message == message && last.source == source) {
            last.count++;
            return;
        }
    }

    m_entries.push_back({level, message, source, 1});

    // Limit entries to prevent memory issues
    const size_t MAX_ENTRIES = 10000;
    if (m_entries.size() > MAX_ENTRIES) {
        m_entries.erase(m_entries.begin(), m_entries.begin() + 1000);
    }
}

void ConsolePanel::clear() {
    std::lock_guard<std::mutex> lock(m_entries_mutex);
    m_entries.clear();
}

void ConsolePanel::log_info(const std::string& message, const std::string& source) {
    log(LogEntry::Level::Info, message, source);
}

void ConsolePanel::log_warning(const std::string& message, const std::string& source) {
    log(LogEntry::Level::Warning, message, source);
}

void ConsolePanel::log_error(const std::string& message, const std::string& source) {
    log(LogEntry::Level::Error, message, source);
}

void ConsolePanel::hook_logger() {
    if (m_hooked) return;

    // Create a sink that forwards to this panel
    m_logger_sink_id = engine::Logger::instance().add_sink(
        [this](const engine::LogEntry& entry) {
            LogEntry::Level level = LogEntry::Level::Info;
            switch (entry.level) {
                case engine::LogLevel::Info:
                    level = LogEntry::Level::Info;
                    break;
                case engine::LogLevel::Warning:
                    level = LogEntry::Level::Warning;
                    break;
                case engine::LogLevel::Error:
                    level = LogEntry::Level::Error;
                    break;
            }
            log(level, entry.message, entry.tag);
        }
    );
    m_hooked = true;
}

void ConsolePanel::unhook_logger() {
    if (!m_hooked) return;

    engine::Logger::instance().remove_sink(m_logger_sink_id);
    m_hooked = false;
}

void ConsolePanel::render_toolbar() {
    if (ImGui::Button(ICON_FA_TRASH " Clear")) {
        clear();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_COPY " Copy All")) {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        copy_selected_to_clipboard(m_entries);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_show_info);

    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &m_show_warnings);

    ImGui::SameLine();
    ImGui::Checkbox("Errors", &m_show_errors);

    ImGui::SameLine();
    ImGui::Checkbox("Collapse", &m_collapse_duplicates);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Filter", "Filter...", m_filter, sizeof(m_filter));
}

void ConsolePanel::render_messages() {
    ImGui::BeginChild("ConsoleMessages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::string filter_str(m_filter);
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    // Copy entries under lock to avoid holding lock during rendering
    std::vector<LogEntry> entries_copy;
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        entries_copy = m_entries;
    }

    // Build visible (filtered) entries list
    struct VisibleEntry {
        size_t original_index;
        const LogEntry* entry;
    };
    std::vector<VisibleEntry> visible;
    visible.reserve(entries_copy.size());

    for (size_t i = 0; i < entries_copy.size(); ++i) {
        const auto& entry = entries_copy[i];

        if (entry.level == LogEntry::Level::Info && !m_show_info) continue;
        if (entry.level == LogEntry::Level::Warning && !m_show_warnings) continue;
        if (entry.level == LogEntry::Level::Error && !m_show_errors) continue;

        if (!filter_str.empty()) {
            std::string message_lower = entry.message;
            std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            if (message_lower.find(filter_str) == std::string::npos) {
                continue;
            }
        }

        visible.push_back({i, &entry});
    }

    // Resize selection vector to match visible entries
    m_selected.resize(visible.size(), false);
    if (m_selected.size() > visible.size()) {
        m_selected.resize(visible.size());
    }

    // Handle Ctrl+A (select all) and Ctrl+C (copy) when console child is focused
    if (ImGui::IsWindowFocused()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            for (size_t i = 0; i < m_selected.size(); ++i) m_selected[i] = true;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            // Collect selected visible entries
            std::vector<LogEntry> selected_entries;
            for (size_t vi = 0; vi < visible.size(); ++vi) {
                if (vi < m_selected.size() && m_selected[vi]) {
                    selected_entries.push_back(*visible[vi].entry);
                }
            }
            if (!selected_entries.empty()) {
                copy_selected_to_clipboard(selected_entries);
            }
        }
    }

    for (size_t vi = 0; vi < visible.size(); ++vi) {
        const auto& entry = *visible[vi].entry;

        ImGui::PushID(static_cast<int>(vi));

        // Determine row color
        ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        const char* icon = "[INFO]";
        switch (entry.level) {
            case LogEntry::Level::Info:
                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                icon = "[INFO]";
                break;
            case LogEntry::Level::Warning:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                icon = "[WARN]";
                break;
            case LogEntry::Level::Error:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                icon = "[ERR]";
                break;
        }

        // Selection highlight
        bool is_selected = (vi < m_selected.size()) && m_selected[vi];

        // Build the display string for the selectable
        char display[1024];
        if (!entry.source.empty() && entry.count > 1) {
            snprintf(display, sizeof(display), "%s [%s] (%d) %s", icon, entry.source.c_str(), entry.count, entry.message.c_str());
        } else if (!entry.source.empty()) {
            snprintf(display, sizeof(display), "%s [%s] %s", icon, entry.source.c_str(), entry.message.c_str());
        } else if (entry.count > 1) {
            snprintf(display, sizeof(display), "%s (%d) %s", icon, entry.count, entry.message.c_str());
        } else {
            snprintf(display, sizeof(display), "%s %s", icon, entry.message.c_str());
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (ImGui::Selectable(display, is_selected, ImGuiSelectableFlags_AllowOverlap)) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl) {
                // Toggle selection
                if (vi < m_selected.size()) m_selected[vi] = !m_selected[vi];
            } else if (io.KeyShift && m_last_clicked >= 0) {
                // Range selection
                int start = std::min(m_last_clicked, static_cast<int>(vi));
                int end = std::max(m_last_clicked, static_cast<int>(vi));
                for (int j = start; j <= end && j < static_cast<int>(m_selected.size()); ++j) {
                    m_selected[j] = true;
                }
            } else {
                // Single selection - clear others
                std::fill(m_selected.begin(), m_selected.end(), false);
                if (vi < m_selected.size()) m_selected[vi] = true;
            }
            m_last_clicked = static_cast<int>(vi);
        }
        ImGui::PopStyleColor();

        // Right-click context menu
        if (ImGui::BeginPopupContextItem("LogContextMenu")) {
            if (ImGui::MenuItem("Copy")) {
                ImGui::SetClipboardText(entry.message.c_str());
            }
            if (ImGui::MenuItem("Copy Selected")) {
                std::vector<LogEntry> selected_entries;
                for (size_t si = 0; si < visible.size(); ++si) {
                    if (si < m_selected.size() && m_selected[si]) {
                        selected_entries.push_back(*visible[si].entry);
                    }
                }
                if (!selected_entries.empty()) {
                    copy_selected_to_clipboard(selected_entries);
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // Auto-scroll to bottom
    if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ConsolePanel::copy_selected_to_clipboard(const std::vector<LogEntry>& entries) {
    std::string text;
    for (const auto& entry : entries) {
        const char* level_str = "";
        switch (entry.level) {
            case LogEntry::Level::Info:    level_str = "[INFO]"; break;
            case LogEntry::Level::Warning: level_str = "[WARN]"; break;
            case LogEntry::Level::Error:   level_str = "[ERR]"; break;
        }
        if (!entry.source.empty()) {
            text += level_str;
            text += " [";
            text += entry.source;
            text += "] ";
        } else {
            text += level_str;
            text += " ";
        }
        text += entry.message;
        text += "\n";
    }
    if (!text.empty()) {
        ImGui::SetClipboardText(text.c_str());
    }
}

}