#include "HierarchyPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/icons/IconsFontAwesome6.h"

#include <imgui.h>
#include <algorithm>

namespace editor {

HierarchyPanel::HierarchyPanel(EditorContext& context)
    : Panel("Hierarchy")
    , m_context(context)
{
}

void HierarchyPanel::on_gui() {
    render_toolbar();
    ImGui::Separator();
    render_entity_tree();
}

void HierarchyPanel::render_toolbar() {
    if (ImGui::Button(ICON_FA_PLUS)) {
        ImGui::OpenPopup("CreateEntityPopup");
    }

    if (ImGui::BeginPopup("CreateEntityPopup")) {
        if (ImGui::MenuItem("Empty Entity")) {
            if (auto* registry = m_context.registry()) {
                auto entity = create_entity(*registry, "New Entity");
                m_context.select(entity);
                m_context.mark_dirty();
            }
        }
        if (ImGui::MenuItem("Child of Selected")) {
            if (auto* registry = m_context.registry()) {
                auto& selection = m_context.selection();
                if (!selection.empty()) {
                    auto entity = create_entity(*registry, "Child Entity");
                    set_parent(*registry, entity, selection[0]);
                    m_context.select(entity);
                    m_context.mark_dirty();
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("From Prefab...")) {
            // TODO: Open prefab picker
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##HierarchyFilter", "Search...", m_filter, sizeof(m_filter));
}

void HierarchyPanel::render_entity_tree() {
    ImGui::BeginChild("EntityTree", ImVec2(0, 0), false);

    auto* registry = m_context.registry();
    if (!registry) {
        ImGui::TextDisabled("No scene loaded");
        ImGui::EndChild();
        return;
    }

    // Pre-compute lowercase filter once per frame (used by render_entity_node)
    m_filter_lower.assign(m_filter);
    std::transform(m_filter_lower.begin(), m_filter_lower.end(), m_filter_lower.begin(),
                   [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    // Get all root entities (entities without parents)
    auto roots = get_root_entities(*registry);

    if (roots.empty()) {
        ImGui::TextDisabled("Scene is empty");
        ImGui::TextDisabled("");
        ImGui::TextDisabled("Click + to create an entity");
    } else {
        for (auto entity : roots) {
            render_entity_node(entity, 0);
        }
    }

    // Click on empty space to deselect
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
        m_context.clear_selection();
    }

    // Right-click context menu on empty space
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            if (registry) {
                auto entity = create_entity(*registry, "New Entity");
                m_context.select(entity);
                m_context.mark_dirty();
            }
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_context.has_clipboard())) {
            m_context.paste();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void HierarchyPanel::render_entity_node(entt::entity entity, int depth) {
    auto* registry = m_context.registry();
    if (!registry || !registry->valid(entity)) return;

    // Get entity info
    std::string name = "Entity";
    bool enabled = true;
    if (registry->all_of<EntityInfo>(entity)) {
        const auto& info = registry->get<EntityInfo>(entity);
        name = info.name;
        enabled = info.enabled_in_hierarchy;  // Use effective state (includes parent hierarchy)
    }

    // Apply filter (m_filter_lower is pre-computed once per frame in render_entity_tree)
    if (!m_filter_lower.empty()) {
        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
        if (name_lower.find(m_filter_lower) == std::string::npos) {
            // Check children too
            if (registry->all_of<Hierarchy>(entity)) {
                const auto& hierarchy = registry->get<Hierarchy>(entity);
                for (auto child : hierarchy.children) {
                    render_entity_node(child, depth + 1);
                }
            }
            return;
        }
    }

    ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

    // Check if this entity has children
    bool has_children = false;
    if (registry->all_of<Hierarchy>(entity)) {
        const auto& hierarchy = registry->get<Hierarchy>(entity);
        has_children = !hierarchy.children.empty();
    }

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (m_context.is_selected(entity)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Dim disabled entities
    if (!enabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    // Render tree node
    bool is_open = ImGui::TreeNodeEx(name.c_str(), flags);

    if (!enabled) {
        ImGui::PopStyleColor();
    }

    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (ImGui::GetIO().KeyCtrl) {
            // Toggle selection
            if (m_context.is_selected(entity)) {
                m_context.remove_from_selection(entity);
            } else {
                m_context.add_to_selection(entity);
            }
        } else if (ImGui::GetIO().KeyShift) {
            // Add to selection
            m_context.add_to_selection(entity);
        } else {
            // Single select
            m_context.select(entity);
        }
    }

    // Double-click to rename
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        m_renaming_entity = entity;
        m_focus_rename = true;
        strncpy(m_rename_buffer, name.c_str(), sizeof(m_rename_buffer) - 1);
        m_rename_buffer[sizeof(m_rename_buffer) - 1] = '\0';
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename", "F2")) {
            m_renaming_entity = entity;
            m_focus_rename = true;
            strncpy(m_rename_buffer, name.c_str(), sizeof(m_rename_buffer) - 1);
            m_rename_buffer[sizeof(m_rename_buffer) - 1] = '\0';
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            m_context.select(entity);  // Ensure this entity is selected
            m_context.duplicate_selection();
        }
        if (ImGui::MenuItem("Delete")) {
            destroy_entity_recursive(*registry, entity);
            m_context.remove_from_selection(entity);
            m_context.mark_dirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Create Child")) {
            auto child = create_entity(*registry, "Child Entity");
            set_parent(*registry, child, entity);
            m_context.select(child);
            m_context.mark_dirty();
        }
        if (ImGui::MenuItem("Unparent")) {
            remove_from_parent(*registry, entity);
            m_context.mark_dirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            m_context.select(entity);  // Ensure this entity is selected
            m_context.copy_selection();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_context.has_clipboard())) {
            m_context.paste();
        }
        ImGui::EndPopup();
    }

    // Drag and drop for reparenting
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY", &entity, sizeof(entity));
        ImGui::Text("%s", name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            if (payload->DataSize != sizeof(entt::entity)) { ImGui::EndDragDropTarget(); return; }
            entt::entity dragged = *static_cast<entt::entity*>(payload->Data);
            if (dragged != entity) {
                set_parent(*registry, dragged, entity);
                m_context.mark_dirty();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Rename input
    if (m_renaming_entity == entity) {
        // Set focus on the first frame
        if (m_focus_rename) {
            ImGui::SetKeyboardFocusHere();
            m_focus_rename = false;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Rename", m_rename_buffer, sizeof(m_rename_buffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (registry->all_of<EntityInfo>(entity)) {
                registry->get<EntityInfo>(entity).name = m_rename_buffer;
                m_context.mark_dirty();
            }
            m_renaming_entity = entt::null;
        }

        // Cancel on Escape
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_renaming_entity = entt::null;
        }
        // Cancel if clicked elsewhere (but not on the first frame)
        else if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) {
            m_renaming_entity = entt::null;
        }
    }

    // Render children
    if (is_open) {
        if (has_children) {
            const auto& hierarchy = registry->get<Hierarchy>(entity);
            for (auto child : hierarchy.children) {
                render_entity_node(child, depth + 1);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // namespace editor
