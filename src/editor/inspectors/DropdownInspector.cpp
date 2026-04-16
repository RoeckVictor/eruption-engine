#include "DropdownInspector.h"
#include "InspectorUtils.h"
#include "engine/ui/Dropdown.h"
#include "engine/core/Hierarchy.h"
#include "editor/core/EditorComponents.h"
#include <imgui.h>
#include <algorithm>

namespace editor {

// Recursively collect all descendant entities
static void collect_descendants(entt::registry& registry, entt::entity parent, std::vector<entt::entity>& out) {
    if (!registry.valid(parent)) return;
    if (!registry.all_of<engine::Hierarchy>(parent)) return;

    auto& hierarchy = registry.get<engine::Hierarchy>(parent);
    for (auto child : hierarchy.children) {
        if (!registry.valid(child)) continue;
        out.push_back(child);
        collect_descendants(registry, child, out);
    }
}

bool DropdownInspector::draw(engine::ui::Dropdown& component, entt::registry& registry, entt::entity owner) {
    bool changed = false;

    // Enabled checkbox
    if (ImGui::Checkbox("Enabled", &component.enabled)) {
        changed = true;
    }

    SectionSeparator();

    // Selected index (combo box showing current options)
    if (!component.options.empty()) {
        // Clamp selected_index to valid range
        if (component.selected_index < 0) component.selected_index = 0;
        if (component.selected_index >= static_cast<int>(component.options.size())) {
            component.selected_index = static_cast<int>(component.options.size()) - 1;
        }

        const char* preview = component.options[component.selected_index].c_str();
        if (ImGui::BeginCombo("Selected", preview)) {
            for (int i = 0; i < static_cast<int>(component.options.size()); ++i) {
                bool is_selected = (component.selected_index == i);
                if (ImGui::Selectable(component.options[i].c_str(), is_selected)) {
                    component.selected_index = i;
                    component._options_dirty = true;
                    changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::Text("Selected: (no options)");
    }

    SectionSeparator();

    // Options list editor
    ImGui::Text("Options:");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##AddOption")) {
        component.options.push_back("New Option");
        component._options_dirty = true;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add new option");
    }

    int option_to_remove = -1;
    int option_to_move_up = -1;
    int option_to_move_down = -1;

    for (int i = 0; i < static_cast<int>(component.options.size()); ++i) {
        ImGui::PushID(i);

        // Up/Down buttons
        bool can_move_up = (i > 0);
        bool can_move_down = (i < static_cast<int>(component.options.size()) - 1);

        ImGui::BeginDisabled(!can_move_up);
        if (ImGui::SmallButton("^")) {
            option_to_move_up = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!can_move_down);
        if (ImGui::SmallButton("v")) {
            option_to_move_down = i;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Option text input
        char buf[256];
        strncpy(buf, component.options[i].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
        if (ImGui::InputText("##opt", buf, sizeof(buf))) {
            component.options[i] = buf;
            component._options_dirty = true;
            changed = true;
        }

        ImGui::SameLine();

        // Remove button
        if (ImGui::SmallButton("X")) {
            option_to_remove = i;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove this option");
        }

        ImGui::PopID();
    }

    // Process deferred operations
    if (option_to_move_up >= 0) {
        std::swap(component.options[option_to_move_up], component.options[option_to_move_up - 1]);
        if (component.selected_index == option_to_move_up) {
            component.selected_index--;
        } else if (component.selected_index == option_to_move_up - 1) {
            component.selected_index++;
        }
        component._options_dirty = true;
        changed = true;
    }

    if (option_to_move_down >= 0) {
        std::swap(component.options[option_to_move_down], component.options[option_to_move_down + 1]);
        if (component.selected_index == option_to_move_down) {
            component.selected_index++;
        } else if (component.selected_index == option_to_move_down + 1) {
            component.selected_index--;
        }
        component._options_dirty = true;
        changed = true;
    }

    if (option_to_remove >= 0) {
        component.options.erase(component.options.begin() + option_to_remove);
        if (component.selected_index >= static_cast<int>(component.options.size()) && !component.options.empty()) {
            component.selected_index = static_cast<int>(component.options.size()) - 1;
        }
        if (component.options.empty()) {
            component.selected_index = -1;
        }
        component._options_dirty = true;
        changed = true;
    }

    SectionSeparator();

    // Settings
    if (ImGui::DragInt("Max Visible Items", &component.max_visible_items, 1.0f, 0, 20)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum items visible before scrolling (0 = show all)");
    }

    if (ImGui::DragFloat("Item Height", &component.item_height, 1.0f, 16.0f, 64.0f)) {
        component._options_dirty = true;
        changed = true;
    }

    SectionSeparator();

    // Entity references
    ImGui::Text("Entity References:");

    // Helper to draw entity reference dropdown with label on left
    auto draw_entity_ref = [&](const char* label, entt::entity& entity_ref) -> bool {
        bool ref_changed = false;

        // Get current entity name
        std::string current_name = "(None)";
        if (entity_ref != entt::null && registry.valid(entity_ref)) {
            if (registry.all_of<EntityInfo>(entity_ref)) {
                current_name = registry.get<EntityInfo>(entity_ref).name;
            }
        }

        // Two-column layout: label on left, combo on right
        float label_width = 120.0f;
        ImGui::Text("%s", label);
        ImGui::SameLine(label_width);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        std::string combo_id = std::string("##") + label;
        if (ImGui::BeginCombo(combo_id.c_str(), current_name.c_str())) {
            // None option
            if (ImGui::Selectable("(None)", entity_ref == entt::null)) {
                entity_ref = entt::null;
                ref_changed = true;
            }

            // List all descendant entities (recursive)
            std::vector<entt::entity> descendants;
            collect_descendants(registry, owner, descendants);
            for (auto descendant : descendants) {
                if (!registry.all_of<EntityInfo>(descendant)) continue;

                auto& desc_info = registry.get<EntityInfo>(descendant);
                bool is_selected = (entity_ref == descendant);
                if (ImGui::Selectable(desc_info.name.c_str(), is_selected)) {
                    entity_ref = descendant;
                    ref_changed = true;
                }
            }

            ImGui::EndCombo();
        }

        return ref_changed;
    };

    if (draw_entity_ref("SelectedText", component.selected_text)) {
        changed = true;
    }

    if (draw_entity_ref("Arrow", component.arrow)) {
        changed = true;
    }

    if (draw_entity_ref("OptionsPanel", component.options_panel)) {
        changed = true;
    }

    if (draw_entity_ref("OptionsScrollView", component.options_scrollview)) {
        changed = true;
    }

    if (draw_entity_ref("OptionsContent", component.options_content)) {
        changed = true;
    }

    return changed;
}

} // namespace editor
