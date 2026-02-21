#include "ImageInspector.h"
#include "engine/render/Image.h"
#include <imgui.h>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace editor {

namespace fs = std::filesystem;

static bool is_image_extension(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".PNG" || ext == ".JPG" || ext == ".JPEG";
}

bool ImageInspector::draw(engine::render::Image& component, const std::string& project_path) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    // Render layer
    if (ImGui::DragInt("Render Layer", &component.layer)) {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Sprite path with asset picker
    ImGui::Text("Sprite");

    // Display current path (or "None" for solid color)
    const char* display_text = component.sprite_path.empty() ?
                               "None (solid color)" :
                               component.sprite_path.c_str();

    // Make the field clickable
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

    if (ImGui::Button(display_text, ImVec2(-1, 0))) {
        ImGui::OpenPopup("SpriteAssetPicker");
    }

    ImGui::PopStyleColor(3);

    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to select an image file (.png, .jpg)\nOr drag & drop from File Browser panel\nLeave empty for solid color");
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string dropped_path((const char*)payload->Data, payload->DataSize);
            fs::path p(dropped_path);
            if (is_image_extension(p.extension().string())) {
                component.sprite_path = dropped_path;
                component._texture_loaded = false;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Asset picker popup
    show_asset_picker(component, project_path);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Color (RGBA)
    ImGui::Text("Color / Tint");
    float color[4] = { component.color_r, component.color_g, component.color_b, component.color_a };
    if (ImGui::ColorEdit4("##Color", color)) {
        component.color_r = color[0];
        component.color_g = color[1];
        component.color_b = color[2];
        component.color_a = color[3];
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

void ImageInspector::show_asset_picker(engine::render::Image& component, const std::string& project_path) {
    if (ImGui::BeginPopup("SpriteAssetPicker")) {
        ImGui::Text("Select Image Asset (.png, .jpg)");
        ImGui::Separator();

        // Collect image files from valid asset folders only
        std::vector<std::string> image_files;

        // Helper to scan a directory for images
        auto scan_directory = [&](const fs::path& dir) {
            if (!fs::exists(dir)) return;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (is_image_extension(ext)) {
                            std::string path = entry.path().string();
                            std::replace(path.begin(), path.end(), '\\', '/');
                            image_files.push_back(path);
                        }
                    }
                }
            } catch (const std::exception&) {
                // Ignore errors for individual directories
            }
        };

        // Scan project Assets folder (capital A)
        if (!project_path.empty()) {
            scan_directory(fs::path(project_path) / "Assets");
        }

        // Scan engine assets folder (lowercase a) - for engine-provided images
        scan_directory("assets");

        std::sort(image_files.begin(), image_files.end());

        if (image_files.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No image files found in Assets folder");
        } else {
            ImGui::BeginChild("AssetList", ImVec2(400, 300), true);

            for (const auto& file : image_files) {
                fs::path p(file);
                std::string filename = p.filename().string();
                std::string folder = p.parent_path().string();

                ImGui::PushID(file.c_str());

                if (ImGui::Selectable(filename.c_str(), component.sprite_path == file)) {
                    component.sprite_path = file;
                    component._texture_loaded = false;
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", file.c_str());
                }

                if (!folder.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", folder.c_str());
                }

                ImGui::PopID();
            }

            ImGui::EndChild();
        }

        ImGui::Spacing();

        if (ImGui::Button("Clear (Solid Color)")) {
            component.sprite_path.clear();
            component._texture_loaded = false;
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
