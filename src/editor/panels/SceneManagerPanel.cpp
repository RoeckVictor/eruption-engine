#include "SceneManagerPanel.h"
#include "editor/icons/IconsFontAwesome6.h"

#include <imgui.h>

namespace editor {

// NOTE: This panel is a placeholder UI shell for future scene management.
// The scene list (m_scenes) is never populated — all actions are stubs.
// Full implementation requires: scene discovery, load/save integration,
// rename/duplicate/delete file operations, and project settings updates.

SceneManagerPanel::SceneManagerPanel()
    : Panel("Scene Manager")
{
}

void SceneManagerPanel::on_gui() {
    // Toolbar
    if (ImGui::Button(ICON_FA_PLUS " New Scene")) {
        // Not implemented — needs scene creation dialog
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_ARROWS_ROTATE)) {
        // Not implemented — needs project asset scanning
    }

    ImGui::Separator();

    render_scene_list();
}

void SceneManagerPanel::render_scene_list() {
    ImGui::BeginChild("SceneList", ImVec2(0, 0), false);

    if (m_scenes.empty()) {
        ImGui::TextDisabled("No scenes in project");
        ImGui::TextDisabled("");
        ImGui::TextDisabled("Click 'New Scene' to create one,");
        ImGui::TextDisabled("or add .scene files to Assets/");
    }

    for (const auto& scene : m_scenes) {
        bool is_current = (scene == m_current_scene);

        ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;

        if (ImGui::Selectable(scene.c_str(), is_current, flags)) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                m_current_scene = scene;
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            render_context_menu(scene);
            ImGui::EndPopup();
        }

        // Show current scene indicator
        if (is_current) {
            ImGui::SameLine();
            ImGui::TextDisabled("(current)");
        }
    }

    ImGui::EndChild();
}

void SceneManagerPanel::render_context_menu(const std::string& scene_path) {
    if (ImGui::MenuItem("Open")) {
        m_current_scene = scene_path;
    }

    if (ImGui::MenuItem("Set as Default")) {
        // Not implemented
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Rename")) {
        // Not implemented
    }

    if (ImGui::MenuItem("Duplicate")) {
        // Not implemented
    }

    if (ImGui::MenuItem("Delete")) {
        // Not implemented
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Show in File Browser")) {
        // Not implemented
    }
}

}