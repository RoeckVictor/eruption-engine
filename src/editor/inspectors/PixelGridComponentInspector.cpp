#include "PixelGridComponentInspector.h"
#include "engine/simulation/PixelGridComponent.h"
#include <imgui.h>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace editor {

namespace fs = std::filesystem;

bool PixelGridComponentInspector::draw(engine::simulation::PixelGridComponent& component) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Pixel Grid Path with asset picker
    ImGui::Text("Pixel Grid Asset");

    // Display current path (or "None" if empty)
    const char* display_text = component.pixel_grid_path.empty() ?
                               "None (click to select)" :
                               component.pixel_grid_path.c_str();

    // Make the field clickable
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

    if (ImGui::Button(display_text, ImVec2(-1, 0))) {
        ImGui::OpenPopup("PixelGridAssetPicker");
    }

    ImGui::PopStyleColor(3);

    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to select a .pxg file\nOr drag & drop from File Browser panel");
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string dropped_path((const char*)payload->Data, payload->DataSize);

            // Check if it's a .pxg file
            if (dropped_path.ends_with(".pxg")) {
                component.pixel_grid_path = dropped_path;
                component.loaded = false;  // Mark for reload
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Asset picker popup
    show_asset_picker(component);

    ImGui::Spacing();

    // Read-only info
    ImGui::BeginDisabled();
    ImGui::Text("Dimensions: %dx%d", component.width, component.height);
    ImGui::Checkbox("Loaded", &component.loaded);
    ImGui::EndDisabled();

    return changed;
}

void PixelGridComponentInspector::show_asset_picker(engine::simulation::PixelGridComponent& component) {
    if (ImGui::BeginPopup("PixelGridAssetPicker")) {
        ImGui::Text("Select Pixel Grid Asset (.pxg)");
        ImGui::Separator();

        // Collect all .pxg files from the project
        std::vector<std::string> pxg_files;

        try {
            // Search recursively from current directory
            for (const auto& entry : fs::recursive_directory_iterator(".")) {
                if (entry.is_regular_file() && entry.path().extension() == ".pxg") {
                    std::string path = entry.path().string();
                    // Normalize path separators
                    std::replace(path.begin(), path.end(), '\\', '/');
                    pxg_files.push_back(path);
                }
            }
        } catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error scanning files: %s", e.what());
        }

        // Sort alphabetically
        std::sort(pxg_files.begin(), pxg_files.end());

        // Display file list
        if (pxg_files.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No .pxg files found in project");
        } else {
            ImGui::BeginChild("AssetList", ImVec2(400, 300), true);

            for (const auto& file : pxg_files) {
                // Extract filename for display
                fs::path p(file);
                std::string filename = p.filename().string();
                std::string folder = p.parent_path().string();

                // Show file with folder info
                ImGui::PushID(file.c_str());

                if (ImGui::Selectable(filename.c_str(), component.pixel_grid_path == file)) {
                    component.pixel_grid_path = file;
                    component.loaded = false;  // Mark for reload
                    ImGui::CloseCurrentPopup();
                }

                // Show full path as tooltip
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", file.c_str());
                }

                // Show folder in gray
                if (!folder.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", folder.c_str());
                }

                ImGui::PopID();
            }

            ImGui::EndChild();
        }

        ImGui::Spacing();

        if (ImGui::Button("Clear Selection")) {
            component.pixel_grid_path.clear();
            component.loaded = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace editor
