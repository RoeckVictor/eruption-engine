#include "TextInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/render/Text.h"
#include <imgui.h>

namespace editor {

// Font file extensions
static const std::vector<std::string> FONT_EXTENSIONS = {
    ".ttf", ".otf", ".TTF", ".OTF"
};

bool TextInspector::draw(engine::render::Text& component, const std::string& project_path) {
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

    // Text content (multiline)
    ImGui::Text("Text Content");
    if (InputTextMultiline("##TextContent", &component.content, ImVec2(-1, 80))) {
        changed = true;
    }

    SectionSeparator();

    // Font path with asset picker
    ImGui::Text("Font");

    AssetPickerConfig config;
    config.popup_id = "FontAssetPicker";
    config.title = "Select Font";
    config.extensions = FONT_EXTENSIONS;
    config.empty_message = "No TTF or OTF fonts found in Assets folder";
    config.clear_button_label = "Clear";
    config.button_tooltip = "Click to select a TTF or OTF font file";

    auto result = AssetPicker::draw_button(component.font_path, project_path, config);
    if (result.changed) {
        component.font_path = result.selected_path;
        component._atlas_loaded = false;
        changed = true;
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(FONT_EXTENSIONS);
        if (!dropped.empty()) {
            component.font_path = dropped;
            component._atlas_loaded = false;
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::Spacing();

    // Font size
    if (ImGui::DragFloat("Font Size", &component.font_size, 1.0f, 1.0f, 200.0f)) {
        changed = true;
    }

    SectionSeparator();

    // Color
    ImGui::Text("Color");
    if (ColorEdit4("##TextColor", &component.color_r, &component.color_g,
                   &component.color_b, &component.color_a)) {
        changed = true;
    }

    SectionSeparator();

    // Horizontal Alignment
    ImGui::Text("Horizontal Align");
    int h_align_idx = static_cast<int>(component.h_align);
    const char* h_align_items[] = { "Left", "Center", "Right" };
    if (ImGui::Combo("##HAlignment", &h_align_idx, h_align_items, 3)) {
        component.h_align = static_cast<engine::render::TextHAlign>(h_align_idx);
        changed = true;
    }

    // Vertical Alignment
    ImGui::Text("Vertical Align");
    int v_align_idx = static_cast<int>(component.v_align);
    const char* v_align_items[] = { "Top", "Middle", "Bottom" };
    if (ImGui::Combo("##VAlignment", &v_align_idx, v_align_items, 3)) {
        component.v_align = static_cast<engine::render::TextVAlign>(v_align_idx);
        changed = true;
    }

    // Line height
    if (ImGui::DragFloat("Line Height", &component.line_height, 0.1f, 0.5f, 3.0f)) {
        changed = true;
    }

    // Max width (word wrap)
    if (ImGui::DragFloat("Max Width", &component.max_width, 1.0f, 0.0f, 1000.0f)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("0 = no wrapping");
    }

    SectionSeparator();

    // Style flags
    ImGui::Text("Style");
    if (ImGui::Checkbox("Bold", &component.bold)) {
        component._atlas_loaded = false;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Italic", &component.italic)) {
        component._atlas_loaded = false;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Underline", &component.underline)) {
        changed = true;
    }

    return changed;
}

}
