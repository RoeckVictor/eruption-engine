#include "MaterialEditorPanel.h"
#include "editor/core/EditorContext.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/simulation/CategoryLibrary.h"
#include "engine/platform/PlatformUtils.h"
#include "engine/core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace editor {

MaterialEditorPanel::MaterialEditorPanel(EditorContext& context)
    : Panel("Material Editor", PanelVisibilityMode::OnDemand)
    , m_context(context)
{
}

MaterialEditorPanel::~MaterialEditorPanel() = default;

void MaterialEditorPanel::on_open() {
    // Initialize owned category library if not set externally
    if (!m_category_library) {
        m_category_library = &m_owned_category_library;
    }

    // Load categories first so materials can resolve category names
    reload_categories();

    // Set category library on material library for name resolution
    if (m_library && m_category_library) {
        m_library->set_category_library(m_category_library);
    }

    reload_materials();
}

void MaterialEditorPanel::on_close() {
    if (m_has_unsaved_changes) {
        // Could show confirmation dialog here
    }
}

void MaterialEditorPanel::set_library(engine::simulation::MaterialLibrary* library) {
    m_library = library;
    m_selected_material = -1;
    m_has_unsaved_changes = false;
}

void MaterialEditorPanel::set_category_library(engine::simulation::CategoryLibrary* library) {
    if (library) {
        m_category_library = library;
    } else {
        m_category_library = &m_owned_category_library;
    }
    m_selected_category = -1;
    m_has_unsaved_category_changes = false;

    // Reload categories into the new library
    reload_categories();

    // Update material library's category reference and reload materials
    // so they resolve category names against the new library
    if (m_library && m_category_library) {
        m_library->set_category_library(m_category_library);
        reload_materials();
    }
}

void MaterialEditorPanel::reload_materials() {
    if (m_library) {
        // Set category library for resolving category names
        if (m_category_library) {
            m_library->set_category_library(m_category_library);
        }

        m_library->clear();

        // Load engine materials first (read-only, IDs 0-9)
        std::string exe_dir = engine::platform::executable_directory();
        std::string engine_materials_path = exe_dir + "/assets/materials";
        m_library->load_from_directory(engine_materials_path);

        // Load project materials (editable) - scan entire Assets folder recursively
        const std::string& project_path = m_context.scene_state().project_path();
        if (!project_path.empty()) {
            namespace fs = std::filesystem;
            fs::path assets_root = fs::path(project_path) / "Assets";

            // Default save location for new materials
            m_materials_path = assets_root.string();

            // Recursively find and load all .material files in Assets
            if (fs::exists(assets_root)) {
                for (const auto& entry : fs::recursive_directory_iterator(assets_root)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".material") {
                        m_library->load_material_file(entry.path().string());
                    }
                }
            }
        } else {
            // Fallback to engine assets if no project loaded
            m_materials_path = engine_materials_path;
        }
    }

    m_selected_material = -1;
    m_has_unsaved_changes = false;
    m_graph_needs_rebuild = true;
}

void MaterialEditorPanel::reload_categories() {
    if (m_category_library) {
        m_category_library->clear();
        m_category_library->ensure_empty_category();

        // Load engine categories (read-only)
        std::string exe_dir = engine::platform::executable_directory();
        std::string engine_categories_path = exe_dir + "/assets/categories";
        m_category_library->load_from_directory(engine_categories_path, true);

        // Load project categories (editable) - scan entire Assets folder recursively
        const std::string& project_path = m_context.scene_state().project_path();
        if (!project_path.empty()) {
            namespace fs = std::filesystem;
            fs::path assets_root = fs::path(project_path) / "Assets";

            // Default save location for new categories
            m_categories_path = assets_root.string();

            // Recursively find and load all .phys files in Assets
            if (fs::exists(assets_root)) {
                for (const auto& entry : fs::recursive_directory_iterator(assets_root)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".phys") {
                        m_category_library->load_category(entry.path().string(), false);
                    }
                }
            }
        } else {
            // Fallback to engine assets if no project loaded
            m_categories_path = engine_categories_path;
        }
    }

    m_selected_category = -1;
    m_has_unsaved_category_changes = false;
}

void MaterialEditorPanel::on_gui() {
    if (!m_library) {
        ImGui::Text("No material library loaded.");
        return;
    }

    // Tab bar for Editor / Graph views
    if (ImGui::BeginTabBar("MaterialEditorTabs")) {
        if (ImGui::BeginTabItem("Editor")) {
            m_current_tab = 0;

            render_toolbar();
            ImGui::Separator();

            // Two-column layout: material list on left, properties on right
            float list_width = 200.0f;

            ImGui::BeginChild("MaterialList", ImVec2(list_width, 0), true);
            render_material_list();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("MaterialProperties", ImVec2(0, 0), true);
            render_material_properties();
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Graph")) {
            m_current_tab = 1;
            render_graph_view();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Categories")) {
            m_current_tab = 2;
            render_categories_tab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // New material dialog
    if (m_show_new_material_dialog) {
        ImGui::OpenPopup("New Material");
    }

    if (ImGui::BeginPopupModal("New Material", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Display Name", m_new_material_name, sizeof(m_new_material_name));
        ImGui::InputText("Internal Name", m_new_material_internal_name, sizeof(m_new_material_internal_name));
        ImGui::InputInt("Material ID", &m_new_material_id);

        // Clamp ID to valid range
        if (m_new_material_id < 10) m_new_material_id = 10;
        if (m_new_material_id > 255) m_new_material_id = 255;

        // Check if ID is already taken
        bool id_taken = is_id_taken(m_new_material_id);
        if (id_taken) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ID %d is already in use!", m_new_material_id);
        }

        ImGui::Separator();

        bool can_create = strlen(m_new_material_name) > 0 &&
                          strlen(m_new_material_internal_name) > 0 &&
                          !id_taken;

        if (!can_create) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            create_new_material();
            m_show_new_material_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        if (!can_create) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_new_material_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

int MaterialEditorPanel::find_smallest_unused_id() const {
    if (!m_library) return 10; // Start at 10 (after engine materials)

    // Find smallest unused ID starting from 10 (engine materials are 0-9)
    for (int id = 10; id < 256; ++id) {
        const auto* mat = m_library->get_material(static_cast<uint8_t>(id));
        // A slot is unused if no material exists or if it's an empty placeholder
        if (mat == nullptr || mat->internal_name.empty()) {
            return id;
        }
    }
    return 255; // Fallback
}

bool MaterialEditorPanel::is_id_taken(int id) const {
    if (!m_library) return false;
    if (id < 0 || id > 255) return true; // Invalid ID range
    const auto* mat = m_library->get_material(static_cast<uint8_t>(id));
    return mat != nullptr && !mat->internal_name.empty();
}

void MaterialEditorPanel::open_new_material_dialog() {
    m_new_material_id = find_smallest_unused_id();
    m_new_material_name[0] = '\0';
    m_new_material_internal_name[0] = '\0';
    m_show_new_material_dialog = true;

    // Make this panel visible if it isn't
    set_visible(true);
}

void MaterialEditorPanel::open_new_category_dialog() {
    if (m_category_library) {
        m_new_category_id = m_category_library->next_available_id();
    } else {
        m_new_category_id = 1;
    }
    m_new_category_name[0] = '\0';
    m_new_category_internal_name[0] = '\0';
    m_show_new_category_dialog = true;

    // Switch to Categories tab and make panel visible
    m_current_tab = 2;
    set_visible(true);
}

void MaterialEditorPanel::select_material_by_path(const std::string& path) {
    if (!m_library) return;

    // Find material with matching source_path
    const auto& materials = m_library->get_all_materials();
    for (size_t i = 0; i < materials.size(); ++i) {
        const auto& mat = materials[i];
        if (mat.source_path == path) {
            m_selected_material = static_cast<int>(i);
            m_editing_material = mat;
            m_has_unsaved_changes = false;
            m_selected_interaction = -1;
            m_current_tab = 0;  // Switch to Editor tab
            set_visible(true);
            return;
        }
    }

    // If not found, just open the editor tab
    m_current_tab = 0;
    set_visible(true);
}

void MaterialEditorPanel::select_category_by_path(const std::string& path) {
    if (!m_category_library) return;

    // Find category with matching source_path
    const auto& categories = m_category_library->all_categories();
    for (size_t i = 0; i < categories.size(); ++i) {
        const auto& cat = categories[i];
        if (cat.source_path == path) {
            m_selected_category = static_cast<int>(i);
            m_editing_category = cat;
            m_has_unsaved_category_changes = false;
            m_current_tab = 2;  // Switch to Categories tab
            set_visible(true);
            return;
        }
    }

    // If not found, just open the categories tab
    m_current_tab = 2;
    set_visible(true);
}

void MaterialEditorPanel::render_toolbar() {
    if (ImGui::Button("New")) {
        open_new_material_dialog();
    }

    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        save_current_material();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload")) {
        reload_materials();
    }

    ImGui::SameLine();

    if (m_has_unsaved_changes) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(unsaved changes)");
    }

    // Filter input
    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##filter", "Filter...", m_filter_text, sizeof(m_filter_text));
}

void MaterialEditorPanel::render_material_list() {
    const auto& materials = m_library->get_all_materials();

    for (size_t i = 0; i < materials.size(); ++i) {
        const auto& mat = materials[i];

        // Skip empty materials
        if (mat.internal_name.empty()) continue;

        // Apply filter
        if (m_filter_text[0] != '\0') {
            std::string filter_lower = m_filter_text;
            std::string name_lower = mat.name;
            std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name_lower.find(filter_lower) == std::string::npos) continue;
        }

        ImGui::PushID(static_cast<int>(i));

        // Color preview
        float color[4];
        uint32_to_color(mat.color, color);
        ImGui::ColorButton("##matcolor", ImVec4(color[0], color[1], color[2], color[3]),
                          ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
        ImGui::SameLine();

        // Material name (show lock icon for engine materials)
        char label[256];
        bool is_engine_mat = (mat.id < 10); // IDs 0-9 are reserved for engine materials
        if (is_engine_mat) {
            snprintf(label, sizeof(label), "[%d] %s (engine)", mat.id, mat.name.c_str());
        } else {
            snprintf(label, sizeof(label), "[%d] %s", mat.id, mat.name.c_str());
        }

        bool selected = (m_selected_material == static_cast<int>(i));
        if (ImGui::Selectable(label, selected)) {
            if (m_selected_material != static_cast<int>(i)) {
                // Copy material data for editing
                m_selected_material = static_cast<int>(i);
                m_editing_material = mat;
                m_has_unsaved_changes = false;
                m_selected_interaction = -1;
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Edit")) {
                m_selected_material = static_cast<int>(i);
                m_editing_material = mat;
                m_has_unsaved_changes = false;
                m_selected_interaction = -1;
            }

            // Only allow delete for non-engine materials
            if (ImGui::MenuItem("Delete", nullptr, false, !is_engine_mat)) {
                m_pending_delete_material_id = mat.id;
                m_pending_delete_material_name = mat.name;
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    // Delete confirmation modal
    if (m_pending_delete_material_id >= 0) {
        ImGui::OpenPopup("Delete Material?");
    }

    if (ImGui::BeginPopupModal("Delete Material?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete \"%s\"?", m_pending_delete_material_name.c_str());
        ImGui::Text("This will delete the .material file permanently.");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            delete_material(static_cast<uint8_t>(m_pending_delete_material_id));
            m_pending_delete_material_id = -1;
            m_pending_delete_material_name.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pending_delete_material_id = -1;
            m_pending_delete_material_name.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

bool MaterialEditorPanel::is_selected_material_readonly() const {
    if (m_selected_material < 0) return true;
    // Engine materials (IDs 0-9) are read-only
    return m_editing_material.id < 10;
}

void MaterialEditorPanel::render_material_properties() {
    if (m_selected_material < 0) {
        ImGui::Text("Select a material to edit.");
        return;
    }

    bool readonly = is_selected_material_readonly();

    // Material header
    ImGui::Text("ID: %d", m_editing_material.id);
    ImGui::SameLine();
    ImGui::Text("Internal: %s", m_editing_material.internal_name.c_str());

    if (readonly) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "(read-only engine material)");
    }

    // Display name (disabled for read-only)
    if (readonly) ImGui::BeginDisabled();
    char name_buf[128];
    strncpy(name_buf, m_editing_material.name.c_str(), sizeof(name_buf));
    if (ImGui::InputText("Display Name", name_buf, sizeof(name_buf))) {
        m_editing_material.name = name_buf;
        m_has_unsaved_changes = true;
    }
    if (readonly) ImGui::EndDisabled();

    ImGui::Separator();

    // Pass readonly to each section so they can disable their controls
    // but still allow collapsing headers to be interactive
    render_physical_section(readonly);
    render_thermal_section(readonly);
    render_rendering_section(readonly);
    render_flags_section(readonly);
    render_interactions_section(readonly);
}

void MaterialEditorPanel::render_physical_section(bool readonly) {
    if (ImGui::CollapsingHeader("Physical Properties", m_physical_expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_physical_expanded = true;

        if (readonly) ImGui::BeginDisabled();

        int density = m_editing_material.density;
        if (ImGui::DragInt("Density", &density, 1.0f, 0, 255)) {
            m_editing_material.density = static_cast<uint8_t>(density);
            m_has_unsaved_changes = true;
        }

        // Build category list from CategoryLibrary
        if (m_category_library) {
            const auto& categories = m_category_library->all_categories();

            // Sort by ID for consistent ordering
            std::vector<size_t> sorted_indices(categories.size());
            for (size_t i = 0; i < categories.size(); ++i) {
                sorted_indices[i] = i;
            }
            std::sort(sorted_indices.begin(), sorted_indices.end(),
                [&categories](size_t a, size_t b) {
                    return categories[a].id < categories[b].id;
                });

            // Find current selection index
            int current_idx = 0;
            uint8_t current_cat_id = static_cast<uint8_t>(m_editing_material.category);
            for (size_t i = 0; i < sorted_indices.size(); ++i) {
                if (categories[sorted_indices[i]].id == current_cat_id) {
                    current_idx = static_cast<int>(i);
                    break;
                }
            }

            // Build combo string
            std::string combo_preview = (current_idx < static_cast<int>(sorted_indices.size()))
                ? categories[sorted_indices[current_idx]].name
                : "Unknown";

            if (ImGui::BeginCombo("Category", combo_preview.c_str())) {
                for (size_t i = 0; i < sorted_indices.size(); ++i) {
                    const auto& cat = categories[sorted_indices[i]];
                    char label[128];
                    snprintf(label, sizeof(label), "[%d] %s", cat.id, cat.name.c_str());

                    bool is_selected = (static_cast<int>(i) == current_idx);
                    if (ImGui::Selectable(label, is_selected)) {
                        m_editing_material.category = static_cast<engine::simulation::MaterialCategory>(cat.id);
                        m_has_unsaved_changes = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            // Fallback to hardcoded categories if no library available
            const char* categories[] = { "empty", "static", "powder", "liquid", "gas" };
            int cat_idx = static_cast<int>(m_editing_material.category);
            if (ImGui::Combo("Category", &cat_idx, categories, IM_ARRAYSIZE(categories))) {
                m_editing_material.category = static_cast<engine::simulation::MaterialCategory>(cat_idx);
                m_has_unsaved_changes = true;
            }
        }

        if (readonly) ImGui::EndDisabled();
    } else {
        m_physical_expanded = false;
    }
}

void MaterialEditorPanel::render_thermal_section(bool readonly) {
    if (ImGui::CollapsingHeader("Thermal Properties", m_thermal_expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_thermal_expanded = true;

        if (readonly) ImGui::BeginDisabled();

        int default_temp = m_editing_material.default_temp;
        if (ImGui::DragInt("Default Temp", &default_temp, 1.0f, 0, 255)) {
            m_editing_material.default_temp = static_cast<uint8_t>(default_temp);
            m_has_unsaved_changes = true;
        }

        float conductivity = m_editing_material.conductivity;
        if (ImGui::DragFloat("Conductivity", &conductivity, 0.01f, 0.0f, 1.0f)) {
            m_editing_material.conductivity = conductivity;
            m_has_unsaved_changes = true;
        }

        if (readonly) ImGui::EndDisabled();
    } else {
        m_thermal_expanded = false;
    }
}

void MaterialEditorPanel::render_rendering_section(bool readonly) {
    if (ImGui::CollapsingHeader("Rendering", m_rendering_expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_rendering_expanded = true;

        if (readonly) ImGui::BeginDisabled();

        float color[4];
        uint32_to_color(m_editing_material.color, color);
        if (ImGui::ColorEdit4("Default Color", color)) {
            m_editing_material.color = color_to_uint32(color);
            m_has_unsaved_changes = true;
        }

        int hue_var = m_editing_material.color_variance_hue;
        int sat_var = m_editing_material.color_variance_saturation;
        int light_var = m_editing_material.color_variance_lightness;

        if (ImGui::DragInt("Hue Variance", &hue_var, 1.0f, 0, 180)) {
            m_editing_material.color_variance_hue = static_cast<uint8_t>(hue_var);
            m_has_unsaved_changes = true;
        }
        if (ImGui::DragInt("Saturation Variance", &sat_var, 1.0f, 0, 100)) {
            m_editing_material.color_variance_saturation = static_cast<uint8_t>(sat_var);
            m_has_unsaved_changes = true;
        }
        if (ImGui::DragInt("Lightness Variance", &light_var, 1.0f, 0, 100)) {
            m_editing_material.color_variance_lightness = static_cast<uint8_t>(light_var);
            m_has_unsaved_changes = true;
        }

        if (readonly) ImGui::EndDisabled();
    } else {
        m_rendering_expanded = false;
    }
}

void MaterialEditorPanel::render_flags_section(bool readonly) {
    if (ImGui::CollapsingHeader("Flags", m_flags_expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_flags_expanded = true;

        if (readonly) ImGui::BeginDisabled();

        bool hazard = (m_editing_material.flags & engine::simulation::MAT_FLAG_HAZARD) != 0;
        bool flammable = (m_editing_material.flags & engine::simulation::MAT_FLAG_FLAMMABLE) != 0;
        bool conductive = (m_editing_material.flags & engine::simulation::MAT_FLAG_CONDUCTIVE) != 0;

        if (ImGui::Checkbox("Hazard", &hazard)) {
            if (hazard) m_editing_material.flags |= engine::simulation::MAT_FLAG_HAZARD;
            else m_editing_material.flags &= ~engine::simulation::MAT_FLAG_HAZARD;
            m_has_unsaved_changes = true;
        }
        if (ImGui::Checkbox("Flammable", &flammable)) {
            if (flammable) m_editing_material.flags |= engine::simulation::MAT_FLAG_FLAMMABLE;
            else m_editing_material.flags &= ~engine::simulation::MAT_FLAG_FLAMMABLE;
            m_has_unsaved_changes = true;
        }
        if (ImGui::Checkbox("Conductive", &conductive)) {
            if (conductive) m_editing_material.flags |= engine::simulation::MAT_FLAG_CONDUCTIVE;
            else m_editing_material.flags &= ~engine::simulation::MAT_FLAG_CONDUCTIVE;
            m_has_unsaved_changes = true;
        }

        if (readonly) ImGui::EndDisabled();
    } else {
        m_flags_expanded = false;
    }
}

void MaterialEditorPanel::render_interactions_section(bool readonly) {
    if (ImGui::CollapsingHeader("Interactions", m_interactions_expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_interactions_expanded = true;

        // "Add" button disabled for read-only materials
        if (readonly) ImGui::BeginDisabled();
        if (ImGui::Button("+ Add Interaction")) {
            engine::simulation::Interaction new_interaction;
            new_interaction.id = "new_interaction_" + std::to_string(m_editing_material.interactions.size());
            new_interaction.type = engine::simulation::InteractionType::TEMPERATURE;
            new_interaction.priority = 50;
            new_interaction.probability = 1.0f;
            m_editing_material.interactions.push_back(new_interaction);
            m_selected_interaction = static_cast<int>(m_editing_material.interactions.size()) - 1;
            m_has_unsaved_changes = true;
        }
        if (readonly) ImGui::EndDisabled();

        ImGui::Separator();

        for (size_t i = 0; i < m_editing_material.interactions.size(); ++i) {
            auto& interaction = m_editing_material.interactions[i];

            ImGui::PushID(static_cast<int>(i));

            bool is_selected = (m_selected_interaction == static_cast<int>(i));
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
            if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

            // Use stable ID (##) to prevent collapse when editing the interaction ID
            bool node_open = ImGui::TreeNodeEx("##interaction_node", flags, "%s", interaction.id.c_str());

            if (ImGui::IsItemClicked()) {
                m_selected_interaction = static_cast<int>(i);
            }

            // Delete button (disabled for read-only)
            ImGui::SameLine(ImGui::GetWindowWidth() - 60);
            if (readonly) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Delete")) {
                m_editing_material.interactions.erase(m_editing_material.interactions.begin() + i);
                if (m_selected_interaction >= static_cast<int>(m_editing_material.interactions.size())) {
                    m_selected_interaction = static_cast<int>(m_editing_material.interactions.size()) - 1;
                }
                m_has_unsaved_changes = true;
                if (readonly) ImGui::EndDisabled();
                ImGui::PopID();
                if (node_open) ImGui::TreePop();
                break;
            }
            if (readonly) ImGui::EndDisabled();

            if (node_open) {
                render_interaction_editor(interaction, static_cast<int>(i), readonly);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    } else {
        m_interactions_expanded = false;
    }
}

void MaterialEditorPanel::render_interaction_editor(engine::simulation::Interaction& interaction, int index, bool readonly) {
    if (readonly) ImGui::BeginDisabled();

    char id_buf[64];
    strncpy(id_buf, interaction.id.c_str(), sizeof(id_buf));
    if (ImGui::InputText("ID", id_buf, sizeof(id_buf))) {
        interaction.id = id_buf;
        m_has_unsaved_changes = true;
    }

    const char* types[] = { "temperature", "contact", "time_decay", "random", "flag_based" };
    int type_idx = static_cast<int>(interaction.type);
    if (ImGui::Combo("Type", &type_idx, types, IM_ARRAYSIZE(types))) {
        interaction.type = static_cast<engine::simulation::InteractionType>(type_idx);
        m_has_unsaved_changes = true;
    }

    int priority = interaction.priority;
    if (ImGui::DragInt("Priority", &priority, 1.0f, 0, 255)) {
        interaction.priority = static_cast<uint8_t>(priority);
        m_has_unsaved_changes = true;
    }

    float probability = interaction.probability;
    if (ImGui::DragFloat("Probability", &probability, 0.01f, 0.0f, 1.0f)) {
        interaction.probability = probability;
        m_has_unsaved_changes = true;
    }

    int threshold = interaction.sim_step_threshold;
    if (ImGui::InputInt("Sim Step Threshold", &threshold)) {
        interaction.sim_step_threshold = static_cast<uint16_t>(std::max(0, threshold));
        m_has_unsaved_changes = true;
    }

    if (readonly) ImGui::EndDisabled();

    // Conditions (tree node stays interactive for viewing)
    if (ImGui::TreeNode("Conditions")) {
        render_conditions_editor(interaction.conditions, readonly);
        ImGui::TreePop();
    }

    // Effects (tree node stays interactive for viewing)
    if (ImGui::TreeNode("Effects")) {
        if (readonly) ImGui::BeginDisabled();
        if (ImGui::Button("+ Add Effect")) {
            engine::simulation::InteractionEffect new_effect;
            new_effect.type = engine::simulation::EffectType::TRANSFORM;
            interaction.effects.push_back(new_effect);
            m_has_unsaved_changes = true;
        }
        if (readonly) ImGui::EndDisabled();

        for (size_t i = 0; i < interaction.effects.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            render_effect_editor(interaction.effects[i], static_cast<int>(i), readonly);

            if (readonly) ImGui::BeginDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                interaction.effects.erase(interaction.effects.begin() + i);
                m_has_unsaved_changes = true;
                if (readonly) ImGui::EndDisabled();
                ImGui::PopID();
                break;
            }
            if (readonly) ImGui::EndDisabled();

            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

void MaterialEditorPanel::render_conditions_editor(engine::simulation::InteractionConditions& conditions, bool readonly) {
    if (readonly) ImGui::BeginDisabled();

    int temp_above = conditions.temp_above;
    int temp_below = conditions.temp_below;

    if (ImGui::DragInt("Temp Above", &temp_above, 1.0f, 0, 255)) {
        conditions.temp_above = static_cast<uint8_t>(temp_above);
        m_has_unsaved_changes = true;
    }
    if (ImGui::DragInt("Temp Below", &temp_below, 1.0f, 0, 255)) {
        conditions.temp_below = static_cast<uint8_t>(temp_below);
        m_has_unsaved_changes = true;
    }

    // Contact materials - dropdown with multi-select
    ImGui::Text("Contact With:");
    ImGui::SameLine();

    // Show current selections as tags
    for (size_t i = 0; i < conditions.contact_with.size(); ++i) {
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "%s", conditions.contact_with[i].c_str());
        ImGui::SameLine(0, 2);
        if (ImGui::SmallButton("x")) {
            conditions.contact_with.erase(conditions.contact_with.begin() + static_cast<ptrdiff_t>(i));
            m_has_unsaved_changes = true;
        }
        ImGui::PopID();
    }

    // Add material dropdown
    if (m_library) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##add_contact", "+Add")) {
            const auto& materials = m_library->get_all_materials();
            for (const auto& mat : materials) {
                if (mat.internal_name.empty()) continue;

                // Check if already in the list
                bool already_added = std::find(conditions.contact_with.begin(),
                                               conditions.contact_with.end(),
                                               mat.internal_name) != conditions.contact_with.end();
                if (already_added) continue;

                if (ImGui::Selectable(mat.name.c_str())) {
                    conditions.contact_with.push_back(mat.internal_name);
                    m_has_unsaved_changes = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (readonly) ImGui::EndDisabled();
}

void MaterialEditorPanel::render_effect_editor(engine::simulation::InteractionEffect& effect, int index, bool readonly) {
    if (readonly) ImGui::BeginDisabled();

    const char* effect_types[] = {
        "transform", "transform_neighbor", "set_temp", "change_temp",
        "set_neighbor_temp", "change_neighbor_temp", "set_flag", "clear_flag",
        "set_neighbor_flag", "spawn_particle", "destroy"
    };
    int type_idx = static_cast<int>(effect.type);
    ImGui::SetNextItemWidth(120);
    if (ImGui::Combo("##efftype", &type_idx, effect_types, IM_ARRAYSIZE(effect_types))) {
        effect.type = static_cast<engine::simulation::EffectType>(type_idx);
        m_has_unsaved_changes = true;
    }

    ImGui::SameLine();

    // Parameter based on effect type
    switch (effect.type) {
        case engine::simulation::EffectType::TRANSFORM:
        case engine::simulation::EffectType::TRANSFORM_NEIGHBOR:
        case engine::simulation::EffectType::SPAWN_PARTICLE: {
            // Material dropdown for "into" field
            ImGui::SetNextItemWidth(120);
            std::string preview = effect.material_name.empty() ? "(Select material)" : effect.material_name;
            if (m_library && ImGui::BeginCombo("##into", preview.c_str())) {
                const auto& materials = m_library->get_all_materials();
                for (const auto& mat : materials) {
                    if (mat.internal_name.empty()) continue;

                    bool is_selected = (effect.material_name == mat.internal_name);
                    if (ImGui::Selectable(mat.name.c_str(), is_selected)) {
                        effect.material_name = mat.internal_name;
                        m_has_unsaved_changes = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // Color behavior dropdown for transform effects
            ImGui::SameLine();
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            // Transform effects: None, Replace, Inherit, Blend, Add, Subtract
            const char* transform_color_behaviors[] = { "None", "Replace", "Inherit", "Blend", "Add", "Subtract" };
            int color_idx = static_cast<int>(effect.color_behavior);
            if (ImGui::Combo("##colorbehavior", &color_idx, transform_color_behaviors, IM_ARRAYSIZE(transform_color_behaviors))) {
                effect.color_behavior = static_cast<engine::simulation::ColorBehavior>(color_idx);
                m_has_unsaved_changes = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("None: No color change\nReplace: Use new material's color\nInherit: Keep original color\nBlend: Mix colors 50/50\nAdd: Add color delta\nSubtract: Subtract color delta");
            }

            // Color picker for Add/Subtract modes
            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD ||
                effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) {
                ImGui::SameLine();
                float color_delta[4];
                uint32_to_color(effect.color_delta, color_delta);
                ImGui::SetNextItemWidth(150);
                if (ImGui::ColorEdit4("##colordelta", color_delta, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    effect.color_delta = color_to_uint32(color_delta);
                    m_has_unsaved_changes = true;
                }
            }
            break;
        }
        case engine::simulation::EffectType::SET_TEMP:
        case engine::simulation::EffectType::SET_NEIGHBOR_TEMP: {
            int value = effect.value;
            ImGui::SetNextItemWidth(60);
            if (ImGui::InputInt("##value", &value)) {
                effect.value = static_cast<uint8_t>(std::clamp(value, 0, 255));
                m_has_unsaved_changes = true;
            }

            // Color behavior dropdown for temperature effects
            ImGui::SameLine();
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            const char* temp_color_behaviors[] = { "None", "Add", "Subtract" };
            int temp_color_idx = 0;
            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD) temp_color_idx = 1;
            else if (effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) temp_color_idx = 2;

            if (ImGui::Combo("##colorbehavior_temp", &temp_color_idx, temp_color_behaviors, IM_ARRAYSIZE(temp_color_behaviors))) {
                switch (temp_color_idx) {
                    case 0: effect.color_behavior = engine::simulation::ColorBehavior::NONE; break;
                    case 1: effect.color_behavior = engine::simulation::ColorBehavior::ADD; break;
                    case 2: effect.color_behavior = engine::simulation::ColorBehavior::SUBTRACT; break;
                }
                m_has_unsaved_changes = true;
            }

            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD ||
                effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) {
                ImGui::SameLine();
                float color_delta[4];
                uint32_to_color(effect.color_delta, color_delta);
                ImGui::SetNextItemWidth(150);
                if (ImGui::ColorEdit4("##colordelta_temp", color_delta, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    effect.color_delta = color_to_uint32(color_delta);
                    m_has_unsaved_changes = true;
                }
            }
            break;
        }
        case engine::simulation::EffectType::CHANGE_TEMP:
        case engine::simulation::EffectType::CHANGE_NEIGHBOR_TEMP: {
            int delta = effect.delta;
            ImGui::SetNextItemWidth(60);
            if (ImGui::InputInt("##delta", &delta)) {
                effect.delta = static_cast<int16_t>(std::clamp(delta, -128, 127));
                m_has_unsaved_changes = true;
            }

            // Color behavior dropdown for temperature change effects
            ImGui::SameLine();
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            const char* chg_color_behaviors[] = { "None", "Add", "Subtract" };
            int chg_color_idx = 0;
            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD) chg_color_idx = 1;
            else if (effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) chg_color_idx = 2;

            if (ImGui::Combo("##colorbehavior_chg", &chg_color_idx, chg_color_behaviors, IM_ARRAYSIZE(chg_color_behaviors))) {
                switch (chg_color_idx) {
                    case 0: effect.color_behavior = engine::simulation::ColorBehavior::NONE; break;
                    case 1: effect.color_behavior = engine::simulation::ColorBehavior::ADD; break;
                    case 2: effect.color_behavior = engine::simulation::ColorBehavior::SUBTRACT; break;
                }
                m_has_unsaved_changes = true;
            }

            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD ||
                effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) {
                ImGui::SameLine();
                float color_delta[4];
                uint32_to_color(effect.color_delta, color_delta);
                ImGui::SetNextItemWidth(150);
                if (ImGui::ColorEdit4("##colordelta_chg", color_delta, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    effect.color_delta = color_to_uint32(color_delta);
                    m_has_unsaved_changes = true;
                }
            }
            break;
        }
        case engine::simulation::EffectType::DESTROY:
            // No parameters or color behavior for destroy - pixel becomes air
            break;
        default:
            // Color behavior dropdown for non-transform effects (None, Add, Subtract only)
            ImGui::SameLine();
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            // Non-transform effects only support None, Add, Subtract
            const char* other_color_behaviors[] = { "None", "Add", "Subtract" };
            // Map enum values to combo index: None=0->0, Add=4->1, Subtract=5->2
            int other_color_idx = 0;
            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD) other_color_idx = 1;
            else if (effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) other_color_idx = 2;

            if (ImGui::Combo("##colorbehavior_other", &other_color_idx, other_color_behaviors, IM_ARRAYSIZE(other_color_behaviors))) {
                switch (other_color_idx) {
                    case 0: effect.color_behavior = engine::simulation::ColorBehavior::NONE; break;
                    case 1: effect.color_behavior = engine::simulation::ColorBehavior::ADD; break;
                    case 2: effect.color_behavior = engine::simulation::ColorBehavior::SUBTRACT; break;
                }
                m_has_unsaved_changes = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("None: No color change\nAdd: Add color delta\nSubtract: Subtract color delta");
            }

            // Color picker for Add/Subtract modes
            if (effect.color_behavior == engine::simulation::ColorBehavior::ADD ||
                effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) {
                ImGui::SameLine();
                float color_delta[4];
                uint32_to_color(effect.color_delta, color_delta);
                ImGui::SetNextItemWidth(150);
                if (ImGui::ColorEdit4("##colordelta_other", color_delta, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                    effect.color_delta = color_to_uint32(color_delta);
                    m_has_unsaved_changes = true;
                }
            }
            break;
    }

    if (readonly) ImGui::EndDisabled();
}

void MaterialEditorPanel::save_current_material() {
    // Check if we have a valid material to save (internal_name is required)
    if (m_editing_material.internal_name.empty()) return;

    namespace fs = std::filesystem;

    std::string filename = m_materials_path + "/" + m_editing_material.internal_name + ".material";

    // Build JSON
    nlohmann::json j;
    j["schema_version"] = 2;
    j["id"] = m_editing_material.id;
    j["name"] = m_editing_material.name;
    j["internal_name"] = m_editing_material.internal_name;

    // Physical
    nlohmann::json phys;
    phys["density"] = m_editing_material.density;

    // Get category internal_name from CategoryLibrary
    std::string category_name = "empty";
    if (m_category_library) {
        uint8_t cat_id = static_cast<uint8_t>(m_editing_material.category);
        const auto* cat = m_category_library->get_by_id(cat_id);
        if (cat) {
            category_name = cat->internal_name;
        }
    }
    phys["category"] = category_name;
    j["physical"] = phys;

    // Thermal
    nlohmann::json therm;
    therm["default_temp"] = m_editing_material.default_temp;
    therm["conductivity"] = m_editing_material.conductivity;
    j["thermal"] = therm;

    // Rendering
    nlohmann::json rend;
    char color_str[16];
    snprintf(color_str, sizeof(color_str), "#%08X", m_editing_material.color);
    rend["default_color"] = color_str;
    if (m_editing_material.color_variance_hue > 0 ||
        m_editing_material.color_variance_saturation > 0 ||
        m_editing_material.color_variance_lightness > 0) {
        nlohmann::json var;
        var["hue"] = m_editing_material.color_variance_hue;
        var["saturation"] = m_editing_material.color_variance_saturation;
        var["lightness"] = m_editing_material.color_variance_lightness;
        rend["color_variance"] = var;
    }
    j["rendering"] = rend;

    // Flags
    std::vector<std::string> flags;
    if (m_editing_material.flags & engine::simulation::MAT_FLAG_HAZARD) flags.push_back("hazard");
    if (m_editing_material.flags & engine::simulation::MAT_FLAG_FLAMMABLE) flags.push_back("flammable");
    if (m_editing_material.flags & engine::simulation::MAT_FLAG_CONDUCTIVE) flags.push_back("conductive");
    j["flags"] = flags;

    // Interactions
    nlohmann::json interactions_arr = nlohmann::json::array();
    for (const auto& interaction : m_editing_material.interactions) {
        nlohmann::json int_j;
        int_j["id"] = interaction.id;
        int_j["type"] = engine::simulation::interaction_type_to_string(interaction.type);
        int_j["priority"] = interaction.priority;
        int_j["probability"] = interaction.probability;
        int_j["sim_step_threshold"] = interaction.sim_step_threshold;

        // Conditions
        nlohmann::json cond;
        if (interaction.conditions.temp_above > 0) cond["temp_above"] = interaction.conditions.temp_above;
        if (interaction.conditions.temp_below < 255) cond["temp_below"] = interaction.conditions.temp_below;
        if (!interaction.conditions.contact_with.empty()) cond["contact_with"] = interaction.conditions.contact_with;
        int_j["conditions"] = cond;

        // Effects
        nlohmann::json effects_arr = nlohmann::json::array();
        for (const auto& effect : interaction.effects) {
            nlohmann::json eff_j;
            eff_j["type"] = engine::simulation::effect_type_to_string(effect.type);
            if (!effect.material_name.empty()) eff_j["into"] = effect.material_name;
            if (effect.value > 0) eff_j["value"] = effect.value;
            if (effect.delta != 0) eff_j["delta"] = effect.delta;

            // Determine default color_behavior based on effect type
            bool is_transform = (effect.type == engine::simulation::EffectType::TRANSFORM ||
                                effect.type == engine::simulation::EffectType::TRANSFORM_NEIGHBOR);
            engine::simulation::ColorBehavior default_cb = is_transform ?
                engine::simulation::ColorBehavior::REPLACE : engine::simulation::ColorBehavior::NONE;

            if (effect.color_behavior != default_cb) {
                eff_j["color_behavior"] = engine::simulation::color_behavior_to_string(effect.color_behavior);
            }

            // Save color_delta for Add/Subtract modes
            if ((effect.color_behavior == engine::simulation::ColorBehavior::ADD ||
                 effect.color_behavior == engine::simulation::ColorBehavior::SUBTRACT) &&
                effect.color_delta != 0) {
                char color_delta_str[16];
                snprintf(color_delta_str, sizeof(color_delta_str), "#%08X", effect.color_delta);
                eff_j["color_delta"] = color_delta_str;
            }

            effects_arr.push_back(eff_j);
        }
        int_j["effects"] = effects_arr;

        interactions_arr.push_back(int_j);
    }
    j["interactions"] = interactions_arr;

    // Write file
    std::ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(2);
        file.close();
        m_has_unsaved_changes = false;
        engine::Logger::instance().info("MaterialEditor", "Saved material to: %s", filename.c_str());

        // Reload materials to update the library with saved changes
        uint8_t saved_id = m_editing_material.id;
        reload_materials();

        // Re-select the material we just saved
        const auto& materials = m_library->get_all_materials();
        for (size_t i = 0; i < materials.size(); ++i) {
            if (materials[i].id == saved_id && !materials[i].internal_name.empty()) {
                m_selected_material = static_cast<int>(i);
                m_editing_material = materials[i];
                break;
            }
        }

        // Refresh file browser in case a new file was created
        m_context.refresh_file_browser();

        // Update GPU material tables if simulation is running
        m_context.update_material_tables();
    } else {
        engine::Logger::instance().error("MaterialEditor", "Failed to save material: %s", filename.c_str());
    }
}

void MaterialEditorPanel::create_new_material() {
    if (strlen(m_new_material_name) == 0 || strlen(m_new_material_internal_name) == 0) return;

    // Validate that ID is not already taken
    if (is_id_taken(m_new_material_id)) {
        return; // ID already exists
    }

    engine::simulation::MaterialDefinition new_mat;
    new_mat.id = static_cast<uint8_t>(m_new_material_id);
    new_mat.name = m_new_material_name;
    new_mat.internal_name = m_new_material_internal_name;
    new_mat.density = 100;
    new_mat.category = engine::simulation::CAT_STATIC;
    new_mat.default_temp = 128;
    new_mat.conductivity = 0.5f;
    new_mat.color = 0xFFFFFFFF;

    m_editing_material = new_mat;
    m_has_unsaved_changes = true;

    // Save immediately to create the file
    save_current_material();

    // Reload to include the new material
    reload_materials();

    // Refresh file browser to show the new file
    m_context.refresh_file_browser();
}

void MaterialEditorPanel::delete_current_material() {
    if (m_selected_material < 0) return;
    if (is_selected_material_readonly()) return;

    delete_material(m_editing_material.id);
}

void MaterialEditorPanel::delete_material(uint8_t material_id) {
    if (!m_library) return;

    // Don't delete engine materials
    if (material_id < 10) return;

    const auto* mat = m_library->get_material(material_id);
    if (!mat) return;

    // Find and delete the .material file
    namespace fs = std::filesystem;

    // Search in project assets folder recursively
    const std::string& project_path = m_context.scene_state().project_path();
    if (!project_path.empty()) {
        fs::path assets_root = fs::path(project_path) / "Assets";

        if (fs::exists(assets_root)) {
            for (const auto& entry : fs::recursive_directory_iterator(assets_root)) {
                if (entry.is_regular_file() && entry.path().extension() == ".material") {
                    // Check if this file contains the material we want to delete
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        try {
                            nlohmann::json j = nlohmann::json::parse(file);
                            if (j.contains("id") && j["id"].get<int>() == material_id) {
                                file.close();
                                fs::remove(entry.path());

                                // Clear selection if we deleted the selected material
                                if (m_selected_material >= 0 && m_editing_material.id == material_id) {
                                    m_selected_material = -1;
                                    m_has_unsaved_changes = false;
                                }

                                // Reload materials and refresh file browser
                                reload_materials();
                                m_context.refresh_file_browser();
                                return;
                            }
                        } catch (...) {
                            // Ignore parse errors
                        }
                    }
                }
            }
        }
    }
}

uint32_t MaterialEditorPanel::color_to_uint32(const float color[4]) {
    uint8_t r = static_cast<uint8_t>(color[0] * 255.0f);
    uint8_t g = static_cast<uint8_t>(color[1] * 255.0f);
    uint8_t b = static_cast<uint8_t>(color[2] * 255.0f);
    uint8_t a = static_cast<uint8_t>(color[3] * 255.0f);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

void MaterialEditorPanel::uint32_to_color(uint32_t color, float out[4]) {
    out[0] = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
    out[1] = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    out[2] = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    out[3] = static_cast<float>(color & 0xFF) / 255.0f;
}

// ============================================================================
// Graph Visualization
// ============================================================================

void MaterialEditorPanel::render_graph_view() {
    // Rebuild graph if needed
    if (m_graph_needs_rebuild) {
        build_graph();
        m_graph_needs_rebuild = false;
        m_graph_needs_layout = true;
    }

    // Run layout simulation if needed
    if (m_graph_needs_layout) {
        layout_graph();
    }

    // Graph controls
    ImGui::Text("Materials: %zu  Connections: %zu", m_graph_nodes.size(), m_graph_edges.size());
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        m_graph_needs_layout = true;
        // Randomize positions
        for (auto& node : m_graph_nodes) {
            node.position = ImVec2(
                static_cast<float>(rand() % 400) - 200.0f,
                static_cast<float>(rand() % 400) - 200.0f
            );
            node.velocity = ImVec2(0, 0);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Rebuild")) {
        m_graph_needs_rebuild = true;
    }
    ImGui::SameLine();
    ImGui::Text("Zoom: %.1fx", m_graph_zoom);
    ImGui::SameLine();
    ImGui::TextDisabled("(drag nodes to move, middle-mouse to pan, scroll to zoom)");

    ImGui::Separator();

    // Get canvas region
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    ImVec2 canvas_center = ImVec2(
        canvas_pos.x + canvas_size.x * 0.5f,
        canvas_pos.y + canvas_size.y * 0.5f
    );

    // Create canvas
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(30, 30, 35, 255));

    // Grid
    float grid_step = 50.0f * m_graph_zoom;
    ImU32 grid_color = IM_COL32(50, 50, 55, 255);
    for (float x = fmodf(m_graph_offset.x * m_graph_zoom, grid_step); x < canvas_size.x; x += grid_step) {
        draw_list->AddLine(
            ImVec2(canvas_pos.x + x, canvas_pos.y),
            ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
            grid_color
        );
    }
    for (float y = fmodf(m_graph_offset.y * m_graph_zoom, grid_step); y < canvas_size.y; y += grid_step) {
        draw_list->AddLine(
            ImVec2(canvas_pos.x, canvas_pos.y + y),
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
            grid_color
        );
    }

    // Clip to canvas
    draw_list->PushClipRect(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    ImVec2 mouse_pos = ImGui::GetMousePos();

    // Build node position lookup
    std::unordered_map<uint8_t, ImVec2> node_screen_pos;
    for (const auto& node : m_graph_nodes) {
        ImVec2 screen_pos = ImVec2(
            canvas_center.x + (node.position.x + m_graph_offset.x) * m_graph_zoom,
            canvas_center.y + (node.position.y + m_graph_offset.y) * m_graph_zoom
        );
        node_screen_pos[node.material_id] = screen_pos;
    }

    // Helper to calculate point-to-line-segment distance
    auto point_to_segment_dist = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
        ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
        ImVec2 ap = ImVec2(p.x - a.x, p.y - a.y);
        float ab_len_sq = ab.x * ab.x + ab.y * ab.y;
        if (ab_len_sq < 0.001f) return sqrtf(ap.x * ap.x + ap.y * ap.y);
        float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / ab_len_sq, 0.0f, 1.0f);
        ImVec2 closest = ImVec2(a.x + t * ab.x, a.y + t * ab.y);
        ImVec2 diff = ImVec2(p.x - closest.x, p.y - closest.y);
        return sqrtf(diff.x * diff.x + diff.y * diff.y);
    };

    // Helper to calculate distance from point to quadratic bezier curve
    auto point_to_bezier_dist = [](ImVec2 p, ImVec2 p0, ImVec2 p1, ImVec2 p2) -> float {
        // Sample the curve at multiple points and find minimum distance
        float min_dist = FLT_MAX;
        constexpr int SAMPLES = 16;
        for (int i = 0; i <= SAMPLES; ++i) {
            float t = static_cast<float>(i) / SAMPLES;
            float u = 1.0f - t;
            // Quadratic bezier: B(t) = (1-t)^2*P0 + 2*(1-t)*t*P1 + t^2*P2
            ImVec2 pt = ImVec2(
                u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x,
                u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y
            );
            float dx = p.x - pt.x;
            float dy = p.y - pt.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < min_dist) min_dist = dist;
        }
        return min_dist;
    };

    // Check edge hover (before drawing so we can highlight)
    m_graph_hovered_edge = -1;
    float edge_hover_threshold = 10.0f;
    float min_hover_dist = edge_hover_threshold;
    for (size_t i = 0; i < m_graph_edges.size(); ++i) {
        const auto& edge = m_graph_edges[i];
        auto from_it = node_screen_pos.find(edge.from_id);
        auto to_it = node_screen_pos.find(edge.to_id);
        if (from_it == node_screen_pos.end() || to_it == node_screen_pos.end()) continue;

        ImVec2 from_pos = from_it->second;
        ImVec2 to_pos = to_it->second;

        float dist;
        if (edge.parallel_count > 1) {
            // Calculate the control point for the curve (same logic as draw function)
            ImVec2 dir = ImVec2(to_pos.x - from_pos.x, to_pos.y - from_pos.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len < 1.0f) continue;
            dir.x /= len;
            dir.y /= len;
            ImVec2 perp = ImVec2(-dir.y, dir.x);

            // Use canonical perpendicular (same as draw function)
            bool is_reverse_direction = (edge.from_id > edge.to_id);
            if (is_reverse_direction) {
                perp.x = -perp.x;
                perp.y = -perp.y;
            }

            float radius = NODE_RADIUS * m_graph_zoom;
            ImVec2 start = ImVec2(from_pos.x + dir.x * radius, from_pos.y + dir.y * radius);
            ImVec2 end = ImVec2(to_pos.x - dir.x * radius, to_pos.y - dir.y * radius);

            float curve_offset = 40.0f * m_graph_zoom;
            float offset_multiplier;
            if (edge.parallel_count == 2) {
                offset_multiplier = (edge.parallel_index == 0) ? -0.5f : 0.5f;
            } else {
                offset_multiplier = (static_cast<float>(edge.parallel_index) - (edge.parallel_count - 1) / 2.0f);
            }
            float offset = offset_multiplier * curve_offset;

            ImVec2 mid = ImVec2(
                (start.x + end.x) * 0.5f + perp.x * offset,
                (start.y + end.y) * 0.5f + perp.y * offset
            );

            dist = point_to_bezier_dist(mouse_pos, start, mid, end);
        } else {
            dist = point_to_segment_dist(mouse_pos, from_pos, to_pos);
        }

        if (dist < min_hover_dist) {
            min_hover_dist = dist;
            m_graph_hovered_edge = static_cast<int>(i);
        }
    }

    // Draw edges first (behind nodes)
    for (size_t i = 0; i < m_graph_edges.size(); ++i) {
        const auto& edge = m_graph_edges[i];
        auto from_it = node_screen_pos.find(edge.from_id);
        auto to_it = node_screen_pos.find(edge.to_id);
        if (from_it != node_screen_pos.end() && to_it != node_screen_pos.end()) {
            bool highlighted = (m_graph_hovered_edge == static_cast<int>(i));
            draw_interaction_edge(from_it->second, to_it->second, edge.from_id, edge.to_id,
                                  edge.type, edge.parallel_index, edge.parallel_count, highlighted);
        }
    }

    // Draw nodes and check hover
    m_graph_hovered_node = -1;

    for (size_t i = 0; i < m_graph_nodes.size(); ++i) {
        const auto& node = m_graph_nodes[i];
        const auto* mat = m_library->get_material(node.material_id);
        if (!mat) continue;

        ImVec2 screen_pos = node_screen_pos[node.material_id];
        bool selected = (m_selected_material >= 0 &&
                         m_library->get_all_materials()[m_selected_material].id == node.material_id);

        // Check hover
        float dist = sqrtf(
            (mouse_pos.x - screen_pos.x) * (mouse_pos.x - screen_pos.x) +
            (mouse_pos.y - screen_pos.y) * (mouse_pos.y - screen_pos.y)
        );
        if (dist <= NODE_RADIUS * m_graph_zoom) {
            m_graph_hovered_node = static_cast<int>(i);
        }

        draw_material_node(*mat, screen_pos, selected);
    }

    draw_list->PopClipRect();

    // Handle input
    ImGui::InvisibleButton("canvas", canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();

    // Node dragging with left mouse
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_graph_hovered_node >= 0) {
        m_graph_dragging_node = m_graph_hovered_node;
        ImVec2 node_screen = node_screen_pos[m_graph_nodes[m_graph_dragging_node].material_id];
        m_graph_drag_offset = ImVec2(mouse_pos.x - node_screen.x, mouse_pos.y - node_screen.y);
    }

    if (m_graph_dragging_node >= 0) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            // Update node position
            ImVec2 new_screen_pos = ImVec2(mouse_pos.x - m_graph_drag_offset.x, mouse_pos.y - m_graph_drag_offset.y);
            m_graph_nodes[m_graph_dragging_node].position.x = (new_screen_pos.x - canvas_center.x) / m_graph_zoom - m_graph_offset.x;
            m_graph_nodes[m_graph_dragging_node].position.y = (new_screen_pos.y - canvas_center.y) / m_graph_zoom - m_graph_offset.y;
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_graph_dragging_node = -1;
        }
    }

    // Click to select material (only if not dragging)
    if (is_hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_graph_dragging_node < 0) {
        if (m_graph_hovered_node >= 0) {
            uint8_t mat_id = m_graph_nodes[m_graph_hovered_node].material_id;
            // Find the index in the materials vector
            const auto& materials = m_library->get_all_materials();
            for (size_t i = 0; i < materials.size(); ++i) {
                if (materials[i].id == mat_id) {
                    m_selected_material = static_cast<int>(i);
                    m_editing_material = materials[i];
                    m_has_unsaved_changes = false;
                    m_selected_interaction = -1;
                    break;
                }
            }
        }
    }

    // Pan with middle mouse
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
        m_graph_offset.x += delta.x / m_graph_zoom;
        m_graph_offset.y += delta.y / m_graph_zoom;
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
    }

    // Zoom with scroll wheel
    if (is_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float zoom_factor = 1.1f;
            if (wheel > 0) {
                m_graph_zoom *= zoom_factor;
            } else {
                m_graph_zoom /= zoom_factor;
            }
            m_graph_zoom = std::clamp(m_graph_zoom, 0.2f, 3.0f);
        }
    }

    // Tooltip for hovered node (full properties)
    if (m_graph_hovered_node >= 0 && is_hovered && m_graph_dragging_node < 0) {
        uint8_t mat_id = m_graph_nodes[m_graph_hovered_node].material_id;
        const auto* mat = m_library->get_material(mat_id);
        if (mat) {
            ImGui::BeginTooltip();
            ImGui::Text("[%d] %s", mat->id, mat->name.c_str());
            ImGui::Separator();

            // Physical
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Physical:");
            const char* cat_name = "unknown";
            if (m_category_library) {
                const auto* cat = m_category_library->get_by_id(static_cast<uint8_t>(mat->category));
                if (cat) cat_name = cat->name.c_str();
            }
            ImGui::Text("  Category: %s", cat_name);
            ImGui::Text("  Density: %d", mat->density);

            // Thermal
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.7f, 1.0f), "Thermal:");
            ImGui::Text("  Default Temp: %d", mat->default_temp);
            ImGui::Text("  Conductivity: %.2f", mat->conductivity);

            // Rendering
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "Rendering:");
            float col[4];
            uint32_to_color(mat->color, col);
            ImGui::Text("  Color: ");
            ImGui::SameLine();
            ImGui::ColorButton("##tooltip_color", ImVec4(col[0], col[1], col[2], col[3]),
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14, 14));

            // Flags
            if (mat->flags != 0) {
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.7f, 1.0f), "Flags:");
                if (mat->flags & engine::simulation::MAT_FLAG_HAZARD) ImGui::Text("  - Hazard");
                if (mat->flags & engine::simulation::MAT_FLAG_FLAMMABLE) ImGui::Text("  - Flammable");
                if (mat->flags & engine::simulation::MAT_FLAG_CONDUCTIVE) ImGui::Text("  - Conductive");
            }

            ImGui::Text("Interactions: %zu", mat->interactions.size());
            ImGui::EndTooltip();
        }
    }

    // Tooltip for hovered edge (interaction details)
    if (m_graph_hovered_edge >= 0 && is_hovered && m_graph_hovered_node < 0 && m_graph_dragging_node < 0) {
        const auto& edge = m_graph_edges[m_graph_hovered_edge];
        const auto* from_mat = m_library->get_material(edge.from_id);
        const auto* to_mat = m_library->get_material(edge.to_id);

        ImGui::BeginTooltip();
        if (from_mat && to_mat) {
            ImGui::Text("%s -> %s", from_mat->name.c_str(), to_mat->name.c_str());
        }
        ImGui::Separator();
        ImGui::Text("ID: %s", edge.interaction_id.c_str());
        ImGui::Text("Type: %s", engine::simulation::interaction_type_to_string(edge.type));
        ImGui::Text("Priority: %d", edge.priority);
        ImGui::Text("Probability: %.0f%%", edge.probability * 100.0f);
        if (edge.sim_step_threshold > 0) {
            ImGui::Text("Step Threshold: %d", edge.sim_step_threshold);
        }

        // Conditions
        if (edge.temp_above > 0 || edge.temp_below < 255) {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.7f, 1.0f), "Conditions:");
            if (edge.temp_above > 0) ImGui::Text("  Temp > %d", edge.temp_above);
            if (edge.temp_below < 255) ImGui::Text("  Temp < %d", edge.temp_below);
        }
        if (!edge.contact_with.empty()) {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.7f, 1.0f), "Contact with:");
            for (const auto& c : edge.contact_with) {
                ImGui::Text("  - %s", c.c_str());
            }
        }

        // Effect
        if (!edge.effect_desc.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Effect:");
            ImGui::Text("  %s", edge.effect_desc.c_str());
        }
        ImGui::EndTooltip();
    }
}

void MaterialEditorPanel::build_graph() {
    m_graph_nodes.clear();
    m_graph_edges.clear();

    if (!m_library) return;

    const auto& materials = m_library->get_all_materials();

    // Create nodes for all non-empty materials
    for (const auto& mat : materials) {
        if (mat.internal_name.empty()) continue;

        GraphNode node;
        node.material_id = mat.id;
        // Initial position in a circle
        float angle = static_cast<float>(m_graph_nodes.size()) * 0.618f * 6.28318f;
        float radius = 100.0f + m_graph_nodes.size() * 15.0f;
        node.position = ImVec2(cosf(angle) * radius, sinf(angle) * radius);
        node.velocity = ImVec2(0, 0);
        m_graph_nodes.push_back(node);
    }

    // Create edges from interactions
    for (const auto& mat : materials) {
        for (const auto& interaction : mat.interactions) {
            for (const auto& effect : interaction.effects) {
                if (effect.type == engine::simulation::EffectType::TRANSFORM ||
                    effect.type == engine::simulation::EffectType::TRANSFORM_NEIGHBOR) {
                    // Find target material ID
                    const auto* target = m_library->get_material(effect.material_name);
                    if (target) {
                        GraphEdge edge;
                        edge.from_id = mat.id;
                        edge.to_id = target->id;
                        edge.type = interaction.type;

                        // Store interaction details for tooltip
                        edge.interaction_id = interaction.id;
                        edge.priority = interaction.priority;
                        edge.probability = interaction.probability;
                        edge.sim_step_threshold = interaction.sim_step_threshold;
                        edge.temp_above = interaction.conditions.temp_above;
                        edge.temp_below = interaction.conditions.temp_below;
                        edge.contact_with = interaction.conditions.contact_with;

                        // Build effect description
                        std::string effect_type_str = engine::simulation::effect_type_to_string(effect.type);
                        if (!effect.material_name.empty()) {
                            edge.effect_desc = effect_type_str + " -> " + effect.material_name;
                        } else {
                            edge.effect_desc = effect_type_str;
                        }

                        m_graph_edges.push_back(edge);
                    }
                }
            }
        }
    }

    // Post-process: detect parallel edges and assign indices
    // Group edges by their node pair (min_id, max_id) to find all edges between same nodes
    std::unordered_map<uint32_t, std::vector<size_t>> edge_groups;
    for (size_t i = 0; i < m_graph_edges.size(); ++i) {
        const auto& edge = m_graph_edges[i];
        // Create a unique key for this node pair (order-independent)
        uint8_t min_id = std::min(edge.from_id, edge.to_id);
        uint8_t max_id = std::max(edge.from_id, edge.to_id);
        uint32_t key = (static_cast<uint32_t>(min_id) << 8) | max_id;
        edge_groups[key].push_back(i);
    }

    // Assign parallel indices
    for (const auto& [key, indices] : edge_groups) {
        int count = static_cast<int>(indices.size());
        for (int i = 0; i < count; ++i) {
            m_graph_edges[indices[i]].parallel_index = i;
            m_graph_edges[indices[i]].parallel_count = count;
        }
    }
}

void MaterialEditorPanel::layout_graph() {
    // Simple force-directed layout
    constexpr float REPULSION = 5000.0f;
    constexpr float ATTRACTION = 0.01f;
    constexpr float DAMPING = 0.9f;
    constexpr float MIN_DIST = 60.0f;
    constexpr int ITERATIONS = 50;

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        // Reset forces
        for (auto& node : m_graph_nodes) {
            node.velocity = ImVec2(0, 0);
        }

        // Repulsion between all nodes
        for (size_t i = 0; i < m_graph_nodes.size(); ++i) {
            for (size_t j = i + 1; j < m_graph_nodes.size(); ++j) {
                ImVec2 diff = ImVec2(
                    m_graph_nodes[i].position.x - m_graph_nodes[j].position.x,
                    m_graph_nodes[i].position.y - m_graph_nodes[j].position.y
                );
                float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);
                if (dist < MIN_DIST) dist = MIN_DIST;

                float force = REPULSION / (dist * dist);
                ImVec2 dir = ImVec2(diff.x / dist, diff.y / dist);

                m_graph_nodes[i].velocity.x += dir.x * force;
                m_graph_nodes[i].velocity.y += dir.y * force;
                m_graph_nodes[j].velocity.x -= dir.x * force;
                m_graph_nodes[j].velocity.y -= dir.y * force;
            }
        }

        // Attraction along edges
        for (const auto& edge : m_graph_edges) {
            // Find nodes
            GraphNode* from_node = nullptr;
            GraphNode* to_node = nullptr;
            for (auto& node : m_graph_nodes) {
                if (node.material_id == edge.from_id) from_node = &node;
                if (node.material_id == edge.to_id) to_node = &node;
            }
            if (!from_node || !to_node) continue;

            ImVec2 diff = ImVec2(
                to_node->position.x - from_node->position.x,
                to_node->position.y - from_node->position.y
            );
            float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);
            if (dist < 1.0f) continue;

            float force = dist * ATTRACTION;
            ImVec2 dir = ImVec2(diff.x / dist, diff.y / dist);

            from_node->velocity.x += dir.x * force;
            from_node->velocity.y += dir.y * force;
            to_node->velocity.x -= dir.x * force;
            to_node->velocity.y -= dir.y * force;
        }

        // Apply velocities with damping
        for (auto& node : m_graph_nodes) {
            node.position.x += node.velocity.x * DAMPING;
            node.position.y += node.velocity.y * DAMPING;
        }
    }

    m_graph_needs_layout = false;
}

void MaterialEditorPanel::draw_material_node(const engine::simulation::MaterialDefinition& mat,
                                              ImVec2 pos, bool selected) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float radius = NODE_RADIUS * m_graph_zoom;

    // Get material color
    float color[4];
    uint32_to_color(mat.color, color);

    // Node fill color (use material color)
    ImU32 fill_color = IM_COL32(
        static_cast<int>(color[0] * 255),
        static_cast<int>(color[1] * 255),
        static_cast<int>(color[2] * 255),
        200
    );

    // Border color
    ImU32 border_color;
    if (selected) {
        border_color = IM_COL32(255, 200, 50, 255);
    } else if (m_graph_hovered_node >= 0 &&
               m_graph_nodes[m_graph_hovered_node].material_id == mat.id) {
        border_color = IM_COL32(150, 150, 200, 255);
    } else {
        border_color = IM_COL32(100, 100, 100, 255);
    }

    // Draw circle
    draw_list->AddCircleFilled(pos, radius, fill_color);
    draw_list->AddCircle(pos, radius, border_color, 0, selected ? 3.0f : 1.5f);

    // Draw label
    const char* name = mat.name.c_str();
    ImVec2 text_size = ImGui::CalcTextSize(name);

    // Truncate if too long
    char truncated[16];
    if (text_size.x > radius * 1.8f) {
        strncpy(truncated, name, 8);
        truncated[8] = '\0';
        strcat(truncated, "..");
        name = truncated;
        text_size = ImGui::CalcTextSize(name);
    }

    ImVec2 text_pos = ImVec2(
        pos.x - text_size.x * 0.5f,
        pos.y - text_size.y * 0.5f
    );

    // Text shadow for readability
    draw_list->AddText(ImVec2(text_pos.x + 1, text_pos.y + 1), IM_COL32(0, 0, 0, 180), name);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), name);

    // Category indicator below node
    const char* cat_name = nullptr;
    switch (mat.category) {
        case engine::simulation::CAT_POWDER: cat_name = "P"; break;
        case engine::simulation::CAT_LIQUID: cat_name = "L"; break;
        case engine::simulation::CAT_GAS:    cat_name = "G"; break;
        case engine::simulation::CAT_STATIC: cat_name = "S"; break;
        default: break;
    }
    if (cat_name) {
        ImVec2 cat_pos = ImVec2(pos.x - 4, pos.y + radius + 2);
        draw_list->AddText(cat_pos, IM_COL32(180, 180, 180, 200), cat_name);
    }
}

void MaterialEditorPanel::draw_interaction_edge(ImVec2 from, ImVec2 to,
                                                 uint8_t from_id, uint8_t to_id,
                                                 engine::simulation::InteractionType type,
                                                 int parallel_index, int parallel_count,
                                                 bool highlighted) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Edge color based on interaction type (brighter when highlighted)
    ImU32 edge_color;
    int alpha = highlighted ? 255 : 200;
    switch (type) {
        case engine::simulation::InteractionType::TEMPERATURE:
            edge_color = IM_COL32(100, 150, 255, alpha); // Blue
            break;
        case engine::simulation::InteractionType::CONTACT:
            edge_color = IM_COL32(255, 100, 100, alpha); // Red
            break;
        case engine::simulation::InteractionType::TIME_DECAY:
            edge_color = IM_COL32(150, 150, 150, alpha); // Gray
            break;
        case engine::simulation::InteractionType::RANDOM:
            edge_color = IM_COL32(255, 180, 100, alpha); // Orange
            break;
        default:
            edge_color = IM_COL32(100, 200, 100, alpha); // Green
            break;
    }

    // Calculate direction and perpendicular
    ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.0f) return;
    dir.x /= len;
    dir.y /= len;
    ImVec2 perp = ImVec2(-dir.y, dir.x);

    // For consistent curve direction across parallel edges, use canonical direction
    // based on node IDs. If this edge goes from larger to smaller ID, flip the
    // perpendicular so all edges between the same nodes curve consistently.
    bool is_reverse_direction = (from_id > to_id);
    if (is_reverse_direction) {
        perp.x = -perp.x;
        perp.y = -perp.y;
    }

    // Offset from node centers (to not overlap with node circles)
    float radius = NODE_RADIUS * m_graph_zoom;
    ImVec2 start = ImVec2(from.x + dir.x * radius, from.y + dir.y * radius);
    ImVec2 end = ImVec2(to.x - dir.x * radius, to.y - dir.y * radius);

    float thickness = highlighted ? 4.0f : 2.0f;

    // If there are parallel edges, curve them
    if (parallel_count > 1) {
        // Calculate curve offset: spread edges evenly on both sides
        float curve_offset = 40.0f * m_graph_zoom;

        // Calculate offset for this edge
        // Spread them evenly across the parallel edges
        float offset_multiplier;
        if (parallel_count == 2) {
            offset_multiplier = (parallel_index == 0) ? -0.5f : 0.5f;
        } else {
            // For more edges, spread them evenly
            offset_multiplier = (static_cast<float>(parallel_index) - (parallel_count - 1) / 2.0f);
        }
        float offset = offset_multiplier * curve_offset;

        // Control point for quadratic bezier (at midpoint, offset perpendicular)
        ImVec2 mid = ImVec2(
            (start.x + end.x) * 0.5f + perp.x * offset,
            (start.y + end.y) * 0.5f + perp.y * offset
        );

        // Draw quadratic bezier curve
        draw_list->AddBezierQuadratic(start, mid, end, edge_color, thickness);

        // For arrowhead, we need the direction at the end of the curve
        // Derivative of quadratic bezier at t=1 is: 2*(P2-P1) = 2*(end-mid)
        ImVec2 end_dir = ImVec2(end.x - mid.x, end.y - mid.y);
        float end_len = sqrtf(end_dir.x * end_dir.x + end_dir.y * end_dir.y);
        if (end_len > 0.01f) {
            end_dir.x /= end_len;
            end_dir.y /= end_len;
        } else {
            end_dir = dir;
        }

        // Draw arrowhead aligned with curve direction at end
        float arrow_size = 10.0f * m_graph_zoom;
        ImVec2 arrow_perp = ImVec2(-end_dir.y, end_dir.x);
        ImVec2 arrow_tip = end;
        ImVec2 arrow_left = ImVec2(
            end.x - end_dir.x * arrow_size + arrow_perp.x * arrow_size * 0.5f,
            end.y - end_dir.y * arrow_size + arrow_perp.y * arrow_size * 0.5f
        );
        ImVec2 arrow_right = ImVec2(
            end.x - end_dir.x * arrow_size - arrow_perp.x * arrow_size * 0.5f,
            end.y - end_dir.y * arrow_size - arrow_perp.y * arrow_size * 0.5f
        );
        draw_list->AddTriangleFilled(arrow_tip, arrow_left, arrow_right, edge_color);
    } else {
        // Single edge: draw straight line
        draw_list->AddLine(start, end, edge_color, thickness);

        // Draw arrowhead
        float arrow_size = 10.0f * m_graph_zoom;
        ImVec2 arrow_tip = end;
        ImVec2 arrow_left = ImVec2(
            end.x - dir.x * arrow_size + perp.x * arrow_size * 0.5f,
            end.y - dir.y * arrow_size + perp.y * arrow_size * 0.5f
        );
        ImVec2 arrow_right = ImVec2(
            end.x - dir.x * arrow_size - perp.x * arrow_size * 0.5f,
            end.y - dir.y * arrow_size - perp.y * arrow_size * 0.5f
        );
        draw_list->AddTriangleFilled(arrow_tip, arrow_left, arrow_right, edge_color);
    }
}

// ============================================================================
// Categories Tab
// ============================================================================

bool MaterialEditorPanel::is_selected_category_readonly() const {
    if (m_selected_category < 0) return true;
    return m_editing_category.is_engine_default;
}

void MaterialEditorPanel::render_categories_tab() {
    if (!m_category_library) {
        ImGui::Text("No category library loaded.");
        return;
    }

    // Toolbar
    if (ImGui::Button("New")) {
        m_new_category_id = m_category_library->next_available_id();
        m_new_category_name[0] = '\0';
        m_new_category_internal_name[0] = '\0';
        m_show_new_category_dialog = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        save_current_category();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload")) {
        reload_categories();
    }

    ImGui::SameLine();

    if (m_has_unsaved_category_changes) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(unsaved changes)");
    }

    // Filter
    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##cat_filter", "Filter...", m_category_filter_text, sizeof(m_category_filter_text));

    ImGui::Separator();

    // Two-column layout
    float list_width = 180.0f;

    ImGui::BeginChild("CategoryList", ImVec2(list_width, 0), true);
    render_category_list();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CategoryProperties", ImVec2(0, 0), true);
    render_category_properties();
    ImGui::EndChild();

    // New category dialog
    if (m_show_new_category_dialog) {
        ImGui::OpenPopup("New Category");
    }

    if (ImGui::BeginPopupModal("New Category", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Display Name", m_new_category_name, sizeof(m_new_category_name));
        ImGui::InputText("Internal Name", m_new_category_internal_name, sizeof(m_new_category_internal_name));
        ImGui::InputInt("Category ID", &m_new_category_id);

        // Clamp ID to valid range (1-15, 0 is EMPTY)
        if (m_new_category_id < 1) m_new_category_id = 1;
        if (m_new_category_id > 15) m_new_category_id = 15;

        // Check if ID is already taken
        bool id_taken = (m_category_library->get_by_id(static_cast<uint8_t>(m_new_category_id)) != nullptr);
        if (id_taken) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ID %d is already in use!", m_new_category_id);
        }

        ImGui::Separator();

        bool can_create = strlen(m_new_category_name) > 0 &&
                          strlen(m_new_category_internal_name) > 0 &&
                          !id_taken;

        if (!can_create) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            create_new_category();
            m_show_new_category_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        if (!can_create) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_new_category_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Delete confirmation modal
    if (m_pending_delete_category_id >= 0) {
        ImGui::OpenPopup("Delete Category?");
    }

    if (ImGui::BeginPopupModal("Delete Category?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete \"%s\"?", m_pending_delete_category_name.c_str());
        ImGui::Text("This will delete the .phys file permanently.");
        ImGui::Spacing();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            delete_category(static_cast<uint8_t>(m_pending_delete_category_id));
            m_pending_delete_category_id = -1;
            m_pending_delete_category_name.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pending_delete_category_id = -1;
            m_pending_delete_category_name.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MaterialEditorPanel::render_category_list() {
    const auto& categories = m_category_library->all_categories();

    // Build sorted index list by category ID
    std::vector<size_t> sorted_indices(categories.size());
    for (size_t i = 0; i < categories.size(); ++i) {
        sorted_indices[i] = i;
    }
    std::sort(sorted_indices.begin(), sorted_indices.end(),
        [&categories](size_t a, size_t b) {
            return categories[a].id < categories[b].id;
        });

    for (size_t idx : sorted_indices) {
        const auto& cat = categories[idx];
        size_t i = idx;  // Use original index for selection tracking

        // Apply filter
        if (m_category_filter_text[0] != '\0') {
            std::string filter_lower = m_category_filter_text;
            std::string name_lower = cat.name;
            std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name_lower.find(filter_lower) == std::string::npos) continue;
        }

        ImGui::PushID(static_cast<int>(i));

        // Category indicator
        const char* indicator = cat.mobile ? "M" : "S";
        ImU32 indicator_color = cat.mobile ? IM_COL32(100, 200, 100, 255) : IM_COL32(150, 150, 150, 255);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(indicator_color), "%s", indicator);
        ImGui::SameLine();

        // Category name
        char label[256];
        if (cat.is_engine_default) {
            snprintf(label, sizeof(label), "[%d] %s (engine)", cat.id, cat.name.c_str());
        } else {
            snprintf(label, sizeof(label), "[%d] %s", cat.id, cat.name.c_str());
        }

        bool selected = (m_selected_category == static_cast<int>(i));
        if (ImGui::Selectable(label, selected)) {
            if (m_selected_category != static_cast<int>(i)) {
                m_selected_category = static_cast<int>(i);
                m_editing_category = cat;
                m_has_unsaved_category_changes = false;
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Edit")) {
                m_selected_category = static_cast<int>(i);
                m_editing_category = cat;
                m_has_unsaved_category_changes = false;
            }

            // Only allow delete for non-engine categories and not EMPTY
            bool can_delete = !cat.is_engine_default && cat.id != engine::simulation::EMPTY_CATEGORY_ID;
            if (ImGui::MenuItem("Delete", nullptr, false, can_delete)) {
                m_pending_delete_category_id = cat.id;
                m_pending_delete_category_name = cat.name;
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void MaterialEditorPanel::render_category_properties() {
    if (m_selected_category < 0) {
        ImGui::Text("Select a category to edit.");
        return;
    }

    bool readonly = is_selected_category_readonly();

    // Category header
    ImGui::Text("ID: %d", m_editing_category.id);
    ImGui::SameLine();
    ImGui::Text("Internal: %s", m_editing_category.internal_name.c_str());

    if (readonly) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "(read-only engine category)");
    }

    // Display name
    if (readonly) ImGui::BeginDisabled();
    char name_buf[128];
    strncpy(name_buf, m_editing_category.name.c_str(), sizeof(name_buf));
    if (ImGui::InputText("Display Name", name_buf, sizeof(name_buf))) {
        m_editing_category.name = name_buf;
        m_has_unsaved_category_changes = true;
    }
    if (readonly) ImGui::EndDisabled();

    ImGui::Separator();

    // Preview (always visible)
    if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        render_category_preview(m_editing_category);
    }

    // General properties
    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (readonly) ImGui::BeginDisabled();

        if (ImGui::Checkbox("Mobile", &m_editing_category.mobile)) {
            m_has_unsaved_category_changes = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, pixels in this category can move according to the rules below.");
        }

        if (ImGui::Checkbox("Randomize Equal Priority", &m_editing_category.randomize_equal_priority)) {
            m_has_unsaved_category_changes = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, directions with the same priority will be tried in random order each tick.");
        }

        if (readonly) ImGui::EndDisabled();
    }

    // Movement rules
    render_movement_rules_editor(m_editing_category, readonly);

    // Dissipation
    render_dissipation_editor(m_editing_category, readonly);
}

void MaterialEditorPanel::render_category_preview(const engine::simulation::CategoryDefinition& cat) {
    // Draw a 3x3 grid with arrows showing movement directions
    float preview_size = 150.0f;
    float cell_size = preview_size / 3.0f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(cursor.x + preview_size * 0.5f, cursor.y + preview_size * 0.5f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(cursor,
        ImVec2(cursor.x + preview_size, cursor.y + preview_size),
        IM_COL32(40, 40, 45, 255));

    // Grid lines
    ImU32 grid_color = IM_COL32(70, 70, 75, 255);
    for (int i = 1; i < 3; ++i) {
        draw_list->AddLine(
            ImVec2(cursor.x + i * cell_size, cursor.y),
            ImVec2(cursor.x + i * cell_size, cursor.y + preview_size),
            grid_color);
        draw_list->AddLine(
            ImVec2(cursor.x, cursor.y + i * cell_size),
            ImVec2(cursor.x + preview_size, cursor.y + i * cell_size),
            grid_color);
    }

    // Center dot (the pixel)
    draw_list->AddCircleFilled(center, 8.0f, IM_COL32(200, 200, 200, 255));

    // Draw arrows for each movement rule
    // Scale arrows to reach the center of adjacent cells
    float arrow_scale = cell_size * 0.9f;
    for (const auto& rule : cat.movement_rules) {
        draw_direction_arrow(draw_list, center, rule.direction, rule.priority, rule.density_check, arrow_scale);
    }

    // Reserve space
    ImGui::Dummy(ImVec2(preview_size, preview_size));

    // Legend
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "P0");
    ImGui::SameLine();
    ImGui::Text("= Priority 0 (highest)");

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.6f, 1.0f), "P1");
    ImGui::SameLine();
    ImGui::Text("= Priority 1");

    ImGui::TextDisabled("Solid = density check, Dashed = no check");
}

void MaterialEditorPanel::draw_direction_arrow(ImDrawList* dl, ImVec2 center,
                                                 engine::simulation::Direction dir,
                                                 uint8_t priority, bool density_check,
                                                 float scale) {
    using namespace engine::simulation;

    if (dir == Direction::NONE) return;

    // Get direction offset
    int dx = direction_dx(dir);
    int dy = direction_dy(dir);

    // Calculate arrow endpoint
    ImVec2 arrow_end = ImVec2(
        center.x + dx * scale,
        center.y + dy * scale
    );

    // Color based on priority
    ImU32 color;
    switch (priority) {
        case 0: color = IM_COL32(100, 200, 100, 255); break;  // Green for P0
        case 1: color = IM_COL32(200, 200, 100, 255); break;  // Yellow for P1
        case 2: color = IM_COL32(200, 150, 100, 255); break;  // Orange for P2
        default: color = IM_COL32(150, 150, 150, 255); break; // Gray for others
    }

    float thickness = (priority == 0) ? 3.0f : 2.0f;

    if (density_check) {
        // Solid line
        dl->AddLine(center, arrow_end, color, thickness);
    } else {
        // Dashed line (draw multiple segments)
        ImVec2 diff = ImVec2(arrow_end.x - center.x, arrow_end.y - center.y);
        float len = sqrtf(diff.x * diff.x + diff.y * diff.y);
        ImVec2 dir_norm = ImVec2(diff.x / len, diff.y / len);

        float dash_len = 6.0f;
        float gap_len = 4.0f;
        float pos = 0.0f;
        bool drawing = true;

        while (pos < len) {
            float segment_len = drawing ? dash_len : gap_len;
            if (pos + segment_len > len) segment_len = len - pos;

            if (drawing) {
                ImVec2 start = ImVec2(center.x + dir_norm.x * pos, center.y + dir_norm.y * pos);
                ImVec2 end = ImVec2(center.x + dir_norm.x * (pos + segment_len), center.y + dir_norm.y * (pos + segment_len));
                dl->AddLine(start, end, color, thickness);
            }

            pos += segment_len;
            drawing = !drawing;
        }
    }

    // Arrowhead
    float arrow_size = 8.0f;
    ImVec2 dir_vec = ImVec2(arrow_end.x - center.x, arrow_end.y - center.y);
    float len = sqrtf(dir_vec.x * dir_vec.x + dir_vec.y * dir_vec.y);
    if (len > 0.01f) {
        dir_vec.x /= len;
        dir_vec.y /= len;

        ImVec2 perp = ImVec2(-dir_vec.y, dir_vec.x);
        ImVec2 arrow_left = ImVec2(
            arrow_end.x - dir_vec.x * arrow_size + perp.x * arrow_size * 0.5f,
            arrow_end.y - dir_vec.y * arrow_size + perp.y * arrow_size * 0.5f
        );
        ImVec2 arrow_right = ImVec2(
            arrow_end.x - dir_vec.x * arrow_size - perp.x * arrow_size * 0.5f,
            arrow_end.y - dir_vec.y * arrow_size - perp.y * arrow_size * 0.5f
        );
        dl->AddTriangleFilled(arrow_end, arrow_left, arrow_right, color);
    }

    // Priority label
    char priority_label[8];
    snprintf(priority_label, sizeof(priority_label), "P%d", priority);
    ImVec2 label_pos = ImVec2(arrow_end.x + 4, arrow_end.y - 6);
    dl->AddText(label_pos, color, priority_label);
}

void MaterialEditorPanel::render_movement_rules_editor(engine::simulation::CategoryDefinition& cat, bool readonly) {
    using namespace engine::simulation;

    if (ImGui::CollapsingHeader("Movement Rules", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (readonly) ImGui::BeginDisabled();

        // Add rule button (max 8 rules)
        if (cat.movement_rules.size() < MAX_MOVEMENT_RULES) {
            if (ImGui::Button("+ Add Rule")) {
                MovementRule new_rule;
                new_rule.direction = Direction::DOWN;
                new_rule.priority = 0;
                new_rule.density_check = true;
                cat.movement_rules.push_back(new_rule);
                m_has_unsaved_category_changes = true;
            }
        } else {
            ImGui::TextDisabled("Max %d rules reached", MAX_MOVEMENT_RULES);
        }

        if (readonly) ImGui::EndDisabled();

        ImGui::Separator();

        // List rules
        for (size_t i = 0; i < cat.movement_rules.size(); ++i) {
            auto& rule = cat.movement_rules[i];

            ImGui::PushID(static_cast<int>(i));

            // Direction dropdown
            const char* directions[] = { "down", "down_left", "down_right", "left", "right", "up", "up_left", "up_right" };
            int dir_idx = static_cast<int>(rule.direction);
            if (dir_idx > 7) dir_idx = 0;

            if (readonly) ImGui::BeginDisabled();

            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("##dir", &dir_idx, directions, IM_ARRAYSIZE(directions))) {
                rule.direction = static_cast<Direction>(dir_idx);
                m_has_unsaved_category_changes = true;
            }

            ImGui::SameLine();

            // Priority
            int priority = rule.priority;
            ImGui::SetNextItemWidth(60);
            if (ImGui::InputInt("##priority", &priority)) {
                rule.priority = static_cast<uint8_t>(std::clamp(priority, 0, 15));
                m_has_unsaved_category_changes = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Priority (0 = highest, checked first)");
            }

            ImGui::SameLine();

            // Density check
            if (ImGui::Checkbox("Density", &rule.density_check)) {
                m_has_unsaved_category_changes = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Only swap with target if it has lower density");
            }

            ImGui::SameLine();

            // Delete button
            if (ImGui::SmallButton("X")) {
                cat.movement_rules.erase(cat.movement_rules.begin() + i);
                m_has_unsaved_category_changes = true;
                if (readonly) ImGui::EndDisabled();
                ImGui::PopID();
                break;
            }

            if (readonly) ImGui::EndDisabled();

            // Swap with categories
            ImGui::Text("  Swap with:");
            ImGui::SameLine();

            // Show current swap_with as tags
            for (size_t j = 0; j < rule.swap_with.size(); ++j) {
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(j));
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "%s", rule.swap_with[j].c_str());
                if (!readonly) {
                    ImGui::SameLine(0, 2);
                    if (ImGui::SmallButton("x")) {
                        rule.swap_with.erase(rule.swap_with.begin() + static_cast<ptrdiff_t>(j));
                        m_has_unsaved_category_changes = true;
                    }
                }
                ImGui::PopID();
            }

            // Add category dropdown
            if (!readonly) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                if (ImGui::BeginCombo("##add_swap", "+Add")) {
                    const auto& categories = m_category_library->all_categories();
                    for (const auto& c : categories) {
                        // Check if already in list
                        bool already_added = std::find(rule.swap_with.begin(), rule.swap_with.end(),
                                                       c.internal_name) != rule.swap_with.end();
                        if (already_added) continue;

                        if (ImGui::Selectable(c.name.c_str())) {
                            rule.swap_with.push_back(c.internal_name);
                            m_has_unsaved_category_changes = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
}

void MaterialEditorPanel::render_dissipation_editor(engine::simulation::CategoryDefinition& cat, bool readonly) {
    if (ImGui::CollapsingHeader("Dissipation")) {
        if (readonly) ImGui::BeginDisabled();

        if (ImGui::Checkbox("Enabled", &cat.dissipation.enabled)) {
            m_has_unsaved_category_changes = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, pixels in this category will gradually transform into another material");
        }

        if (cat.dissipation.enabled) {
            if (ImGui::SliderFloat("Rate", &cat.dissipation.rate, 0.0f, 1.0f, "%.3f")) {
                m_has_unsaved_category_changes = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Probability (0.0 - 1.0) per tick to dissipate");
            }

            // Material dropdown for "into"
            std::string preview = cat.dissipation.into_material.empty() ? "(Select material)" : cat.dissipation.into_material;
            if (m_library && ImGui::BeginCombo("Into", preview.c_str())) {
                const auto& materials = m_library->get_all_materials();
                for (const auto& mat : materials) {
                    if (mat.internal_name.empty()) continue;

                    bool is_selected = (cat.dissipation.into_material == mat.internal_name);
                    if (ImGui::Selectable(mat.name.c_str(), is_selected)) {
                        cat.dissipation.into_material = mat.internal_name;
                        m_has_unsaved_category_changes = true;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (readonly) ImGui::EndDisabled();
    }
}

void MaterialEditorPanel::save_current_category() {
    if (m_editing_category.internal_name.empty()) return;
    if (is_selected_category_readonly()) return;

    namespace fs = std::filesystem;

    // Use existing source_path if available, otherwise create new path in project Assets
    std::string filename;
    if (!m_editing_category.source_path.empty()) {
        filename = m_editing_category.source_path;
    } else {
        // Determine save path - use project Assets folder directly
        std::string save_dir = m_categories_path;
        const std::string& project_path = m_context.scene_state().project_path();
        if (!project_path.empty()) {
            save_dir = (fs::path(project_path) / "Assets").string();
        }

        filename = save_dir + "/" + m_editing_category.internal_name + ".phys";
        m_editing_category.source_path = filename;
    }

    if (m_category_library->save_category(m_editing_category, filename)) {
        m_has_unsaved_category_changes = false;

        // Update the library
        m_category_library->add_or_update(m_editing_category);

        // Re-select the category
        uint8_t saved_id = m_editing_category.id;
        const auto& categories = m_category_library->all_categories();
        for (size_t i = 0; i < categories.size(); ++i) {
            if (categories[i].id == saved_id) {
                m_selected_category = static_cast<int>(i);
                m_editing_category = categories[i];
                break;
            }
        }

        // Update GPU tables
        m_context.update_material_tables();

        // Refresh file browser
        m_context.refresh_file_browser();

        engine::Logger::instance().info("MaterialEditor", "Saved category to: %s", filename.c_str());
    } else {
        engine::Logger::instance().error("MaterialEditor", "Failed to save category: %s", filename.c_str());
    }
}

void MaterialEditorPanel::create_new_category() {
    if (strlen(m_new_category_name) == 0 || strlen(m_new_category_internal_name) == 0) return;

    // Validate ID is not taken
    if (m_category_library->get_by_id(static_cast<uint8_t>(m_new_category_id)) != nullptr) {
        return;
    }

    namespace fs = std::filesystem;

    // Determine save path - use project Assets folder directly
    std::string save_dir = m_categories_path;
    const std::string& project_path = m_context.scene_state().project_path();
    if (!project_path.empty()) {
        save_dir = (fs::path(project_path) / "Assets").string();
    }

    engine::simulation::CategoryDefinition new_cat;
    new_cat.id = static_cast<uint8_t>(m_new_category_id);
    new_cat.name = m_new_category_name;
    new_cat.internal_name = m_new_category_internal_name;
    new_cat.mobile = true;
    new_cat.randomize_equal_priority = true;
    new_cat.is_engine_default = false;
    new_cat.source_path = save_dir + "/" + std::string(m_new_category_internal_name) + ".phys";

    // Add a default down rule
    engine::simulation::MovementRule down_rule;
    down_rule.direction = engine::simulation::Direction::DOWN;
    down_rule.priority = 0;
    down_rule.density_check = true;
    down_rule.swap_with.push_back("empty");
    new_cat.movement_rules.push_back(down_rule);

    // Save to file
    if (m_category_library->save_category(new_cat, new_cat.source_path)) {
        // Add to library
        m_category_library->add_or_update(new_cat);

        // Reload to pick up new category and re-sort
        reload_categories();

        // Select the newly created category
        const auto& categories = m_category_library->all_categories();
        for (size_t i = 0; i < categories.size(); ++i) {
            if (categories[i].id == new_cat.id) {
                m_selected_category = static_cast<int>(i);
                m_editing_category = categories[i];
                m_has_unsaved_category_changes = false;
                break;
            }
        }

        // Update GPU tables
        m_context.update_material_tables();

        // Refresh file browser
        m_context.refresh_file_browser();

        engine::Logger::instance().info("MaterialEditor", "Created category: %s", new_cat.source_path.c_str());
    } else {
        engine::Logger::instance().error("MaterialEditor", "Failed to create category: %s", new_cat.source_path.c_str());
    }
}

void MaterialEditorPanel::delete_category(uint8_t category_id) {
    if (!m_category_library) return;

    // Cannot delete EMPTY or engine categories
    if (category_id == engine::simulation::EMPTY_CATEGORY_ID) return;

    const auto* cat = m_category_library->get_by_id(category_id);
    if (!cat || cat->is_engine_default) return;

    // Delete the file
    namespace fs = std::filesystem;
    if (!cat->source_path.empty() && fs::exists(cat->source_path)) {
        fs::remove(cat->source_path);
    }

    // Remove from library
    m_category_library->remove_by_id(category_id);

    // Clear selection if we deleted the selected category
    if (m_selected_category >= 0 && m_editing_category.id == category_id) {
        m_selected_category = -1;
        m_has_unsaved_category_changes = false;
    }

    // Update GPU tables
    m_context.update_material_tables();

    // Refresh file browser
    m_context.refresh_file_browser();

    engine::Logger::instance().info("MaterialEditor", "Deleted category ID %d", category_id);
}

} // namespace editor
