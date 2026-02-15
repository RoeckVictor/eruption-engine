#include "editor/ui/UnsavedDialog.h"
#include <imgui.h>

namespace editor::ui {

UnsavedAction render_unsaved_popup(const char* popup_id, const char* message) {
    UnsavedAction result = UnsavedAction::None;

    if (ImGui::BeginPopupModal(popup_id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(message);
        ImGui::Text("Do you want to save before continuing?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            result = UnsavedAction::Save;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            result = UnsavedAction::DontSave;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            result = UnsavedAction::Cancel;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return result;
}

} // namespace editor::ui
