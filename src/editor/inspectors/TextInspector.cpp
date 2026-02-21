#include "TextInspector.h"
#include "engine/render/Text.h"
#include <imgui.h>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace editor {

namespace fs = std::filesystem;

bool TextInspector::draw(engine::render::Text& component, const std::string& project_path) {
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

    // Text content (multiline)
    ImGui::Text("Text Content");

    static char text_buffer[4096];
    strncpy(text_buffer, component.content.c_str(), sizeof(text_buffer) - 1);
    text_buffer[sizeof(text_buffer) - 1] = '\0';

    if (ImGui::InputTextMultiline("##TextContent", text_buffer, sizeof(text_buffer),
                                   ImVec2(-1, 80))) {
        component.content = text_buffer;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Font path with asset picker
    ImGui::Text("Font");

    const char* display_text = component.font_path.empty() ?
                               "None (click to select)" :
                               component.font_path.c_str();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

    if (ImGui::Button(display_text, ImVec2(-1, 0))) {
        ImGui::OpenPopup("FontAssetPicker");
    }

    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to select a TTF or OTF font file");
    }

    // Handle drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string dropped_path((const char*)payload->Data, payload->DataSize);
            fs::path p(dropped_path);
            std::string ext = p.extension().string();
            if (ext == ".ttf" || ext == ".TTF" || ext == ".otf" || ext == ".OTF") {
                component.font_path = dropped_path;
                component._atlas_loaded = false;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    show_font_picker(component, project_path);

    ImGui::Spacing();

    // Font size
    if (ImGui::DragFloat("Font Size", &component.font_size, 1.0f, 1.0f, 200.0f)) {
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Color
    ImGui::Text("Color");
    float color[4] = { component.color_r, component.color_g, component.color_b, component.color_a };
    if (ImGui::ColorEdit4("##TextColor", color)) {
        component.color_r = color[0];
        component.color_g = color[1];
        component.color_b = color[2];
        component.color_a = color[3];
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Alignment
    ImGui::Text("Alignment");
    int align_idx = static_cast<int>(component.align);
    const char* align_items[] = { "Left", "Center", "Right" };
    if (ImGui::Combo("##Alignment", &align_idx, align_items, 3)) {
        component.align = static_cast<engine::render::TextAlign>(align_idx);
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

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

void TextInspector::show_font_picker(engine::render::Text& component, const std::string& project_path) {
    if (ImGui::BeginPopup("FontAssetPicker")) {
        ImGui::Text("Select Font");
        ImGui::Separator();

        // Collect font files from valid asset folders
        std::vector<std::string> font_files;

        // Helper to scan a directory for fonts
        auto scan_directory = [&](const fs::path& dir) {
            if (!fs::exists(dir)) return;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        if (ext == ".ttf" || ext == ".TTF" || ext == ".otf" || ext == ".OTF") {
                            std::string path = entry.path().string();
                            std::replace(path.begin(), path.end(), '\\', '/');
                            font_files.push_back(path);
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

        // Scan engine assets folder (lowercase a) - for engine-provided fonts
        scan_directory("assets");

        std::sort(font_files.begin(), font_files.end());

        if (font_files.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No TTF or OTF fonts found in Assets folder");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Add .ttf or .otf files to your project's Assets folder");
        } else {
            ImGui::BeginChild("FontList", ImVec2(400, 300), true);

            for (const auto& file : font_files) {
                fs::path p(file);
                std::string filename = p.filename().string();
                std::string folder = p.parent_path().string();

                ImGui::PushID(file.c_str());

                if (ImGui::Selectable(filename.c_str(), component.font_path == file)) {
                    component.font_path = file;
                    component._atlas_loaded = false;
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

        if (ImGui::Button("Clear")) {
            component.font_path.clear();
            component._atlas_loaded = false;
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
