#include "ProjectSettingsPanel.h"
#include "editor/core/ProjectManager.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace editor {

ProjectSettingsPanel::ProjectSettingsPanel(ProjectManager& project_manager)
    : Panel("Project Settings")
    , m_project_manager(project_manager)
{
}

void ProjectSettingsPanel::on_open() {
    load_settings();
}

void ProjectSettingsPanel::on_close() {
    if (m_settings_changed) {
        // Auto-save on close
        save_settings();
    }
}

void ProjectSettingsPanel::on_gui() {
    if (!m_project_manager.has_project()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    // Tab bar for different settings categories
    if (ImGui::BeginTabBar("ProjectSettingsTabs")) {
        if (ImGui::BeginTabItem("General")) {
            render_general_settings();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Build")) {
            render_build_settings();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Physics")) {
            render_physics_settings();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Save/Revert buttons
    if (m_settings_changed) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
            save_settings();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (ImGui::Button("Revert", ImVec2(80, 0))) {
            load_settings();
            m_settings_changed = false;
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "* Unsaved changes");
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Save Settings", ImVec2(120, 0));
        ImGui::SameLine();
        ImGui::Button("Revert", ImVec2(80, 0));
        ImGui::EndDisabled();
    }
}

void ProjectSettingsPanel::render_general_settings() {
    ImGui::Spacing();

    ImGui::Text("Project Information");
    ImGui::Separator();
    ImGui::Spacing();

    // Project Name
    ImGui::Text("Project Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##ProjectName", m_project_name, sizeof(m_project_name))) {
        m_settings_changed = true;
    }

    ImGui::Spacing();

    // Company Name
    ImGui::Text("Company / Developer:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##CompanyName", m_company_name, sizeof(m_company_name))) {
        m_settings_changed = true;
    }

    ImGui::Spacing();

    // Version
    ImGui::Text("Version:");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##Version", m_version, sizeof(m_version))) {
        m_settings_changed = true;
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Default Scene");
    ImGui::Separator();
    ImGui::Spacing();

    // List available scenes
    fs::path scenes_dir = fs::path(m_project_manager.project_path()) / "Assets" / "Scenes";

    ImGui::Text("Scene loaded at startup:");
    ImGui::SetNextItemWidth(-1);

    if (fs::exists(scenes_dir)) {
        std::string current_scene = m_default_scene;
        if (ImGui::BeginCombo("##DefaultScene", current_scene.empty() ? "(None)" : current_scene.c_str())) {
            // Option for no default scene
            if (ImGui::Selectable("(None)", current_scene.empty())) {
                std::memset(m_default_scene, 0, sizeof(m_default_scene));
                m_settings_changed = true;
            }

            for (const auto& entry : fs::directory_iterator(scenes_dir)) {
                if (entry.path().extension() == ".scene") {
                    std::string scene_name = entry.path().stem().string();
                    bool is_selected = (current_scene == scene_name);
                    if (ImGui::Selectable(scene_name.c_str(), is_selected)) {
                        std::strncpy(m_default_scene, scene_name.c_str(), sizeof(m_default_scene) - 1);
                        m_settings_changed = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("No scenes folder found");
    }
}

void ProjectSettingsPanel::render_build_settings() {
    ImGui::Spacing();

    ImGui::Text("Window Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Resolution
    ImGui::Text("Default Resolution:");
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##Width", &m_window_width, 0, 0)) {
        m_window_width = std::max(320, std::min(7680, m_window_width));
        m_settings_changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("x");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##Height", &m_window_height, 0, 0)) {
        m_window_height = std::max(240, std::min(4320, m_window_height));
        m_settings_changed = true;
    }

    ImGui::Spacing();

    // Fullscreen
    if (ImGui::Checkbox("Fullscreen", &m_fullscreen)) {
        m_settings_changed = true;
    }

    // Resizable
    if (ImGui::Checkbox("Resizable Window", &m_resizable)) {
        m_settings_changed = true;
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Performance");
    ImGui::Separator();
    ImGui::Spacing();

    // Target FPS
    ImGui::Text("Target Frame Rate:");
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##TargetFPS", &m_target_fps, 0, 0)) {
        m_target_fps = std::max(30, std::min(240, m_target_fps));
        m_settings_changed = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(30-240)");

    // VSync
    if (ImGui::Checkbox("VSync", &m_vsync)) {
        m_settings_changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Synchronize frame rate with display refresh rate");
    }
}

void ProjectSettingsPanel::render_physics_settings() {
    ImGui::Spacing();

    ImGui::Text("Gravity");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("X:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("##GravityX", &m_gravity_x, 0.1f, -100.0f, 100.0f, "%.2f")) {
        m_settings_changed = true;
    }

    ImGui::SameLine();
    ImGui::Text("Y:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("##GravityY", &m_gravity_y, 0.1f, -100.0f, 100.0f, "%.2f")) {
        m_settings_changed = true;
    }

    ImGui::Spacing();

    // Quick presets
    ImGui::Text("Presets:");
    ImGui::SameLine();
    if (ImGui::Button("Earth")) {
        m_gravity_x = 0.0f;
        m_gravity_y = -9.81f;
        m_settings_changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Moon")) {
        m_gravity_x = 0.0f;
        m_gravity_y = -1.62f;
        m_settings_changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Zero-G")) {
        m_gravity_x = 0.0f;
        m_gravity_y = 0.0f;
        m_settings_changed = true;
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Text("Simulation");
    ImGui::Separator();
    ImGui::Spacing();

    // Physics iterations
    ImGui::Text("Solver Iterations:");
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("##PhysicsIterations", &m_physics_iterations, 1, 2)) {
        m_physics_iterations = std::max(1, std::min(32, m_physics_iterations));
        m_settings_changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Higher values = more accurate physics, but slower");
    }
}

void ProjectSettingsPanel::load_settings() {
    if (!m_project_manager.has_project()) {
        return;
    }

    const auto& info = m_project_manager.project_info();

    std::strncpy(m_project_name, info.name.c_str(), sizeof(m_project_name) - 1);
    std::strncpy(m_default_scene, info.default_scene.c_str(), sizeof(m_default_scene) - 1);

    // Load other settings from project file if available
    // For now, use defaults
    m_settings_changed = false;

    engine::Logger::instance().info("ProjectSettings", "Loaded project settings");
}

void ProjectSettingsPanel::save_settings() {
    if (!m_project_manager.has_project()) {
        return;
    }

    // Update project info
    auto& info = m_project_manager.project_info();
    info.name = m_project_name;
    info.default_scene = m_default_scene;

    // Save to project file
    if (m_project_manager.save_project()) {
        m_settings_changed = false;
        engine::Logger::instance().info("ProjectSettings", "Saved project settings");
    } else {
        engine::Logger::instance().error("ProjectSettings", "Failed to save project settings");
    }
}

} // namespace editor
