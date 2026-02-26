#include "HierarchyPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "engine/platform/PlatformUtils.h"

#include <imgui.h>
#include <algorithm>
#include <filesystem>

namespace editor {

HierarchyPanel::HierarchyPanel(EditorContext& context)
    : Panel("Hierarchy")
    , m_context(context)
{
}

void HierarchyPanel::on_gui() {
    render_toolbar();
    ImGui::Separator();

    ImGui::BeginChild("HierarchyContent", ImVec2(0, 0), false);

    auto* registry = m_context.registry();
    if (!registry) {
        ImGui::TextDisabled("No scene loaded");
        ImGui::EndChild();
        // Still need to handle prefab dialog below
    } else {
        // Pre-compute lowercase filter once per frame
        m_filter_lower.assign(m_filter);
        std::transform(m_filter_lower.begin(), m_filter_lower.end(), m_filter_lower.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

        // World Entities Section
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.5f, 0.5f));
        ImGuiTreeNodeFlags world_header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
        if (ImGui::CollapsingHeader(ICON_FA_GLOBE " World Entities", world_header_flags)) {
            m_world_section_open = true;
            render_world_hierarchy();
        } else {
            m_world_section_open = false;
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Screen Entities Section
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.35f, 0.2f, 0.5f));
        ImGuiTreeNodeFlags screen_header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
        if (ImGui::CollapsingHeader(ICON_FA_DISPLAY " Screen Entities", screen_header_flags)) {
            m_screen_section_open = true;
            render_screen_hierarchy();
        } else {
            m_screen_section_open = false;
        }
        ImGui::PopStyleColor();

        // Click on empty space to deselect
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            m_context.selection().clear_selection();
        }

        // Right-click context menu on empty space
        if (ImGui::BeginPopupContextWindow("HierarchyEmptyContextMenu",
                ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (m_world_section_open) {
                if (ImGui::BeginMenu(ICON_FA_GLOBE " Create World Entity")) {
                    if (ImGui::MenuItem("Empty Entity")) {
                        auto entity = create_entity(*registry, "New Entity");
                        m_context.selection().select(entity);
                        m_context.scene_state().mark_dirty();
                    }
                    ImGui::EndMenu();
                }
            }
            if (m_screen_section_open) {
                if (ImGui::BeginMenu(ICON_FA_DISPLAY " Create Screen Entity")) {
                    if (ImGui::MenuItem("Empty Screen Entity")) {
                        auto entity = create_screen_entity(*registry, "New Screen Entity");
                        m_context.selection().select(entity);
                        m_context.scene_state().mark_dirty();
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_context.clipboard().has_entity_clipboard())) {
                m_context.paste();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::EndChild();

    // Prefab save dialog
    if (m_show_prefab_save_dialog && m_pending_prefab_entity != entt::null) {
        ImGui::OpenPopup("Save as Prefab");
    }

    if (ImGui::BeginPopupModal("Save as Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto* popup_registry = m_context.registry();
        if (!popup_registry || !popup_registry->valid(m_pending_prefab_entity)) {
            m_show_prefab_save_dialog = false;
            m_pending_prefab_entity = entt::null;
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("Enter prefab name:");

            if (m_prefab_name_focus) {
                ImGui::SetKeyboardFocusHere();
                m_prefab_name_focus = false;
            }

            bool enter_pressed = ImGui::InputText("##PrefabName", m_prefab_name_buffer, sizeof(m_prefab_name_buffer),
                                                   ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0)) || enter_pressed) {
                // Get project prefabs folder path
                std::filesystem::path prefab_dir = std::filesystem::path(m_context.scene_state().project_path()) / "Assets" / "Prefabs";
                std::filesystem::create_directories(prefab_dir);

                std::filesystem::path prefab_path = prefab_dir / (std::string(m_prefab_name_buffer) + ".prefab");

                // Handle name collision
                int counter = 1;
                while (std::filesystem::exists(prefab_path)) {
                    prefab_path = prefab_dir / (std::string(m_prefab_name_buffer) + "_" + std::to_string(counter++) + ".prefab");
                }

                SceneSerializer serializer(*popup_registry);
                if (serializer.save_prefab(prefab_path, m_pending_prefab_entity)) {
                    // Mark the entity as a prefab instance
                    if (popup_registry->all_of<EntityInfo>(m_pending_prefab_entity)) {
                        auto& info = popup_registry->get<EntityInfo>(m_pending_prefab_entity);
                        info.is_prefab_instance = true;
                        info.prefab_path = prefab_path.string();
                    }
                    m_context.scene_state().mark_dirty();
                    m_context.refresh_file_browser();
                }

                m_show_prefab_save_dialog = false;
                m_pending_prefab_entity = entt::null;
                m_prefab_name_buffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_show_prefab_save_dialog = false;
                m_pending_prefab_entity = entt::null;
                m_prefab_name_buffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void HierarchyPanel::render_toolbar() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##HierarchyFilter", ICON_FA_MAGNIFYING_GLASS " Search...", m_filter, sizeof(m_filter));
}

void HierarchyPanel::render_world_hierarchy() {
    auto* registry = m_context.registry();
    if (!registry) return;

    // "+" button for world entities
    if (ImGui::SmallButton(ICON_FA_PLUS "##WorldAdd")) {
        ImGui::OpenPopup("CreateWorldEntityPopup");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Add world entity");

    if (ImGui::BeginPopup("CreateWorldEntityPopup")) {
        if (ImGui::MenuItem("Empty Entity")) {
            auto entity = create_entity(*registry, "New Entity");
            m_context.selection().select(entity);
            m_context.scene_state().mark_dirty();
        }
        if (ImGui::MenuItem("Child of Selected")) {
            auto& selection = m_context.selection().selection();
            if (!selection.empty() && is_world_space_entity(*registry, selection[0])) {
                auto entity = create_entity(*registry, "Child Entity");
                set_parent(*registry, entity, selection[0]);
                m_context.selection().select(entity);
                m_context.scene_state().mark_dirty();
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("From Prefab...")) {
            std::string path = engine::platform::open_file_dialog(
                "Select Prefab", {{"Prefab Files (*.prefab)", "*.prefab"}});
            if (!path.empty()) {
                SceneSerializer serializer(*registry);
                entt::entity e = serializer.load_prefab(path);
                if (e != entt::null) {
                    m_context.selection().select(e);
                    m_context.scene_state().mark_dirty();
                }
            }
        }
        ImGui::EndPopup();
    }

    auto roots = get_world_root_entities(*registry);
    if (roots.empty()) {
        ImGui::TextDisabled("  No world entities");
    } else {
        for (auto entity : roots) {
            render_entity_node(entity, 0, false);
        }
    }
}

void HierarchyPanel::render_screen_hierarchy() {
    auto* registry = m_context.registry();
    if (!registry) return;

    // "+" button for screen entities
    if (ImGui::SmallButton(ICON_FA_PLUS "##ScreenAdd")) {
        ImGui::OpenPopup("CreateScreenEntityPopup");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Add screen entity");

    if (ImGui::BeginPopup("CreateScreenEntityPopup")) {
        if (ImGui::MenuItem("Empty Screen Entity")) {
            auto entity = create_screen_entity(*registry, "New Screen Entity");
            m_context.selection().select(entity);
            m_context.scene_state().mark_dirty();
        }
        if (ImGui::MenuItem("Child of Selected")) {
            auto& selection = m_context.selection().selection();
            if (!selection.empty() && is_screen_space_entity(*registry, selection[0])) {
                auto entity = create_screen_entity(*registry, "Child Screen Entity");
                set_parent(*registry, entity, selection[0]);
                m_context.selection().select(entity);
                m_context.scene_state().mark_dirty();
            }
        }
        ImGui::EndPopup();
    }

    auto roots = get_screen_root_entities(*registry);
    if (roots.empty()) {
        ImGui::TextDisabled("  No screen entities");
    } else {
        for (auto entity : roots) {
            render_entity_node(entity, 0, true);
        }
    }
}

void HierarchyPanel::render_entity_node(entt::entity entity, int depth, bool is_screen_space) {
    auto* registry = m_context.registry();
    if (!registry || !registry->valid(entity)) return;

    // Get entity info
    std::string name = "Entity";
    bool enabled = true;
    bool is_prefab_instance = false;
    if (registry->all_of<EntityInfo>(entity)) {
        const auto& info = registry->get<EntityInfo>(entity);
        name = info.name;
        enabled = info.enabled_in_hierarchy;  // Use effective state (includes parent hierarchy)
        is_prefab_instance = info.is_prefab_instance;
    }

    // Prepend icon for prefab instances only (screen space is already indicated by section)
    std::string display_name;
    if (is_prefab_instance) {
        display_name = std::string(ICON_FA_CUBE) + " " + name;
    } else {
        display_name = name;
    }

    // Apply filter (m_filter_lower is pre-computed once per frame)
    if (!m_filter_lower.empty()) {
        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
        if (name_lower.find(m_filter_lower) == std::string::npos) {
            // Check children too
            if (registry->all_of<Hierarchy>(entity)) {
                const auto& hierarchy = registry->get<Hierarchy>(entity);
                for (auto child : hierarchy.children) {
                    render_entity_node(child, depth + 1, is_screen_space);
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

    if (m_context.selection().is_selected(entity)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Dim disabled entities
    if (!enabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    // Prefab instances get a subtle blue tint
    bool pushed_prefab_color = false;
    if (is_prefab_instance && enabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
        pushed_prefab_color = true;
    }

    // Render tree node
    bool is_open = ImGui::TreeNodeEx(display_name.c_str(), flags);

    if (pushed_prefab_color) {
        ImGui::PopStyleColor();
    }
    if (!enabled) {
        ImGui::PopStyleColor();
    }

    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (ImGui::GetIO().KeyCtrl) {
            // Toggle selection
            if (m_context.selection().is_selected(entity)) {
                m_context.selection().remove_from_selection(entity);
            } else {
                m_context.selection().add_to_selection(entity);
            }
        } else if (ImGui::GetIO().KeyShift) {
            // Add to selection
            m_context.selection().add_to_selection(entity);
        } else {
            // Single select
            m_context.selection().select(entity);
        }
    }

    // Show prefab path tooltip for prefab instances
    if (is_prefab_instance && ImGui::IsItemHovered()) {
        if (registry->all_of<EntityInfo>(entity)) {
            const auto& info = registry->get<EntityInfo>(entity);
            if (!info.prefab_path.empty()) {
                ImGui::SetTooltip("Prefab: %s", info.prefab_path.c_str());
            }
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
            m_context.selection().select(entity);  // Ensure this entity is selected
            m_context.duplicate_selection();
        }
        if (ImGui::MenuItem("Delete")) {
            destroy_entity_recursive(*registry, entity);
            m_context.selection().remove_from_selection(entity);
            m_context.scene_state().mark_dirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Create Child")) {
            entt::entity child;
            if (is_screen_space) {
                child = create_screen_entity(*registry, "Child Screen Entity");
            } else {
                child = create_entity(*registry, "Child Entity");
            }
            set_parent(*registry, child, entity);
            m_context.selection().select(child);
            m_context.scene_state().mark_dirty();
        }
        if (ImGui::MenuItem("Unparent")) {
            remove_from_parent(*registry, entity);
            m_context.scene_state().mark_dirty();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Make Prefab from Entity", nullptr, false, !is_prefab_instance)) {
            m_pending_prefab_entity = entity;
            m_show_prefab_save_dialog = true;
            m_prefab_name_focus = true;
            strncpy(m_prefab_name_buffer, name.c_str(), sizeof(m_prefab_name_buffer) - 1);
            m_prefab_name_buffer[sizeof(m_prefab_name_buffer) - 1] = '\0';
        }
        if (is_prefab_instance && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Entity is already linked to a prefab");
        }
        // Show "Unpack Prefab" only if this is a prefab instance
        if (registry->all_of<EntityInfo>(entity)) {
            auto& info = registry->get<EntityInfo>(entity);
            if (info.is_prefab_instance) {
                if (ImGui::MenuItem("Unpack Prefab")) {
                    info.is_prefab_instance = false;
                    info.prefab_path.clear();
                    m_context.scene_state().mark_dirty();
                }
                if (ImGui::IsItemHovered() && !info.prefab_path.empty()) {
                    ImGui::SetTooltip("Disconnect from: %s", info.prefab_path.c_str());
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            m_context.selection().select(entity);  // Ensure this entity is selected
            m_context.copy_selection();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_context.clipboard().has_entity_clipboard())) {
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
        // Peek at payload to show feedback before accepting
        if (const ImGuiPayload* peek = ImGui::GetDragDropPayload()) {
            if (peek->IsDataType("ENTITY") && peek->DataSize == sizeof(entt::entity)) {
                entt::entity dragged = *static_cast<entt::entity*>(peek->Data);
                if (dragged != entity) {
                    bool dragged_is_screen = is_screen_space_entity(*registry, dragged);
                    bool target_is_screen = is_screen_space_entity(*registry, entity);
                    if (dragged_is_screen != target_is_screen) {
                        // Show rejection feedback
                        ImGui::SetTooltip("Cannot parent: mixed world/screen space");
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY")) {
            if (payload->DataSize != sizeof(entt::entity)) { ImGui::EndDragDropTarget(); return; }
            entt::entity dragged = *static_cast<entt::entity*>(payload->Data);
            if (dragged != entity) {
                // Validate same-space before accepting
                bool dragged_is_screen = is_screen_space_entity(*registry, dragged);
                bool target_is_screen = is_screen_space_entity(*registry, entity);
                if (dragged_is_screen == target_is_screen) {
                    set_parent(*registry, dragged, entity);
                    m_context.scene_state().mark_dirty();
                }
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
                m_context.scene_state().mark_dirty();
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
                render_entity_node(child, depth + 1, is_screen_space);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

}