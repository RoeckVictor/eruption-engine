#include "ConsolePanel.h"
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
            LogEntry::Level level;
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
    if (ImGui::Button("Clear")) {
        clear();
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
    std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

    // Copy entries under lock to avoid holding lock during rendering
    std::vector<LogEntry> entries_copy;
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        entries_copy = m_entries;
    }

    for (size_t i = 0; i < entries_copy.size(); ++i) {
        const auto& entry = entries_copy[i];

        // Filter by level
        if (entry.level == LogEntry::Level::Info && !m_show_info) continue;
        if (entry.level == LogEntry::Level::Warning && !m_show_warnings) continue;
        if (entry.level == LogEntry::Level::Error && !m_show_errors) continue;

        // Filter by text
        if (!filter_str.empty()) {
            std::string message_lower = entry.message;
            std::transform(message_lower.begin(), message_lower.end(), message_lower.begin(), ::tolower);
            if (message_lower.find(filter_str) == std::string::npos) {
                continue;
            }
        }

        ImGui::PushID(static_cast<int>(i));

        // Icon and color based on level
        ImVec4 color;
        const char* icon;
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

        ImGui::TextColored(color, "%s", icon);
        ImGui::SameLine();

        // Show source tag
        if (!entry.source.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "[%s]", entry.source.c_str());
            ImGui::SameLine();
        }

        // Show count if collapsed
        if (entry.count > 1) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%d)", entry.count);
            ImGui::SameLine();
        }

        ImGui::TextWrapped("%s", entry.message.c_str());

        // Right-click to copy
        if (ImGui::BeginPopupContextItem("LogContextMenu")) {
            if (ImGui::MenuItem("Copy")) {
                ImGui::SetClipboardText(entry.message.c_str());
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

} // namespace editor
