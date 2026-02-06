#include "BuildSettingsPanel.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace editor {

BuildSettingsPanel::BuildSettingsPanel()
    : Panel("Build Settings")
{
    // Default product name
    std::strcpy(m_product_name_buffer, "MyGame");
}

void BuildSettingsPanel::on_open() {
}

void BuildSettingsPanel::on_close() {
}

void BuildSettingsPanel::on_gui() {
    if (m_project_path.empty()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    if (m_builder.is_building()) {
        render_build_progress();
    } else if (m_builder.status() == GameBuildStatus::Complete || m_builder.status() == GameBuildStatus::Failed) {
        render_build_complete();
    } else {
        render_build_settings();
    }
}

void BuildSettingsPanel::set_project_path(const std::string& path) {
    m_project_path = path;
    m_builder.set_project_path(path);

    // Set default output path to project/Builds/
    fs::path default_output = fs::path(path) / "Builds";
    std::strncpy(m_output_path_buffer, default_output.string().c_str(), sizeof(m_output_path_buffer) - 1);

    // Try to get project name from project.eruption
    fs::path project_file = fs::path(path) / "project.eruption";
    if (fs::exists(project_file)) {
        // TODO: Parse project file to get name
        // For now, use folder name
        std::string folder_name = fs::path(path).filename().string();
        std::strncpy(m_product_name_buffer, folder_name.c_str(), sizeof(m_product_name_buffer) - 1);
    }
}

void BuildSettingsPanel::set_engine_paths(const std::string& src_path, const std::string& build_path) {
    m_engine_src_path = src_path;
    m_engine_build_path = build_path;
    m_builder.set_engine_paths(src_path, build_path);
}

void BuildSettingsPanel::update() {
    m_builder.update();
}

void BuildSettingsPanel::render_build_settings() {
    ImGui::Text("Build Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Product Name
    ImGui::Text("Product Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ProductName", m_product_name_buffer, sizeof(m_product_name_buffer));
    ImGui::Spacing();

    // Output Path
    ImGui::Text("Output Directory:");
    ImGui::SetNextItemWidth(-60);
    ImGui::InputText("##OutputPath", m_output_path_buffer, sizeof(m_output_path_buffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
#ifdef _WIN32
        // Use Windows folder picker
        BROWSEINFOA bi = {};
        bi.lpszTitle = "Select Output Directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl != nullptr) {
            char path[MAX_PATH];
            if (SHGetPathFromIDListA(pidl, path)) {
                std::strncpy(m_output_path_buffer, path, sizeof(m_output_path_buffer) - 1);
            }
            CoTaskMemFree(pidl);
        }
#endif
    }
    ImGui::Spacing();

    // Build Options
    ImGui::Text("Options:");
    ImGui::Checkbox("Debug Build", &m_config.debug_build);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include debug symbols and enable logging");
    }

    ImGui::Checkbox("Include All Scenes", &m_config.include_editor_scenes);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Include all scenes in the project, not just the default");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Default Scene
    ImGui::Text("Default Scene:");
    ImGui::TextDisabled("(First scene loaded when game starts)");

    // List available scenes
    fs::path scenes_dir = fs::path(m_project_path) / "Assets" / "Scenes";
    if (fs::exists(scenes_dir)) {
        if (ImGui::BeginCombo("##DefaultScene", m_config.default_scene.empty() ? "(None)" : m_config.default_scene.c_str())) {
            for (const auto& entry : fs::directory_iterator(scenes_dir)) {
                if (entry.path().extension() == ".scene") {
                    std::string scene_name = entry.path().stem().string();
                    bool is_selected = (m_config.default_scene == scene_name);
                    if (ImGui::Selectable(scene_name.c_str(), is_selected)) {
                        m_config.default_scene = scene_name;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("No scenes found in Assets/Scenes/");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Build Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

    if (ImGui::Button("Build Game", ImVec2(-1, 40))) {
        // Validate settings
        if (strlen(m_output_path_buffer) == 0) {
            engine::Logger::instance().error("Build", "Output path is required");
        } else if (strlen(m_product_name_buffer) == 0) {
            engine::Logger::instance().error("Build", "Product name is required");
        } else {
            // Start build
            m_config.output_path = m_output_path_buffer;
            m_config.product_name = m_product_name_buffer;

            m_builder.start_build(m_config);
        }
    }

    ImGui::PopStyleColor(3);
}

void BuildSettingsPanel::render_build_progress() {
    ImGui::Text("Building...");
    ImGui::Separator();
    ImGui::Spacing();

    // Progress bar
    ImGui::ProgressBar(m_builder.progress(), ImVec2(-1, 0));
    ImGui::Spacing();

    // Current step
    ImGui::TextWrapped("%s", m_builder.current_step().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Cancel button (disabled for now - async build can't be cancelled easily)
    ImGui::BeginDisabled();
    if (ImGui::Button("Cancel", ImVec2(-1, 30))) {
        // TODO: Implement build cancellation
    }
    ImGui::EndDisabled();
}

void BuildSettingsPanel::render_build_complete() {
    bool success = (m_builder.status() == GameBuildStatus::Complete);

    if (success) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Build Completed Successfully!");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        ImGui::Text("Build Failed!");
        ImGui::PopStyleColor();

        if (!m_builder.error().empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("Error: %s", m_builder.error().c_str());
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Show output path
    ImGui::Text("Output: %s", m_output_path_buffer);

    ImGui::Spacing();

    // Buttons
    if (success) {
#ifdef _WIN32
        if (ImGui::Button("Open Output Folder", ImVec2(-1, 30))) {
            std::string cmd = "explorer \"" + std::string(m_output_path_buffer) + "\"";
            system(cmd.c_str());
        }
#endif
    }

    ImGui::Spacing();

    if (ImGui::Button("Build Again", ImVec2(-1, 30))) {
        // Reset builder to idle state
        m_builder.reset();
    }
}

} // namespace editor
