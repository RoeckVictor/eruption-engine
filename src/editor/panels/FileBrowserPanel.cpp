#include "FileBrowserPanel.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <functional>

namespace fs = std::filesystem;

namespace editor {

FileBrowserPanel::FileBrowserPanel()
    : Panel("File Browser")
{
}

void FileBrowserPanel::on_gui() {
    render_toolbar();
    ImGui::Separator();

    // Split into folder tree and file list
    float tree_width = 200.0f;

    ImGui::BeginChild("FolderTree", ImVec2(tree_width, 0), true);
    render_folder_tree();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("FileList", ImVec2(0, 0), true);
    render_file_list();
    ImGui::EndChild();
}

void FileBrowserPanel::set_root(const std::string& path) {
    m_root_path = path;
    m_current_path = path;
    refresh();
}

void FileBrowserPanel::render_toolbar() {
    // Back button
    if (ImGui::Button("<")) {
        if (m_current_path != m_root_path) {
            fs::path parent = fs::path(m_current_path).parent_path();
            if (parent.string().find(m_root_path) == 0) {
                navigate_to(parent.string());
            }
        }
    }

    ImGui::SameLine();

    // Refresh button
    if (ImGui::Button("Refresh")) {
        refresh();
    }

    ImGui::SameLine();

    // Current path display
    std::string display_path = m_current_path;
    if (!m_root_path.empty() && display_path.find(m_root_path) == 0) {
        display_path = display_path.substr(m_root_path.length());
        if (display_path.empty()) {
            display_path = "/";
        }
    }
    ImGui::TextDisabled("%s", display_path.c_str());

    ImGui::SameLine(ImGui::GetWindowWidth() - 210);

    // Filter
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Filter", "Filter...", m_filter, sizeof(m_filter));
}

void FileBrowserPanel::render_folder_tree() {
    if (m_root_path.empty()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    // Render folder tree recursively
    std::function<void(const fs::path&)> render_directory = [&](const fs::path& dir) {
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_directory()) continue;

                std::string name = entry.path().filename().string();

                // Skip hidden files/folders unless enabled
                if (!m_show_hidden && name[0] == '.') continue;

                // Skip Library folder
                if (name == "Library") continue;

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

                // Check if this is the current path
                if (entry.path().string() == m_current_path) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                bool is_open = ImGui::TreeNodeEx(name.c_str(), flags);

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    navigate_to(entry.path().string());
                }

                if (is_open) {
                    render_directory(entry.path());
                    ImGui::TreePop();
                }
            }
        } catch (const std::exception&) {
            // Ignore permission errors, etc.
        }
    };

    // Root node
    std::string root_name = fs::path(m_root_path).filename().string();
    ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                     ImGuiTreeNodeFlags_SpanAvailWidth |
                                     ImGuiTreeNodeFlags_DefaultOpen;

    if (m_current_path == m_root_path) {
        root_flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (ImGui::TreeNodeEx(root_name.c_str(), root_flags)) {
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            navigate_to(m_root_path);
        }
        render_directory(m_root_path);
        ImGui::TreePop();
    }
}

void FileBrowserPanel::render_file_list() {
    if (m_root_path.empty()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    std::string filter_lower(m_filter);
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

    for (const auto& entry : m_entries) {
        // Apply filter
        if (!filter_lower.empty()) {
            std::string name_lower = entry.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (name_lower.find(filter_lower) == std::string::npos) {
                continue;
            }
        }

        // Icon based on type
        const char* icon = entry.is_directory ? "[D]" : "[F]";

        ImGui::Text("%s", icon);
        ImGui::SameLine();

        bool is_selected = (entry.path == m_selected_file);
        if (ImGui::Selectable(entry.name.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            // Single click - select for preview
            if (!entry.is_directory) {
                select_file(entry.path);
            }

            // Double click - open
            if (ImGui::IsMouseDoubleClicked(0)) {
                if (entry.is_directory) {
                    navigate_to(entry.path);
                } else {
                    // TODO: Open file in appropriate editor
                }
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Open")) {
                if (entry.is_directory) {
                    navigate_to(entry.path);
                } else {
                    // TODO: Open file
                }
            }
            if (ImGui::MenuItem("Rename")) {
                // TODO: Rename dialog
            }
            if (ImGui::MenuItem("Delete")) {
                // TODO: Delete confirmation
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show in Explorer")) {
#ifdef _WIN32
                std::string cmd = "explorer /select,\"" + entry.path + "\"";
                system(cmd.c_str());
#endif
            }
            ImGui::EndPopup();
        }

        // Drag source for asset
        if (!entry.is_directory && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::Text("%s", entry.name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    // Right-click on empty space
    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                // TODO: Create folder dialog
            }
            if (ImGui::MenuItem("Scene")) {
                // TODO: Create scene
            }
            if (ImGui::MenuItem("Prefab")) {
                // TODO: Create prefab
            }
            if (ImGui::MenuItem("Script")) {
                // TODO: Create script dialog
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Refresh")) {
            refresh();
        }
        ImGui::EndPopup();
    }
}

void FileBrowserPanel::refresh() {
    m_entries.clear();

    if (m_current_path.empty()) {
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_current_path)) {
            std::string name = entry.path().filename().string();

            // Skip hidden files unless enabled
            if (!m_show_hidden && name[0] == '.') continue;

            // Skip Library folder
            if (name == "Library") continue;

            FileEntry fe;
            fe.name = name;
            fe.path = entry.path().string();
            fe.is_directory = entry.is_directory();
            m_entries.push_back(fe);
        }

        // Sort: directories first, then by name
        std::sort(m_entries.begin(), m_entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
            }
            return a.name < b.name;
        });
    } catch (const std::exception&) {
        // Ignore errors
    }
}

void FileBrowserPanel::navigate_to(const std::string& path) {
    m_current_path = path;
    refresh();
}

void FileBrowserPanel::select_file(const std::string& path) {
    m_selected_file = path;
    if (m_file_selected_callback) {
        m_file_selected_callback(path);
    }
}

} // namespace editor
