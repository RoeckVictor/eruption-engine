#include "FileBrowserPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/core/Logger.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/platform/PlatformUtils.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;

namespace editor {

FileBrowserPanel::FileBrowserPanel()
    : Panel("File Browser")
{
}

void FileBrowserPanel::on_gui() {
    render_toolbar();
    ImGui::Separator();

    // Split into folder tree and file list
    float tree_width = 200.0f;

    ImGui::BeginChild("FolderTree", ImVec2(tree_width, 0), true);
    render_folder_tree();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("FileList", ImVec2(0, 0), true);
    render_file_list();
    ImGui::EndChild();

    // Delete confirmation modal (rendered outside children)
    if (!m_pending_delete_path.empty()) {
        // Check if this is a prefab file and count usage
        fs::path del_path(m_pending_delete_path);
        if (del_path.extension() == ".prefab") {
            m_pending_delete_prefab_usage = count_prefab_instances(m_pending_delete_path);
        } else {
            m_pending_delete_prefab_usage = 0;
        }
        ImGui::OpenPopup("Confirm Delete");
    }

    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string filename = fs::path(m_pending_delete_path).filename().string();
        ImGui::Text("Are you sure you want to delete \"%s\"?", filename.c_str());

        // Show prefab usage warning
        if (m_pending_delete_prefab_usage > 0) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
            ImGui::Text("Warning: %d entity instance(s) in the scene are linked to this prefab.",
                        m_pending_delete_prefab_usage);
            ImGui::Text("They will be automatically unlinked.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Text("This cannot be undone.");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            // Unlink prefab instances before deleting
            if (m_pending_delete_prefab_usage > 0) {
                unlink_prefab_instances(m_pending_delete_path);
            }
            perform_delete(m_pending_delete_path);
            m_pending_delete_path.clear();
            m_pending_delete_prefab_usage = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pending_delete_path.clear();
            m_pending_delete_prefab_usage = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FileBrowserPanel::set_root(const std::string& path) {
    m_root_path = path;
    m_current_path = path;
    refresh();
}

void FileBrowserPanel::render_toolbar() {
    // Back button
    if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
        if (m_current_path != m_root_path) {
            fs::path parent = fs::path(m_current_path).parent_path();
            if (parent.string().find(m_root_path) == 0) {
                navigate_to(parent.string());
            }
        }
    }

    ImGui::SameLine();

    // Refresh button
    if (ImGui::Button(ICON_FA_ARROWS_ROTATE)) {
        refresh();
    }

    ImGui::SameLine();

    // Current path display
    std::string display_path = m_current_path;
    if (!m_root_path.empty() && display_path.find(m_root_path) == 0) {
        display_path = display_path.substr(m_root_path.length());
        if (display_path.empty()) {
            display_path = "/";
        }
    }
    ImGui::TextDisabled("%s", display_path.c_str());

    ImGui::SameLine(ImGui::GetWindowWidth() - 210);

    // Filter
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Filter", "Filter...", m_filter, sizeof(m_filter));
}

void FileBrowserPanel::render_folder_tree() {
    if (m_root_path.empty()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    // Helper lambda for tree node context menu
    auto render_folder_context_menu = [&](const std::string& folder_path) {
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Copy")) {
                m_clipboard_path = folder_path;
                m_clipboard_is_cut = false;
            }
            if (ImGui::MenuItem("Cut")) {
                m_clipboard_path = folder_path;
                m_clipboard_is_cut = true;
            }
            ImGui::Separator();

            bool can_paste = !m_clipboard_path.empty() && fs::exists(m_clipboard_path);
            if (ImGui::MenuItem("Paste", nullptr, false, can_paste)) {
                perform_paste(folder_path);
            }
            ImGui::Separator();

            // Don't allow rename/delete of root
            bool is_root = (folder_path == m_root_path);

            if (ImGui::MenuItem("Rename", nullptr, false, !is_root)) {
                m_rename_target = folder_path;
                std::string name = fs::path(folder_path).filename().string();
                std::strncpy(m_rename_buffer, name.c_str(), sizeof(m_rename_buffer) - 1);
                m_rename_buffer[sizeof(m_rename_buffer) - 1] = '\0';
                m_rename_focus_set = false;
            }
            if (ImGui::MenuItem("Delete", nullptr, false, !is_root)) {
                m_pending_delete_path = folder_path;
            }
            ImGui::EndPopup();
        }
    };

    // Helper lambda for folder drop target
    auto render_folder_drop_target = [&](const std::string& folder_path) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string src_path(static_cast<const char*>(payload->Data));
                // Don't allow dropping a folder onto itself
                if (src_path != folder_path) {
                    perform_move(src_path, folder_path);
                }
            }
            ImGui::EndDragDropTarget();
        }
    };

    // Render folder tree recursively
    std::function<void(const fs::path&)> render_directory = [&](const fs::path& dir) {
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_directory()) continue;

                std::string name = entry.path().filename().string();
                if (name.empty()) continue;
                if (name[0] == '.' && !m_show_hidden) continue;
                if (name == "Library") continue;

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (entry.path().string() == m_current_path) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                bool is_open = ImGui::TreeNodeEx(name.c_str(), flags);

                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    navigate_to(entry.path().string());
                }

                render_folder_context_menu(entry.path().string());
                render_folder_drop_target(entry.path().string());

                if (is_open) {
                    render_directory(entry.path());
                    ImGui::TreePop();
                }
            }
        } catch (const std::exception&) {
            // Ignore permission errors
        }
    };

    // Root node
    std::string root_name = fs::path(m_root_path).filename().string();
    ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                     ImGuiTreeNodeFlags_SpanAvailWidth |
                                     ImGuiTreeNodeFlags_DefaultOpen;

    if (m_current_path == m_root_path) {
        root_flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (ImGui::TreeNodeEx(root_name.c_str(), root_flags)) {
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            navigate_to(m_root_path);
        }
        render_folder_context_menu(m_root_path);
        render_folder_drop_target(m_root_path);
        render_directory(m_root_path);
        ImGui::TreePop();
    }
}

void FileBrowserPanel::render_file_list() {
    if (m_root_path.empty()) {
        ImGui::TextDisabled("No project loaded");
        return;
    }

    // Handle keyboard shortcuts
    handle_keyboard_shortcuts();

    std::string filter_lower(m_filter);
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    // Deferred actions (don't modify state while iterating)
    std::string deferred_navigate_path;

    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        const auto& entry = m_entries[i];

        // Apply filter
        if (!filter_lower.empty()) {
            std::string name_lower = entry.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            if (name_lower.find(filter_lower) == std::string::npos) {
                continue;
            }
        }

        ImGui::PushID(i);

        // Icon based on type
        const char* icon = entry.is_directory ? ICON_FA_FOLDER : ICON_FA_FILE;
        ImGui::TextColored(entry.is_directory ? ImVec4(0.9f, 0.75f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", icon);
        ImGui::SameLine();

        // Inline rename mode
        if (m_rename_target == entry.path) {
            if (!m_rename_focus_set) {
                ImGui::SetKeyboardFocusHere();
                m_rename_focus_set = true;
            }

            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##rename", m_rename_buffer, sizeof(m_rename_buffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                // Enter pressed
                std::string new_name(m_rename_buffer);
                if (!new_name.empty()) {
                    perform_rename(m_rename_target, new_name);
                }
                m_rename_target.clear();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_rename_target.clear();
            }
            // Cancel on click elsewhere (lost focus, but not on the first frame)
            if (m_rename_focus_set && !ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
                // Small delay: only cancel if we already had focus once
                static int frames_active = 0;
                frames_active++;
                if (frames_active > 2) {
                    m_rename_target.clear();
                    frames_active = 0;
                }
            }
        } else {
            // Normal display
            bool is_selected = (entry.path == m_selected_file);
            if (ImGui::Selectable(entry.name.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                // Single click - select
                if (!entry.is_directory) {
                    select_file(entry.path);
                } else {
                    m_selected_file = entry.path;
                }

                // Double click - open
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (entry.is_directory) {
                        deferred_navigate_path = entry.path;
                    } else if (m_file_opened_callback) {
                        m_file_opened_callback(entry.path);
                    }
                }
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Open")) {
                if (entry.is_directory) {
                    deferred_navigate_path = entry.path;
                } else if (m_file_opened_callback) {
                    m_file_opened_callback(entry.path);
                }
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Copy")) {
                m_clipboard_path = entry.path;
                m_clipboard_is_cut = false;
            }
            if (ImGui::MenuItem("Cut")) {
                m_clipboard_path = entry.path;
                m_clipboard_is_cut = true;
            }

            // Paste into folder (only for directories)
            if (entry.is_directory) {
                bool can_paste = !m_clipboard_path.empty() && fs::exists(m_clipboard_path);
                if (ImGui::MenuItem("Paste", nullptr, false, can_paste)) {
                    perform_paste(entry.path);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Rename")) {
                m_rename_target = entry.path;
                std::strncpy(m_rename_buffer, entry.name.c_str(), sizeof(m_rename_buffer) - 1);
                m_rename_buffer[sizeof(m_rename_buffer) - 1] = '\0';
                m_rename_focus_set = false;
            }
            if (ImGui::MenuItem("Delete")) {
                m_pending_delete_path = entry.path;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Show in File Manager")) {
                engine::platform::reveal_in_file_manager(entry.path);
            }
            ImGui::EndPopup();
        }

        // Drag source for files and directories
        // Use ASSET_PATH as the universal payload type for both internal and external drag-drop
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::Text("%s %s", entry.is_directory ? ICON_FA_FOLDER : ICON_FA_FILE, entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target for directories (accept files/folders being dropped onto them)
        if (entry.is_directory && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string src_path(static_cast<const char*>(payload->Data));
                // Don't allow dropping a folder onto itself
                if (src_path != entry.path) {
                    perform_move(src_path, entry.path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }

    // Inline create folder
    if (m_creating_folder) {
        ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.0f), "%s", ICON_FA_FOLDER);
        ImGui::SameLine();

        if (!m_create_focus_set) {
            ImGui::SetKeyboardFocusHere();
            m_create_focus_set = true;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##createfolder", m_create_buffer, sizeof(m_create_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string name(m_create_buffer);
            if (!name.empty()) {
                try {
                    fs::create_directories(fs::path(m_current_path) / name);
                } catch (const std::exception& e) {
                    engine::Logger::instance().error("FileBrowser", "Failed to create folder: %s", e.what());
                }
                refresh();
            }
            m_creating_folder = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_creating_folder = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
    }

    // Inline create scene
    if (m_creating_scene) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", ICON_FA_FILE);
        ImGui::SameLine();

        if (!m_create_focus_set) {
            ImGui::SetKeyboardFocusHere();
            m_create_focus_set = true;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##createscene", m_create_buffer, sizeof(m_create_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string name(m_create_buffer);
            if (!name.empty()) {
                // Ensure .scene extension
                if (name.size() < 6 || name.substr(name.size() - 6) != ".scene") {
                    name += ".scene";
                }
                try {
                    fs::path scene_path = fs::path(m_current_path) / name;
                    std::ofstream file(scene_path);
                    if (file.is_open()) {
                        std::string scene_name = fs::path(name).stem().string();
                        file << "{\n";
                        file << "  \"name\": \"" << scene_name << "\",\n";
                        file << "  \"guid\": \"\",\n";
                        file << "  \"settings\": {\n";
                        file << "    \"gravity\": [0.0, -9.81],\n";
                        file << "    \"pixelsPerMeter\": 16.0,\n";
                        file << "    \"physicsSubsteps\": 4,\n";
                        file << "    \"backgroundColor\": [0.1, 0.1, 0.15, 1.0]\n";
                        file << "  },\n";
                        file << "  \"entities\": []\n";
                        file << "}\n";
                    }
                } catch (const std::exception& e) {
                    engine::Logger::instance().error("FileBrowser", "Failed to create scene: %s", e.what());
                }
                refresh();
            }
            m_creating_scene = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_creating_scene = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
    }

    // Inline create pixel grid
    if (m_creating_pxg) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", ICON_FA_FILE);
        ImGui::SameLine();

        if (!m_create_focus_set) {
            ImGui::SetKeyboardFocusHere();
            m_create_focus_set = true;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##createpxg", m_create_buffer, sizeof(m_create_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string name(m_create_buffer);
            if (!name.empty()) {
                // Ensure .pxg extension
                if (name.size() < 4 || name.substr(name.size() - 4) != ".pxg") {
                    name += ".pxg";
                }
                try {
                    fs::path pxg_path = fs::path(m_current_path) / name;
                    // Create a default 16x16 .pxg with color + material layers
                    const uint32_t w = 16, h = 16;
                    const uint32_t num_channels = 5; // RGBA + material
                    engine::asset::PxgChannelDesc descs[5];
                    std::strncpy(descs[0].name, "color_r", sizeof(descs[0].name));
                    std::strncpy(descs[1].name, "color_g", sizeof(descs[1].name));
                    std::strncpy(descs[2].name, "color_b", sizeof(descs[2].name));
                    std::strncpy(descs[3].name, "color_a", sizeof(descs[3].name));
                    std::strncpy(descs[4].name, "material", sizeof(descs[4].name));

                    // All zeros = transparent, material 0 (air)
                    std::vector<uint8_t> pixels(w * h * num_channels, 0);

                    // Metadata with layer definitions
                    nlohmann::json meta;
                    meta["origin"] = { {"x", 0}, {"y", 0} };
                    meta["layers"] = nlohmann::json::array({
                        { {"name", "color"}, {"opacity", 1.0}, {"visible", true}, {"type", "color"}, {"engine_required", true} },
                        { {"name", "material"}, {"opacity", 1.0}, {"visible", true}, {"type", "enum"}, {"engine_required", true},
                          {"values", {"air", "rock", "dirt", "sand", "water", "lava", "ice", "steam", "fire", "explosive"}} }
                    });

                    engine::asset::pxg_save(pxg_path.string(), w, h, descs, num_channels, pixels.data(), meta.dump());
                } catch (const std::exception& e) {
                    engine::Logger::instance().error("FileBrowser", "Failed to create pixel grid: %s", e.what());
                }
                refresh();
            }
            m_creating_pxg = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_creating_pxg = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
    }

    // Inline create prefab
    if (m_creating_prefab) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", ICON_FA_FILE);
        ImGui::SameLine();

        if (!m_create_focus_set) {
            ImGui::SetKeyboardFocusHere();
            m_create_focus_set = true;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##createprefab", m_create_buffer, sizeof(m_create_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string name(m_create_buffer);
            if (!name.empty()) {
                // Ensure .prefab extension
                if (name.size() < 7 || name.substr(name.size() - 7) != ".prefab") {
                    name += ".prefab";
                }
                try {
                    fs::path prefab_path = fs::path(m_current_path) / name;
                    std::string prefab_name = fs::path(name).stem().string();

                    nlohmann::json json;
                    json["name"] = prefab_name;
                    json["components"] = nlohmann::json::array({
                        { {"type", "engine::Transform"}, {"data", { {"x", 0.0}, {"y", 0.0}, {"rotation", 0.0}, {"scale_x", 1.0}, {"scale_y", 1.0} }} }
                    });

                    std::ofstream file(prefab_path);
                    if (file.is_open()) {
                        file << json.dump(2);
                    }
                } catch (const std::exception& e) {
                    engine::Logger::instance().error("FileBrowser", "Failed to create prefab: %s", e.what());
                }
                refresh();
            }
            m_creating_prefab = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_creating_prefab = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
    }

    // Inline script creation
    if (m_creating_script) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.4f, 1.0f), "%s", ICON_FA_CODE);
        ImGui::SameLine();

        if (!m_create_focus_set) {
            ImGui::SetKeyboardFocusHere();
            m_create_focus_set = true;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##createscript", m_create_buffer, sizeof(m_create_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string name(m_create_buffer);
            if (!name.empty()) {
                // Strip any extension the user may have typed
                fs::path name_path(name);
                std::string class_name = name_path.stem().string();

                // Create in the current directory
                fs::path header_path = fs::path(m_current_path) / (class_name + ".h");
                fs::path source_path = fs::path(m_current_path) / (class_name + ".cpp");

                try {
                    // Generate header
                    {
                        std::ofstream file(header_path);
                        if (file.is_open()) {
                            file << "#pragma once\n\n";
                            file << "#include \"runtime/ComponentScript.h\"\n";
                            file << "#include \"engine/core/Transform.h\"\n\n";
                            file << "class " << class_name << " : public runtime::ComponentScript {\n";
                            file << "public:\n";
                            file << "    const char* type_name() const override { return \"" << class_name << "\"; }\n\n";
                            file << "    void on_create() override {\n";
                            file << "        // Called when the script is first created\n";
                            file << "    }\n\n";
                            file << "    void on_destroy() override {\n";
                            file << "        // Called when the script is about to be destroyed\n";
                            file << "    }\n\n";
                            file << "    void on_update() override {\n";
                            file << "        // float dt = delta_time();\n";
                            file << "        // Called every frame\n";
                            file << "    }\n\n";
                            file << "private:\n";
                            file << "};\n";
                        }
                    }

                    // Generate source
                    {
                        std::ofstream file(source_path);
                        if (file.is_open()) {
                            file << "#include \"" << class_name << ".h\"\n\n";
                            file << "REGISTER_COMPONENT_SCRIPT(" << class_name << ")\n";
                        }
                    }

                    engine::Logger::instance().info("FileBrowser",
                        "Created script: %s (.h + .cpp)", class_name.c_str());
                } catch (const std::exception& e) {
                    engine::Logger::instance().error("FileBrowser",
                        "Failed to create script: %s", e.what());
                }
                refresh();
            }
            m_creating_script = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_creating_script = false;
            std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
        }
    }

    // Accept entity drag-drop to create prefab
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            if (payload->DataSize == sizeof(entt::entity) && m_editor_context) {
                entt::entity entity = *static_cast<entt::entity*>(payload->Data);
                auto* registry = m_editor_context->registry();

                if (registry && registry->valid(entity)) {
                    // Get entity name for prefab filename
                    std::string prefab_name = "NewPrefab";
                    if (registry->all_of<EntityInfo>(entity)) {
                        prefab_name = registry->get<EntityInfo>(entity).name;
                    }

                    // Create prefab file in current directory
                    fs::path prefab_path = fs::path(m_current_path) / (prefab_name + ".prefab");

                    // Handle name collision by appending number
                    int counter = 1;
                    while (fs::exists(prefab_path)) {
                        prefab_path = fs::path(m_current_path) / (prefab_name + "_" + std::to_string(counter++) + ".prefab");
                    }

                    SceneSerializer serializer(*registry);
                    if (serializer.save_prefab(prefab_path, entity)) {
                        // Update entity to be a prefab instance
                        // Ensure EntityInfo exists (add if missing)
                        if (!registry->all_of<EntityInfo>(entity)) {
                            registry->emplace<EntityInfo>(entity, EntityInfo{prefab_name});
                        }
                        auto& info = registry->get<EntityInfo>(entity);
                        info.is_prefab_instance = true;
                        info.prefab_path = prefab_path.string();

                        m_editor_context->scene_state().mark_dirty();
                        refresh();
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click on empty space
    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                m_creating_folder = true;
                m_creating_scene = false;
                m_creating_pxg = false;
                m_creating_prefab = false;
                m_creating_script = false;
                std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
                m_create_focus_set = false;
            }
            if (ImGui::MenuItem("Scene")) {
                m_creating_scene = true;
                m_creating_folder = false;
                m_creating_pxg = false;
                m_creating_prefab = false;
                m_creating_script = false;
                std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
                m_create_focus_set = false;
            }
            if (ImGui::MenuItem("Pixel Grid (.pxg)")) {
                m_creating_pxg = true;
                m_creating_folder = false;
                m_creating_scene = false;
                m_creating_prefab = false;
                m_creating_script = false;
                std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
                m_create_focus_set = false;
            }
            if (ImGui::MenuItem("Prefab (.prefab)")) {
                m_creating_prefab = true;
                m_creating_folder = false;
                m_creating_scene = false;
                m_creating_pxg = false;
                m_creating_script = false;
                std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
                m_create_focus_set = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Script (.h/.cpp)")) {
                m_creating_script = true;
                m_creating_folder = false;
                m_creating_scene = false;
                m_creating_pxg = false;
                m_creating_prefab = false;
                std::memset(m_create_buffer, 0, sizeof(m_create_buffer));
                m_create_focus_set = false;
            }
            if (ImGui::MenuItem("Material (.material)")) {
                // Use Material Editor panel for proper material creation
                if (m_material_create_callback) {
                    m_material_create_callback();
                }
            }
            if (ImGui::MenuItem("Physical Category (.phys)")) {
                // Use Material Editor panel for proper category creation
                if (m_category_create_callback) {
                    m_category_create_callback();
                }
            }
            ImGui::EndMenu();
        }

        bool can_paste = !m_clipboard_path.empty() && fs::exists(m_clipboard_path);
        if (ImGui::MenuItem("Paste", nullptr, false, can_paste)) {
            perform_paste(m_current_path);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh")) {
            refresh();
        }
        ImGui::EndPopup();
    }

    // Perform deferred navigation now that the loop is done
    if (!deferred_navigate_path.empty()) {
        navigate_to(deferred_navigate_path);
    }
}

void FileBrowserPanel::refresh() {
    m_entries.clear();

    if (m_current_path.empty()) {
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_current_path)) {
            std::string name = entry.path().filename().string();
            if (name.empty()) continue;
            if (name[0] == '.' && !m_show_hidden) continue;
            if (name == "Library") continue;

            // Hide .meta sidecar files (asset registry metadata)
            if (name.size() > 5 && name.substr(name.size() - 5) == ".meta") continue;

            FileEntry fe;
            fe.name = name;
            fe.path = entry.path().string();
            fe.is_directory = entry.is_directory();
            m_entries.push_back(fe);
        }

        // Sort: directories first, then by name
        std::sort(m_entries.begin(), m_entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
            }
            return a.name < b.name;
        });
    } catch (const std::exception&) {
        // Ignore errors
    }
}

void FileBrowserPanel::navigate_to(const std::string& path) {
    m_current_path = path;
    m_rename_target.clear();
    m_creating_folder = false;
    m_creating_scene = false;
    m_creating_pxg = false;
    m_creating_prefab = false;
    m_creating_script = false;
    refresh();
}

void FileBrowserPanel::select_file(const std::string& path) {
    m_selected_file = path;
    if (m_file_selected_callback) {
        m_file_selected_callback(path);
    }
}

void FileBrowserPanel::perform_delete(const std::string& path) {
    try {
        if (fs::is_directory(path)) {
            fs::remove_all(path);
        } else {
            fs::remove(path);

            // Also remove the .meta sidecar file if it exists
            std::string meta_path = path + ".meta";
            if (fs::exists(meta_path)) {
                fs::remove(meta_path);
            }
        }

        // Notify asset registry about the deletion
        if (m_editor_context) {
            m_editor_context->asset_registry().notify_deleted(path);
        }

        // Clear selection if we deleted the selected item
        if (m_selected_file == path) {
            m_selected_file.clear();
        }

        // Clear clipboard if we deleted the clipboard item
        if (m_clipboard_path == path) {
            m_clipboard_path.clear();
        }

        refresh();
    } catch (const std::exception& e) {
        engine::Logger::instance().error("FileBrowser", "Failed to delete: %s", e.what());
    }
}

void FileBrowserPanel::perform_rename(const std::string& old_path, const std::string& new_name) {
    try {
        fs::path parent = fs::path(old_path).parent_path();
        fs::path new_path = parent / new_name;

        if (fs::exists(new_path)) {
            engine::Logger::instance().warning("FileBrowser", "Cannot rename: '%s' already exists", new_name.c_str());
            return;
        }

        fs::rename(old_path, new_path);

        // Also rename the .meta sidecar file if it exists
        if (!fs::is_directory(old_path)) {
            std::string old_meta = old_path + ".meta";
            std::string new_meta = new_path.string() + ".meta";
            if (fs::exists(old_meta)) {
                fs::rename(old_meta, new_meta);
            }

            // Notify asset registry and update all references
            if (m_editor_context) {
                m_editor_context->asset_registry().notify_moved(old_path, new_path.string());
                m_editor_context->update_asset_references(old_path, new_path.string());
            }
        }

        // Update selection if renamed item was selected
        if (m_selected_file == old_path) {
            m_selected_file = new_path.string();
        }

        // Update clipboard if renamed item was in clipboard
        if (m_clipboard_path == old_path) {
            m_clipboard_path = new_path.string();
        }

        // Update current path if we renamed the current directory
        if (m_current_path == old_path) {
            m_current_path = new_path.string();
        }

        refresh();
    } catch (const std::exception& e) {
        engine::Logger::instance().error("FileBrowser", "Failed to rename: %s", e.what());
    }
}

void FileBrowserPanel::perform_paste(const std::string& dest_dir) {
    if (m_clipboard_path.empty() || !fs::exists(m_clipboard_path)) {
        return;
    }

    try {
        fs::path src(m_clipboard_path);
        fs::path dest = fs::path(dest_dir) / src.filename();

        // Don't paste into self
        if (src == dest) {
            return;
        }

        // For copy: don't allow pasting a folder into itself or a subdirectory of itself
        if (fs::is_directory(src)) {
            std::string dest_str = fs::path(dest_dir).string();
            std::string src_str = src.string();
            if (dest_str.find(src_str) == 0) {
                engine::Logger::instance().warning("FileBrowser", "Cannot paste folder into itself");
                return;
            }
        }

        std::string src_path_str = m_clipboard_path;

        if (m_clipboard_is_cut) {
            // Move
            fs::rename(src, dest);

            // Also move the .meta sidecar file if it exists
            if (!fs::is_directory(src)) {
                std::string src_meta = src_path_str + ".meta";
                std::string dest_meta = dest.string() + ".meta";
                if (fs::exists(src_meta)) {
                    fs::rename(src_meta, dest_meta);
                }

                // Notify asset registry and update all references
                if (m_editor_context) {
                    m_editor_context->asset_registry().notify_moved(src_path_str, dest.string());
                    m_editor_context->update_asset_references(src_path_str, dest.string());
                }
            }

            m_clipboard_path.clear();
        } else {
            // Copy - note: copied files get new GUIDs (the .meta is NOT copied)
            if (fs::is_directory(src)) {
                fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            } else {
                fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
                // Register the new file - it will get a fresh GUID
                if (m_editor_context) {
                    m_editor_context->asset_registry().register_asset(dest.string());
                }
            }
        }

        refresh();
    } catch (const std::exception& e) {
        engine::Logger::instance().error("FileBrowser", "Failed to paste: %s", e.what());
    }
}

void FileBrowserPanel::perform_move(const std::string& src_path, const std::string& dest_dir) {
    if (src_path.empty() || !fs::exists(src_path)) {
        return;
    }

    try {
        fs::path src(src_path);
        fs::path dest = fs::path(dest_dir) / src.filename();

        // Don't move to same location
        if (src.parent_path() == fs::path(dest_dir)) {
            return;
        }

        // Don't move into self
        if (src == dest) {
            return;
        }

        // Don't allow moving a folder into itself or a subdirectory of itself
        if (fs::is_directory(src)) {
            std::string dest_norm = engine::platform::normalize_path_for_comparison(dest_dir);
            std::string src_norm = engine::platform::normalize_path_for_comparison(src.string());
            if (dest_norm.find(src_norm) == 0) {
                engine::Logger::instance().warning("FileBrowser", "Cannot move folder into itself");
                return;
            }
        }

        // Handle name collision
        if (fs::exists(dest)) {
            engine::Logger::instance().warning("FileBrowser", "Cannot move: '%s' already exists in destination",
                                                src.filename().string().c_str());
            return;
        }

        fs::rename(src, dest);

        // Also move the .meta sidecar file if it exists
        if (!fs::is_directory(src)) {
            std::string src_meta = src_path + ".meta";
            std::string dest_meta = dest.string() + ".meta";
            if (fs::exists(src_meta)) {
                fs::rename(src_meta, dest_meta);
            }

            // Notify asset registry and update all references
            if (m_editor_context) {
                m_editor_context->asset_registry().notify_moved(src_path, dest.string());
                m_editor_context->update_asset_references(src_path, dest.string());
            }
        }

        // Update selection if we moved the selected item
        if (m_selected_file == src_path) {
            m_selected_file = dest.string();
        }

        // Update clipboard if we moved the clipboard item
        if (m_clipboard_path == src_path) {
            m_clipboard_path = dest.string();
        }

        refresh();
    } catch (const std::exception& e) {
        engine::Logger::instance().error("FileBrowser", "Failed to move: %s", e.what());
    }
}

void FileBrowserPanel::handle_keyboard_shortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) return;

    // Don't process shortcuts if rename or create is active
    if (!m_rename_target.empty() || m_creating_folder || m_creating_scene || m_creating_pxg ||
        m_creating_prefab || m_creating_script) return;

    ImGuiIO& io = ImGui::GetIO();

    // Ctrl+C - Copy
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        if (!m_selected_file.empty()) {
            m_clipboard_path = m_selected_file;
            m_clipboard_is_cut = false;
        }
    }

    // Ctrl+X - Cut
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        if (!m_selected_file.empty()) {
            m_clipboard_path = m_selected_file;
            m_clipboard_is_cut = true;
        }
    }

    // Ctrl+V - Paste
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        if (!m_clipboard_path.empty()) {
            perform_paste(m_current_path);
        }
    }

    // Delete key
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (!m_selected_file.empty() && fs::exists(m_selected_file)) {
            m_pending_delete_path = m_selected_file;
        }
    }

    // F2 - Rename
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        if (!m_selected_file.empty() && fs::exists(m_selected_file)) {
            m_rename_target = m_selected_file;
            std::string name = fs::path(m_selected_file).filename().string();
            std::strncpy(m_rename_buffer, name.c_str(), sizeof(m_rename_buffer) - 1);
            m_rename_buffer[sizeof(m_rename_buffer) - 1] = '\0';
            m_rename_focus_set = false;
        }
    }
}

int FileBrowserPanel::count_prefab_instances(const std::string& prefab_path) {
    if (!m_editor_context) return 0;

    auto* registry = m_editor_context->registry();
    if (!registry) return 0;

    int count = 0;
    auto view = registry->view<EntityInfo>();
    for (auto entity : view) {
        const auto& info = view.get<EntityInfo>(entity);
        if (info.is_prefab_instance && info.prefab_path == prefab_path) {
            ++count;
        }
    }
    return count;
}

void FileBrowserPanel::unlink_prefab_instances(const std::string& prefab_path) {
    if (!m_editor_context) return;

    auto* registry = m_editor_context->registry();
    if (!registry) return;

    auto view = registry->view<EntityInfo>();
    for (auto entity : view) {
        auto& info = view.get<EntityInfo>(entity);
        if (info.is_prefab_instance && info.prefab_path == prefab_path) {
            info.is_prefab_instance = false;
            info.prefab_path.clear();
        }
    }
    m_editor_context->scene_state().mark_dirty();
}

}
