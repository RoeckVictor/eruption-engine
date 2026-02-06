#include "ProjectHubPanel.h"
#include "editor/core/ProjectManager.h"
#include "editor/core/EditorApplication.h"

#include <imgui.h>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace fs = std::filesystem;

namespace editor {

ProjectHubPanel::ProjectHubPanel(ProjectManager& project_manager, EditorApplication& app)
    : Panel("Project Hub")
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
}

void ProjectHubPanel::render_header() {
    ImGui::Spacing();
    ImGui::Spacing();

    // Title
    ImGui::PushFont(nullptr); // TODO: Use larger font
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.9f, 1.0f), "Eruption Editor");
    ImGui::PopFont();

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
            if (ImGui::MenuItem("Show in Explorer")) {
#ifdef _WIN32
                std::string cmd = "explorer \"" + path + "\"";
                system(cmd.c_str());
#endif
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

        // Set default path
#ifdef _WIN32
        const char* userprofile = std::getenv("USERPROFILE");
        if (userprofile) {
            std::string default_path = (fs::path(userprofile) / "Documents" / "EruptionProjects").string();
            strncpy(m_new_project_path, default_path.c_str(), sizeof(m_new_project_path) - 1);
        }
#else
        const char* home = std::getenv("HOME");
        if (home) {
            std::string default_path = (fs::path(home) / "EruptionProjects").string();
            strncpy(m_new_project_path, default_path.c_str(), sizeof(m_new_project_path) - 1);
        }
#endif
    }

    ImGui::SameLine();

    // Open Project button
    if (ImGui::Button("Open Project", ImVec2(button_width, button_height))) {
        std::string path = open_folder_dialog();
        if (!path.empty()) {
            if (ProjectManager::is_valid_project(path)) {
                if (m_project_manager.open_project(path)) {
                    m_app.on_project_loaded();
                }
            } else {
                // TODO: Show error message
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
            std::string path = open_folder_dialog();
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
                // TODO: Show error
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            m_show_new_dialog = false;
        }

        ImGui::EndPopup();
    }
}

std::string ProjectHubPanel::open_folder_dialog() {
#ifdef _WIN32
    std::string result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileDialog* pFileDialog = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_IFileDialog, reinterpret_cast<void**>(&pFileDialog));

        if (SUCCEEDED(hr)) {
            DWORD options;
            pFileDialog->GetOptions(&options);
            pFileDialog->SetOptions(options | FOS_PICKFOLDERS);

            hr = pFileDialog->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                hr = pFileDialog->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr)) {
                        // Convert wide string to narrow string
                        int size = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                        if (size > 0) {
                            result.resize(size - 1);
                            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, result.data(), size, nullptr, nullptr);
                        }
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pFileDialog->Release();
        }
        CoUninitialize();
    }

    return result;
#else
    // TODO: Implement for other platforms
    return "";
#endif
}

} // namespace editor
