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
    bool changed = false;

    // Enabled checkbox
    if (EnabledCheckbox(&component.enabled)) {
        changed = true;
    }

    SectionSeparator();

    // Pixel Grid Path with asset picker
    ImGui::Text("Pixel Grid Asset");

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
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        std::string dropped = accept_asset_drag_drop(PXG_EXTENSIONS);
        if (!dropped.empty()) {
            component.pixel_grid_path = dropped;
            component.loaded = false;  // Mark for reload
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::Spacing();

    // Destructible checkbox
    if (ImGui::Checkbox("Destructible", &component.destructible)) {
        changed = true;
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
