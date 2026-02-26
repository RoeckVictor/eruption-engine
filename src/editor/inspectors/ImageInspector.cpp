#include "ImageInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/render/Image.h"
#include <imgui.h>

namespace editor {

// Image file extensions
static const std::vector<std::string> IMAGE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".PNG", ".JPG", ".JPEG"
};

bool ImageInspector::draw(engine::render::Image& component, const std::string& project_path) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    // Render layer
    if (RenderLayerInput(&component.layer)) {
        changed = true;
    }

    SectionSeparator();

    // Sprite path with asset picker
    ImGui::Text("Sprite");

    AssetPickerConfig config;
    config.popup_id = "SpriteAssetPicker";
    config.title = "Select Image Asset (.png, .jpg)";
    config.extensions = IMAGE_EXTENSIONS;
    config.empty_message = "No image files found in Assets folder";
    config.clear_button_label = "Clear (Solid Color)";
    config.button_tooltip = "Click to select an image file (.png, .jpg)\n"
                            "Or drag & drop from File Browser panel\n"
                            "Leave empty for solid color";

    auto result = AssetPicker::draw_button(component.sprite_path, project_path, config);
    if (result.changed) {
        component.sprite_path = result.selected_path;
        component._texture_loaded = false;
        changed = true;
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(IMAGE_EXTENSIONS);
        if (!dropped.empty()) {
            component.sprite_path = dropped;
            component._texture_loaded = false;
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    SectionSeparator();

    // Color (RGBA)
    ImGui::Text("Color / Tint");
    if (ColorEdit4("##Color", &component.color_r, &component.color_g,
                   &component.color_b, &component.color_a)) {
        changed = true;
    }

    ImGui::Spacing();

    // UV coordinates (collapsed by default)
    if (ImGui::CollapsingHeader("UV Coordinates")) {
        if (ImGui::DragFloat("UV Min X", &component.uv_min_x, 0.01f, 0.0f, 1.0f)) changed = true;
        if (ImGui::DragFloat("UV Min Y", &component.uv_min_y, 0.01f, 0.0f, 1.0f)) changed = true;
        if (ImGui::DragFloat("UV Max X", &component.uv_max_x, 0.01f, 0.0f, 1.0f)) changed = true;
        if (ImGui::DragFloat("UV Max Y", &component.uv_max_y, 0.01f, 0.0f, 1.0f)) changed = true;
    }

    ImGui::Spacing();

    // Flip options
    if (ImGui::Checkbox("Flip X", &component.flip_x)) changed = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Flip Y", &component.flip_y)) changed = true;

    return changed;
}

}
