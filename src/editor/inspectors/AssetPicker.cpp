#include "AssetPicker.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace editor {

namespace fs = std::filesystem;

bool AssetPicker::matches_extension(const std::string& ext, const std::vector<std::string>& extensions) {
    // Convert to lowercase for comparison
    std::string ext_lower = ext;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& allowed : extensions) {
        std::string allowed_lower = allowed;
        std::transform(allowed_lower.begin(), allowed_lower.end(), allowed_lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext_lower == allowed_lower) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> AssetPicker::scan_for_assets(
    const std::string& project_path,
    const std::vector<std::string>& extensions,
    const std::vector<std::string>& extra_dirs)
{
    std::vector<std::string> files;

    auto scan_directory = [&](const fs::path& dir) {
        if (!fs::exists(dir)) return;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (matches_extension(ext, extensions)) {
                        std::string path = entry.path().string();
                        std::replace(path.begin(), path.end(), '\\', '/');
                        files.push_back(path);
                    }
                }
            }
        } catch (const std::exception&) {
            // Ignore filesystem errors
        }
    };

    // Scan project Assets folder (capital A)
    if (!project_path.empty()) {
        scan_directory(fs::path(project_path) / "Assets");
    }

    // Scan engine assets folder (lowercase a)
    scan_directory("assets");

    // Scan extra directories
    for (const auto& dir : extra_dirs) {
        scan_directory(dir);
    }

    std::sort(files.begin(), files.end());
    return files;
}

AssetPickerResult AssetPicker::draw_button(
    const std::string& current_path,
    const std::string& project_path,
    const AssetPickerConfig& config)
{
    AssetPickerResult result;
    result.selected_path = current_path;

    // Display current path or placeholder
    const char* display_text = current_path.empty() ?
                               "None (click to select)" :
                               current_path.c_str();

    // Styled button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

    if (ImGui::Button(display_text, ImVec2(-1, 0))) {
        ImGui::OpenPopup(config.popup_id);
    }

    ImGui::PopStyleColor(3);

    // Tooltip
    if (config.button_tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", config.button_tooltip);
    }

    // Scan and show popup
    if (ImGui::BeginPopup(config.popup_id)) {
        auto files = scan_for_assets(project_path, config.extensions, config.extra_search_dirs);
        result = draw_popup(current_path, files, config);
        ImGui::EndPopup();
    }

    return result;
}

AssetPickerResult AssetPicker::draw_popup(
    const std::string& current_path,
    const std::vector<std::string>& files,
    const AssetPickerConfig& config)
{
    AssetPickerResult result;
    result.selected_path = current_path;

    ImGui::Text("%s", config.title);
    ImGui::Separator();

    if (files.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", config.empty_message);
    } else {
        ImGui::BeginChild("AssetList", ImVec2(config.popup_width, config.popup_height), true);

        for (const auto& file : files) {
            fs::path p(file);
            std::string filename = p.filename().string();
            std::string folder = p.parent_path().string();

            ImGui::PushID(file.c_str());

            if (ImGui::Selectable(filename.c_str(), current_path == file)) {
                result.selected_path = file;
                result.changed = true;
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

    // Clear button
    if (config.clear_button_label) {
        if (ImGui::Button(config.clear_button_label)) {
            result.selected_path.clear();
            result.changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }

    return result;
}

std::string accept_asset_drag_drop(const std::vector<std::string>& extensions) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
        std::string dropped_path((const char*)payload->Data, payload->DataSize);
        fs::path p(dropped_path);
        std::string ext = p.extension().string();
        if (AssetPicker::matches_extension(ext, extensions)) {
            return dropped_path;
        }
    }
    return "";
}

} // namespace editor
