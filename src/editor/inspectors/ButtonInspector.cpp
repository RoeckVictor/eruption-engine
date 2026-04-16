#include "ButtonInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/ui/Button.h"
#include <imgui.h>

namespace editor {

// Audio file extensions
static const std::vector<std::string> SOUND_EXTENSIONS = {
    ".wav", ".ogg", ".mp3", ".WAV", ".OGG", ".MP3"
};

bool ButtonInspector::draw(engine::ui::Button& component, const std::string& project_path) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    SectionSeparator();

    // Click sound with asset picker
    ImGui::Text("Click Sound");

    AssetPickerConfig config;
    config.popup_id = "ClickSoundPicker";
    config.title = "Select Sound File";
    config.extensions = SOUND_EXTENSIONS;
    config.empty_message = "No audio files found in Assets folder";
    config.clear_button_label = "Clear";
    config.button_tooltip = "Click to select an audio file for click feedback\n"
                            "Or drag & drop from File Browser panel";

    auto result = AssetPicker::draw_button(component.click_sound, project_path, config);
    if (result.changed) {
        component.click_sound = result.selected_path;
        changed = true;
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(SOUND_EXTENSIONS);
        if (!dropped.empty()) {
            component.click_sound = dropped;
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

} // namespace editor
