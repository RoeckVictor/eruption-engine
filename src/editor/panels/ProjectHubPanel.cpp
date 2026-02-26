#include "ProjectHubPanel.h"
#include "editor/core/ProjectManager.h"
#include "editor/core/EditorApplication.h"

#include <imgui.h>
#include <filesystem>

#include "engine/platform/PlatformUtils.h"

namespace fs = std::filesystem;

namespace editor {

ProjectHubPanel::ProjectHubPanel(ProjectManager& project_manager, EditorApplication& app)
    : Panel("Project Hub", PanelVisibilityMode::Manual)
    , m_project_manager(project_manager)
    , m_app(app)
{
}

void ProjectHubPanel::on_gui() {
    // Center the content
    ImVec2 window_size = ImGui::GetWindowSize();
    float content_width = 600.0f;
    float padding = (window_size.x - content_width) * 0.5f;
    if (padding > 0) {
        ImGui::SetCursorPosX(padding);
    }

    ImGui::BeginChild("HubContent", ImVec2(content_width, 0), false);

    render_header();
    ImGui::Spacing();
    ImGui::Spacing();

    render_actions();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    render_recent_projects();

    ImGui::EndChild();

    // Modal dialogs
    render_new_project_dialog();
    render_error_dialog();
}

void ProjectHubPanel::render_header() {
    ImGui::Spacing();
    ImGui::Spacing();

    // Title (using font scale for larger text)
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 1.0f), "Eruption Editor");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::TextDisabled("Falling Sand Physics Engine");

    ImGui::Spacing();
}

void ProjectHubPanel::render_recent_projects() {
    ImGui::Text("Recent Projects");
    ImGui::Spacing();

    const auto& recent = m_project_manager.recent_projects();

    if (recent.empty()) {
        ImGui::TextDisabled("No recent projects");
        return;
    }

    for (size_t i = 0; i < recent.size(); ++i) {
        const std::string& path = recent[i];

        ImGui::PushID(static_cast<int>(i));

        // Project name (derived from path)
        fs::path fs_path(path);
        std::string name = fs_path.filename().string();

        // Clickable item
        if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0, 30))) {
            if (m_project_manager.open_project(path)) {
                m_app.on_project_loaded();
            }
        }

        // Show full path on hover
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", path.c_str());
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Open")) {
                if (m_project_manager.open_project(path)) {
                    m_app.on_project_loaded();
                }
            }
            if (ImGui::MenuItem("Remove from Recent")) {
                m_project_manager.remove_from_recent(path);
            }
            if (ImGui::MenuItem("Show in File Manager")) {
                engine::platform::open_folder_in_file_manager(path);
            }
            ImGui::EndPopup();
        }

        // Show path below name
        ImGui::SameLine();
        ImGui::TextDisabled("%s", path.c_str());

        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button("Clear Recent Projects")) {
        m_project_manager.clear_recent();
    }
}

void ProjectHubPanel::render_actions() {
    float button_width = 200.0f;
    float button_height = 40.0f;

    // New Project button
    if (ImGui::Button("New Project", ImVec2(button_width, button_height))) {
        m_show_new_dialog = true;

        // Set default path using platform utility
        std::string docs_dir = engine::platform::user_documents_directory();
        if (!docs_dir.empty()) {
            std::string default_path = (fs::path(docs_dir) / "EruptionProjects").string();
            strncpy(m_new_project_path, default_path.c_str(), sizeof(m_new_project_path) - 1);
        }
    }

    ImGui::SameLine();

    // Open Project button
    if (ImGui::Button("Open Project", ImVec2(button_width, button_height))) {
        std::string path = engine::platform::folder_dialog("Select Project Folder");
        if (!path.empty()) {
            if (ProjectManager::is_valid_project(path)) {
                if (m_project_manager.open_project(path)) {
                    m_app.on_project_loaded();
                }
            } else {
                m_error_message = "Invalid project folder. The selected folder does not contain a valid Eruption project.";
                m_show_error_dialog = true;
            }
        }
    }
}

void ProjectHubPanel::render_new_project_dialog() {
    if (!m_show_new_dialog) {
        return;
    }

    ImGui::OpenPopup("New Project");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("New Project", &m_show_new_dialog, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Project Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##ProjectName", m_new_project_name, sizeof(m_new_project_name));

        ImGui::Spacing();

        ImGui::Text("Location:");
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##ProjectPath", m_new_project_path, sizeof(m_new_project_path));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            std::string path = engine::platform::folder_dialog("Select Project Folder");
            if (!path.empty()) {
                strncpy(m_new_project_path, path.c_str(), sizeof(m_new_project_path) - 1);
            }
        }

        // Show full project path
        fs::path full_path = fs::path(m_new_project_path) / m_new_project_name;
        ImGui::TextDisabled("Project will be created at: %s", full_path.string().c_str());

        ImGui::Spacing();
        ImGui::Spacing();

        // Buttons
        float button_width = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_width) * 0.5f);

        if (ImGui::Button("Create", ImVec2(button_width, 0))) {
            std::string project_path = full_path.string();
            if (m_project_manager.create_project(project_path, m_new_project_name)) {
                m_show_new_dialog = false;
                m_app.on_project_loaded();
            } else {
                m_error_message = "Failed to create project. Check that the path is valid and writable.";
                m_show_error_dialog = true;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            m_show_new_dialog = false;
        }

        ImGui::EndPopup();
    }
}

void ProjectHubPanel::render_error_dialog() {
    if (!m_show_error_dialog) {
        return;
    }

    ImGui::OpenPopup("Error");

    // Center the modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Error", &m_show_error_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", m_error_message.c_str());
        ImGui::Spacing();
        ImGui::Spacing();

        float button_width = 100.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
        if (ImGui::Button("OK", ImVec2(button_width, 0))) {
            m_show_error_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}
