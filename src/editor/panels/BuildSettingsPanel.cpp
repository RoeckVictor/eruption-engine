#include "BuildSettingsPanel.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <nlohmann/json.hpp>

#include "engine/platform/PlatformUtils.h"

namespace fs = std::filesystem;

namespace editor {

BuildSettingsPanel::BuildSettingsPanel()
    : Panel("Build Settings", PanelVisibilityMode::OnDemand)
{
    // Default product name
    std::strncpy(m_product_name_buffer, "MyGame", sizeof(m_product_name_buffer) - 1);
    m_product_name_buffer[sizeof(m_product_name_buffer) - 1] = '\0';
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
    } else {
        render_build_settings();

        // Show build status section if there was a build result
        auto status = m_builder.status();
        if (status == GameBuildStatus::Complete ||
            status == GameBuildStatus::Cancelled ||
            status == GameBuildStatus::Failed) {
            render_build_status();
        }
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
    std::string product_name;
    if (fs::exists(project_file)) {
        std::ifstream file(project_file);
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                product_name = j.value("name", "");
            } catch (const std::exception&) {
                // Fall through to folder name fallback
            }
        }
    }
    if (product_name.empty()) {
        product_name = fs::path(path).filename().string();
    }
    std::strncpy(m_product_name_buffer, product_name.c_str(), sizeof(m_product_name_buffer) - 1);
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
        std::string path = engine::platform::folder_dialog("Select Output Directory");
        if (!path.empty()) {
            std::strncpy(m_output_path_buffer, path.c_str(), sizeof(m_output_path_buffer) - 1);
        }
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

    // List available scenes (recursive scan of Assets/)
    fs::path assets_dir = fs::path(m_project_path) / "Assets";
    if (fs::exists(assets_dir)) {
        if (ImGui::BeginCombo("##DefaultScene", m_config.default_scene.empty() ? "(None)" : m_config.default_scene.c_str())) {
            for (const auto& entry : fs::recursive_directory_iterator(assets_dir)) {
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
        ImGui::TextDisabled("No scenes found in Assets/");
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
            // Reset any previous build state and start new build
            m_builder.reset();
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

    if (ImGui::Button("Cancel", ImVec2(-1, 30))) {
        m_builder.request_cancel();
    }
}

void BuildSettingsPanel::render_build_status() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto status = m_builder.status();

    if (status == GameBuildStatus::Complete) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Completed Successfully!");
        ImGui::PopStyleColor();

        // Show output path (includes Debug/Release subfolder)
        const auto& actual_path = m_builder.actual_output_path();
        ImGui::Text("Output: %s", actual_path.empty() ? m_output_path_buffer : actual_path.c_str());

        ImGui::Spacing();

        if (ImGui::Button("Open Output Folder", ImVec2(-1, 30))) {
            engine::platform::open_folder_in_file_manager(
                actual_path.empty() ? m_output_path_buffer : actual_path.c_str());
        }
    } else if (status == GameBuildStatus::Cancelled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
        ImGui::Text("Build Cancelled");
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
}

}