#include "UIInteractableInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/ui/UIInteractable.h"
#include <imgui.h>

namespace editor {

// Image file extensions for sprite pickers
static const std::vector<std::string> IMAGE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".PNG", ".JPG", ".JPEG"
};

/// Helper to draw a sprite picker with label
static bool draw_sprite_picker(const char* label, const char* popup_id,
                               std::string& sprite_path, const std::string& project_path) {
    ImGui::Text("%s", label);

    AssetPickerConfig config;
    config.popup_id = popup_id;
    config.title = "Select Sprite";
    config.extensions = IMAGE_EXTENSIONS;
    config.empty_message = "No image files found in Assets folder";
    config.clear_button_label = "Clear";
    config.button_tooltip = "Click to select an image for this state\n"
                            "Or drag & drop from File Browser panel";

    auto result = AssetPicker::draw_button(sprite_path, project_path, config);
    if (result.changed) {
        sprite_path = result.selected_path;
        return true;
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(IMAGE_EXTENSIONS);
        ImGui::EndDragDropTarget();
        if (!dropped.empty()) {
            sprite_path = dropped;
            return true;
        }
    }

    return false;
}

/// Helper to draw color picker with label using std::array
static bool draw_color_picker(const char* label, std::array<float, 4>& color) {
    ImGui::Text("%s", label);
    return ImGui::ColorEdit4("##color", color.data(), ImGuiColorEditFlags_NoLabel);
}

bool UIInteractableInspector::draw(engine::ui::UIInteractable& component, const std::string& project_path) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    // Interactable checkbox
    if (ImGui::Checkbox("Interactable", &component.interactable)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When disabled, the element won't respond to input\nbut still renders normally");
    }

    SectionSeparator();

    // Transition mode
    ImGui::Text("Visual Feedback");
    const char* transition_items[] = { "None", "Color Tint", "Sprite Swap" };
    int mode_idx = static_cast<int>(component.transition_mode);
    if (ImGui::Combo("Transition", &mode_idx, transition_items, 3)) {
        component.transition_mode = static_cast<engine::ui::UIInteractable::TransitionMode>(mode_idx);
        changed = true;
    }

    SectionSeparator();

    // Show relevant settings based on transition mode
    if (component.transition_mode == engine::ui::UIInteractable::TransitionMode::ColorTint) {
        ImGui::Text("State Colors");
        ImGui::Indent();

        ImGui::PushID("NormalColor");
        if (draw_color_picker("Normal", component.normal_color)) changed = true;
        ImGui::PopID();

        ImGui::PushID("HoveredColor");
        if (draw_color_picker("Hovered", component.hovered_color)) changed = true;
        ImGui::PopID();

        ImGui::PushID("PressedColor");
        if (draw_color_picker("Pressed", component.pressed_color)) changed = true;
        ImGui::PopID();

        ImGui::PushID("DisabledColor");
        if (draw_color_picker("Disabled", component.disabled_color)) changed = true;
        ImGui::PopID();

        ImGui::Unindent();
    }
    else if (component.transition_mode == engine::ui::UIInteractable::TransitionMode::SpriteSwap) {
        ImGui::Text("State Sprites");
        ImGui::Indent();

        ImGui::PushID("NormalSprite");
        if (draw_sprite_picker("Normal", "NormalSpritePicker", component.normal_sprite, project_path)) changed = true;
        ImGui::PopID();

        ImGui::PushID("HoveredSprite");
        if (draw_sprite_picker("Hovered", "HoveredSpritePicker", component.hovered_sprite, project_path)) changed = true;
        ImGui::PopID();

        ImGui::PushID("PressedSprite");
        if (draw_sprite_picker("Pressed", "PressedSpritePicker", component.pressed_sprite, project_path)) changed = true;
        ImGui::PopID();

        ImGui::PushID("DisabledSprite");
        if (draw_sprite_picker("Disabled", "DisabledSpritePicker", component.disabled_sprite, project_path)) changed = true;
        ImGui::PopID();

        ImGui::Unindent();
    }

    return changed;
}

} // namespace editor
