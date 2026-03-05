#include "PixelGridComponentInspector.h"
#include "AssetPicker.h"
#include "InspectorUtils.h"
#include "engine/simulation/PixelGridComponent.h"
#include <imgui.h>

namespace editor {

// Pixel grid file extensions
static const std::vector<std::string> PXG_EXTENSIONS = {
    ".pxg"
};

bool PixelGridComponentInspector::draw(engine::simulation::PixelGridComponent& component, const std::string& project_path) {
    RecordingContext empty_ctx;
    return draw(component, project_path, empty_ctx);
}

bool PixelGridComponentInspector::draw(engine::simulation::PixelGridComponent& component, const std::string& project_path,
                                        const RecordingContext& rec_ctx) {
    bool changed = false;

    // Helper to check if a property is animated (for red background)
    auto is_animated = [&](const std::string& prop_name) {
        return rec_ctx.is_animated("PixelGridComponent." + prop_name);
    };

    // Helper to record a property change
    auto record_property = [&](const std::string& prop_name, const engine::animation::PropertyValue& value,
                               engine::animation::PropertyValueType type) {
        rec_ctx.record("PixelGridComponent." + prop_name, value, type);
    };

    // Cache animation state to ensure push/pop are balanced
    bool enabled_animated = is_animated("enabled");
    bool path_animated = is_animated("pixel_grid_path");
    bool destructible_animated = is_animated("destructible");

    // Enabled checkbox
    if (enabled_animated) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
    }
    if (EnabledCheckbox(&component.enabled)) {
        changed = true;
        record_property("enabled", component.enabled, engine::animation::PropertyValueType::Bool);
    }
    if (enabled_animated) {
        ImGui::PopStyleColor();
    }

    SectionSeparator();

    // Pixel Grid Path with asset picker
    ImGui::Text("Pixel Grid Asset");

    // Highlight if animated
    if (path_animated) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
    }

    AssetPickerConfig config;
    config.popup_id = "PixelGridAssetPicker";
    config.title = "Select Pixel Grid Asset (.pxg)";
    config.extensions = PXG_EXTENSIONS;
    config.empty_message = "No .pxg files found in Assets folder";
    config.clear_button_label = "Clear Selection";
    config.button_tooltip = "Click to select a .pxg file\nOr drag & drop from File Browser panel";

    auto result = AssetPicker::draw_button(component.pixel_grid_path, project_path, config);
    if (result.changed) {
        component.pixel_grid_path = result.selected_path;
        component.loaded = false;  // Mark for reload
        changed = true;
        record_property("pixel_grid_path", result.selected_path, engine::animation::PropertyValueType::String);
    }

    if (path_animated) {
        ImGui::PopStyleColor(2);
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(PXG_EXTENSIONS);
        if (!dropped.empty()) {
            component.pixel_grid_path = dropped;
            component.loaded = false;  // Mark for reload
            changed = true;
            record_property("pixel_grid_path", dropped, engine::animation::PropertyValueType::String);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::Spacing();

    // Destructible checkbox
    if (destructible_animated) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
    }
    if (ImGui::Checkbox("Destructible", &component.destructible)) {
        changed = true;
        record_property("destructible", component.destructible, engine::animation::PropertyValueType::Bool);
    }
    if (destructible_animated) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When enabled, pixels can be erased at runtime\n(e.g., by PixelBrush right-click)");
    }

    ImGui::Spacing();

    // Read-only info
    ImGui::BeginDisabled();
    ImGui::Text("Dimensions: %dx%d", component.width, component.height);
    ImGui::Checkbox("Loaded", &component.loaded);
    ImGui::EndDisabled();

    return changed;
}

}
