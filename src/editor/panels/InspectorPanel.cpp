#include "InspectorPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/ComponentTypeRegistry.h"
#include "editor/inspectors/AutoInspector.h"
#include "editor/inspectors/Camera2DInspector.h"
#include "editor/inspectors/AnimatorInspector.h"
#include "editor/inspectors/PixelGridComponentInspector.h"
#include "engine/reflection/TypeRegistry.h"
#include "engine/core/Transform.h"
#include "engine/core/Logger.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/scripting/ScriptManager.h"
#include "runtime/ScriptComponent.h"

#include <imgui.h>
#include <algorithm>
#include <map>

namespace editor {

InspectorPanel::InspectorPanel(EditorContext& context)
    : Panel("Inspector")
    , m_context(context)
{
}

void InspectorPanel::on_gui() {
    const auto& selection = m_context.selection();

    if (selection.empty()) {
        render_no_selection();
    } else if (selection.size() > 1) {
        render_multi_selection();
    } else {
        render_entity_inspector(selection[0]);
    }
}

void InspectorPanel::render_no_selection() {
    // When editing a prefab, don't show scene settings
    if (m_context.has_editing_override()) {
        ImGui::TextDisabled("No entity selected");
        return;
    }
    // Show scene settings instead of "no selection" message
    render_scene_settings();
}

void InspectorPanel::render_multi_selection() {
    ImGui::TextDisabled("%zu entities selected", m_context.selection().size());
    ImGui::TextDisabled("");
    ImGui::TextDisabled("Multi-entity editing");
    ImGui::TextDisabled("is not yet supported.");
}

void InspectorPanel::render_entity_inspector(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry || !registry->valid(entity)) {
        render_no_selection();
        return;
    }

    // Entity header with name and enabled toggle
    if (registry->all_of<EntityInfo>(entity)) {
        auto& info = registry->get<EntityInfo>(entity);

        // Enabled checkbox (uses Unity-style propagation)
        bool enabled = info.enabled;
        if (ImGui::Checkbox("##Enabled", &enabled)) {
            set_entity_enabled(*registry, entity, enabled);
            m_context.mark_dirty();
        }
        ImGui::SameLine();

        // Entity name (use entity ID for unique ImGui state per entity)
        char name_buffer[128];
        strncpy(name_buffer, info.name.c_str(), sizeof(name_buffer) - 1);
        name_buffer[sizeof(name_buffer) - 1] = '\0';

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##EntityName", name_buffer, sizeof(name_buffer))) {
            info.name = name_buffer;
            m_context.mark_dirty();
        }
        ImGui::PopID();

        // Show entity ID in tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Entity ID: %u", static_cast<unsigned int>(entt::to_integral(entity)));
            if (!info.guid.empty()) {
                ImGui::Text("GUID: %s", info.guid.c_str());
            }
            if (info.is_prefab_instance && !info.prefab_path.empty()) {
                ImGui::Text("Prefab: %s", info.prefab_path.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render all components dynamically using reflection
    auto& type_registry = engine::reflection::TypeRegistry::instance();
    const auto& all_types = type_registry.all_types();

    if (all_types.empty()) {
        ImGui::TextDisabled("No reflected types registered");
    }

    // Use ComponentTypeRegistry for dynamic component access
    auto& component_registry = ComponentTypeRegistry::instance();

    // Build ordered list of components this entity has, caching the void* pointers
    // to avoid a second get_component() round-trip during rendering.
    struct ComponentEntry {
        const engine::reflection::TypeInfo* type_info;
        void* ptr;
    };
    std::vector<ComponentEntry> present_components;
    for (const auto* type_info_ptr : all_types) {
        if (!type_info_ptr) continue;
        void* component_ptr = component_registry.get_component(*registry, entity, type_info_ptr->type_index());
        if (component_ptr) {
            present_components.push_back({type_info_ptr, component_ptr});
        }
    }

    // Apply custom component order if available
    {
        std::vector<const engine::reflection::TypeInfo*> order_vec;
        order_vec.reserve(present_components.size());
        for (auto& entry : present_components) order_vec.push_back(entry.type_info);
        apply_component_order(*registry, entity, order_vec);
        // Reorder present_components to match the sorted order
        std::vector<ComponentEntry> reordered;
        reordered.reserve(order_vec.size());
        for (const auto* ti : order_vec) {
            for (auto& entry : present_components) {
                if (entry.type_info == ti) {
                    reordered.push_back(entry);
                    break;
                }
            }
        }
        present_components = std::move(reordered);
    }

    // Render components in order, with drag-and-drop reordering
    for (size_t i = 0; i < present_components.size(); ++i) {
        auto& entry = present_components[i];
        render_component_inspector(entity, *entry.type_info, entry.ptr, i, present_components.size());
    }

    // Render script components (same visual style as engine components)
    if (registry->all_of<runtime::ScriptComponent>(entity)) {
        auto& sc = registry->get<runtime::ScriptComponent>(entity);
        for (size_t i = 0; i < sc.script_types.size(); ++i) {
            ImGui::PushID(("script_" + std::to_string(i)).c_str());

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
            bool open = ImGui::CollapsingHeader(sc.script_types[i].c_str(), flags);

            if (open) {
                // Delete button (same style as engine component delete)
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));

                if (ImGui::Button(ICON_FA_TRASH " Delete Component", ImVec2(-1, 0))) {
                    sc.remove_script_by_name(sc.script_types[i]);
                    if (sc.empty()) {
                        registry->remove<runtime::ScriptComponent>(entity);
                    }
                    m_context.mark_dirty();
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                    break; // List changed, bail out of loop
                }
                ImGui::PopStyleColor(3);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // During play mode, call on_inspector_gui if script instance exists
                if (i < sc.scripts.size() && sc.scripts[i]) {
                    sc.scripts[i]->on_inspector_gui();
                }
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    render_add_component_button(entity);
}

void InspectorPanel::render_transform_component(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry) return;

    auto& transform = registry->get<engine::Transform>(entity);

    ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    if (ImGui::CollapsingHeader("engine::Transform", header_flags)) {
        ImGui::PushID("engine::Transform");

        bool changed = false;

        // Position
        ImGui::Text("Position");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        float pos[2] = {transform.x, transform.y};
        if (ImGui::DragFloat2("##Position", pos, 1.0f)) {
            transform.x = pos[0];
            transform.y = pos[1];
            changed = true;
        }

        // Rotation
        ImGui::Text("Rotation");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Rotation", &transform.rotation, 0.5f)) {
            changed = true;
        }

        // Scale
        ImGui::Text("Scale");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        float scale[2] = {transform.scale_x, transform.scale_y};
        if (ImGui::DragFloat2("##Scale", scale, 0.01f)) {
            transform.scale_x = scale[0];
            transform.scale_y = scale[1];
            changed = true;
        }

        // Reset button
        if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Reset")) {
            transform.x = 0.0f;
            transform.y = 0.0f;
            transform.rotation = 0.0f;
            transform.scale_x = 1.0f;
            transform.scale_y = 1.0f;
            changed = true;
        }

        if (changed) {
            m_context.mark_dirty();
        }

        ImGui::PopID();
    }
}

void InspectorPanel::render_component_inspector(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr, size_t index, size_t count) {
    if (!component_ptr) return;

    ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    // Use unique ID scope for this component to avoid ImGui ID conflicts
    ImGui::PushID(type_info.name().c_str());

    // Check if this is a required component (can't be removed)
    bool is_required = (type_info.name() == "engine::Transform");

    bool header_open = ImGui::CollapsingHeader(type_info.name().c_str(), header_flags);

    // Drag-and-drop source (on collapsing header)
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("COMPONENT_REORDER", &index, sizeof(size_t));
        ImGui::Text("%s", type_info.name().c_str());
        ImGui::EndDragDropSource();
    }

    // Drag-and-drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMPONENT_REORDER")) {
            size_t from_index = *static_cast<const size_t*>(payload->Data);
            if (from_index != index) {
                move_component(entity, from_index, index);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Context menu for component header (right-click)
    if (ImGui::BeginPopupContextItem()) {
        // Move Up/Down
        if (ImGui::MenuItem("Move Up", nullptr, false, index > 0)) {
            move_component(entity, index, index - 1);
        }
        if (ImGui::MenuItem("Move Down", nullptr, false, index < count - 1)) {
            move_component(entity, index, index + 1);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Copy Component")) {
            copy_component_to_clipboard(entity, type_info, component_ptr);
        }

        // Paste: only if same type (updates values)
        bool can_paste = m_context.has_component_clipboard() &&
                        m_context.component_clipboard_type() == type_info.type_index();

        if (ImGui::MenuItem("Paste Component Values", nullptr, false, can_paste)) {
            paste_component_from_clipboard(entity, type_info.type_index());
        }

        ImGui::Separator();

        // Paste as New: any type (creates if missing)
        if (ImGui::MenuItem("Paste Component as New", nullptr, false,
                           m_context.has_component_clipboard())) {
            paste_component_as_new_from_clipboard(entity);
        }

        ImGui::EndPopup();
    }

    if (header_open) {

        bool changed = false;

        // Delete component button (for non-required components)
        if (!is_required) {
            ImGui::Spacing();

            // Red delete button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));

            if (ImGui::Button(ICON_FA_TRASH " Delete Component", ImVec2(-1, 0))) {
                ImGui::OpenPopup("Remove Component");
            }

            ImGui::PopStyleColor(3);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Confirmation popup for removing component
        if (!is_required && ImGui::BeginPopupModal("Remove Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Remove this component?");
            ImGui::Text("This action cannot be undone.");
            ImGui::Spacing();

            if (ImGui::Button("Remove", ImVec2(120, 0))) {
                remove_component_from_entity(entity, type_info.type_index());
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Use custom inspectors for specific component types
        if (type_info.name() == "engine::render::Camera2D") {
            changed = Camera2DInspector::draw(*static_cast<engine::render::Camera2D*>(component_ptr));
        }
        else if (type_info.name() == "engine::animation::Animator") {
            changed = AnimatorInspector::draw(*static_cast<engine::animation::Animator*>(component_ptr));
        }
        else if (type_info.name() == "engine::simulation::PixelGridComponent") {
            changed = PixelGridComponentInspector::draw(*static_cast<engine::simulation::PixelGridComponent*>(component_ptr));
        }
        // Add more custom inspectors here as needed
        else {
            // Fall back to AutoInspector for components without custom inspectors
            changed = AutoInspector::draw(type_info, component_ptr);
        }

        if (changed) {
            m_context.mark_dirty();
        }
    }

    // PopID to match the PushID at the top of the function
    ImGui::PopID();
}

void InspectorPanel::render_add_component_button(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry) return;

    float width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button(ICON_FA_PLUS " Add Component", ImVec2(width, 0))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        static char search[64] = "";
        ImGui::InputTextWithHint("##ComponentSearch", "Search components...", search, sizeof(search));
        ImGui::Separator();

        // Get all reflected types from TypeRegistry
        auto& type_registry = engine::reflection::TypeRegistry::instance();
        const auto& all_types = type_registry.all_types();

        // Group types by category
        std::map<std::string, std::vector<const engine::reflection::TypeInfo*>> categories;

        for (const auto* type_info : all_types) {
            if (!type_info) continue;

            // Skip if entity already has this component
            bool has_component = false;
            if (type_info->name() == "engine::Transform" && registry->all_of<engine::Transform>(entity)) {
                has_component = true;
            } else if (type_info->name() == "engine::render::Camera2D" && registry->all_of<engine::render::Camera2D>(entity)) {
                has_component = true;
            } else if (type_info->name() == "engine::animation::Animator" && registry->all_of<engine::animation::Animator>(entity)) {
                has_component = true;
            }

            if (has_component) continue;

            // Filter by search text
            std::string lower_name = type_info->name();
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            std::string lower_search = search;
            std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

            if (strlen(search) > 0 && lower_name.find(lower_search) == std::string::npos) {
                continue;
            }

            // Add to appropriate category
            std::string category = get_component_category(type_info->name());
            categories[category].push_back(type_info);
        }

        // Render components grouped by category
        for (const auto& [category, types] : categories) {
            ImGui::TextDisabled("-- %s --", category.c_str());

            for (const auto* type_info : types) {
                // Extract simple name (after last ::)
                std::string simple_name = type_info->name();
                size_t last_colon = simple_name.rfind("::");
                if (last_colon != std::string::npos) {
                    simple_name = simple_name.substr(last_colon + 2);
                }

                if (ImGui::MenuItem(simple_name.c_str())) {
                    add_component_to_entity(entity, type_info->type_index());
                    ImGui::CloseCurrentPopup();
                }

                // Show full name in tooltip
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", type_info->name().c_str());
                    ImGui::EndTooltip();
                }
            }
        }

        // Scripts category
        auto* sm = m_context.script_manager();
        bool any_scripts_shown = false;
        if (sm && sm->are_scripts_loaded()) {
            auto* sc = registry->try_get<runtime::ScriptComponent>(entity);

            std::vector<std::string> available_scripts;
            for (const auto& type_info : sm->dll_manager().script_types()) {
                if (type_info.is_system) continue;

                // Skip if already attached
                if (sc) {
                    auto it = std::find(sc->script_types.begin(), sc->script_types.end(), type_info.name);
                    if (it != sc->script_types.end()) continue;
                }

                // Filter by search text
                std::string lower_name = type_info.name;
                std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
                std::string lower_search = search;
                std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

                if (strlen(search) > 0 && lower_name.find(lower_search) == std::string::npos) {
                    continue;
                }

                available_scripts.push_back(type_info.name);
            }

            if (!available_scripts.empty()) {
                ImGui::TextDisabled("-- Scripts --");
                for (const auto& script_name : available_scripts) {
                    if (ImGui::MenuItem(script_name.c_str())) {
                        if (!sc) {
                            sc = &registry->emplace<runtime::ScriptComponent>(entity);
                        }
                        sc->script_types.push_back(script_name);
                        m_context.mark_dirty();
                        ImGui::CloseCurrentPopup();
                    }
                }
                any_scripts_shown = true;
            }
        }

        // Show message if nothing available
        if (categories.empty() && !any_scripts_shown) {
            ImGui::TextDisabled("No components available");
            if (strlen(search) > 0) {
                ImGui::TextDisabled("(try different search)");
            }
        }

        ImGui::EndPopup();
    }
}

void InspectorPanel::render_scene_settings() {
    auto& settings = m_context.scene_settings();

    ImGui::Text("Scene Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Background color
    ImGui::Text("Background Color");
    if (ImGui::ColorEdit4("##BgColor", settings.bg_color)) {
        m_context.mark_dirty();
    }

    ImGui::Spacing();

    // Physics settings
    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Gravity");
        float gravity[2] = {settings.gravity_x, settings.gravity_y};
        if (ImGui::DragFloat2("##Gravity", gravity, 10.0f)) {
            settings.gravity_x = gravity[0];
            settings.gravity_y = gravity[1];
            m_context.mark_dirty();
        }

        ImGui::Text("Pixels Per Meter");
        if (ImGui::DragFloat("##PPM", &settings.pixels_per_meter, 0.1f, 1.0f, 100.0f, "%.1f")) {
            m_context.mark_dirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Conversion factor for physics calculations");
        }

        ImGui::Text("Substeps");
        if (ImGui::DragInt("##Substeps", &settings.physics_substeps, 1, 1, 10)) {
            m_context.mark_dirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Higher values = more accurate but slower");
        }
    }
}

void InspectorPanel::add_component_to_entity(entt::entity entity, std::type_index type) {
    auto* registry = m_context.registry();
    if (!registry) return;

    auto& component_registry = ComponentTypeRegistry::instance();

    // Skip if entity already has this component
    if (component_registry.has_component(*registry, entity, type)) {
        return;
    }

    void* ptr = component_registry.create_component(*registry, entity, type);
    if (ptr) {
        // Auto-size BoxCollider from PixelGridComponent if available
        if (type == std::type_index(typeid(engine::physics::BoxCollider))) {
            auto& box = *static_cast<engine::physics::BoxCollider*>(ptr);
            if (registry->all_of<engine::simulation::PixelGridComponent>(entity)) {
                auto& grid = registry->get<engine::simulation::PixelGridComponent>(entity);
                if (grid.width > 0 && grid.height > 0) {
                    box.width = static_cast<float>(grid.width);
                    box.height = static_cast<float>(grid.height);
                    box.offset_x = (grid.width * 0.5f) - grid.origin_x;
                    box.offset_y = (grid.height * 0.5f) - grid.origin_y;
                }
            }
        }

        // Append to component order list
        if (registry->all_of<EntityInfo>(entity)) {
            auto& info = registry->get<EntityInfo>(entity);
            // Find type name from TypeRegistry
            auto& type_reg = engine::reflection::TypeRegistry::instance();
            for (const auto* ti : type_reg.all_types()) {
                if (ti && ti->type_index() == type) {
                    // Only add if order list is already populated (otherwise it will be built on next render)
                    if (!info.component_order.empty()) {
                        info.component_order.push_back(ti->name());
                    }
                    break;
                }
            }
        }

        m_context.mark_dirty();
        engine::Logger::instance().info("Inspector", "Added component to entity");
    } else {
        engine::Logger::instance().warning("Inspector", "Failed to add component - type not registered");
    }
}

void InspectorPanel::remove_component_from_entity(entt::entity entity, std::type_index type) {
    auto* registry = m_context.registry();
    if (!registry) return;

    // Transform is required, never remove it
    if (type == std::type_index(typeid(engine::Transform))) {
        return;
    }

    auto& component_registry = ComponentTypeRegistry::instance();
    component_registry.remove_component(*registry, entity, type);

    // Remove from component order list
    if (registry->all_of<EntityInfo>(entity)) {
        auto& info = registry->get<EntityInfo>(entity);
        auto& type_reg = engine::reflection::TypeRegistry::instance();
        for (const auto* ti : type_reg.all_types()) {
            if (ti && ti->type_index() == type) {
                auto it = std::find(info.component_order.begin(), info.component_order.end(), ti->name());
                if (it != info.component_order.end()) {
                    info.component_order.erase(it);
                }
                break;
            }
        }
    }

    m_context.mark_dirty();

    engine::Logger::instance().info("Inspector", "Removed component from entity");
}

void InspectorPanel::copy_component_to_clipboard(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr) {
    SceneSerializer serializer(*m_context.registry());
    nlohmann::json json = serializer.serialize_component(entity, type_info, component_ptr);

    m_context.set_component_clipboard(json.dump(), type_info.type_index());
}

void InspectorPanel::paste_component_from_clipboard(entt::entity entity, std::type_index type) {
    // Type check: only paste if same type (updates values)
    if (m_context.component_clipboard_type() != type) return;

    try {
        nlohmann::json json = nlohmann::json::parse(m_context.component_clipboard());
        SceneSerializer serializer(*m_context.registry());

        if (serializer.deserialize_component(entity, json)) {
            m_context.mark_dirty();
            engine::Logger::instance().info("Inspector", "Pasted component values");
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("Inspector", "Failed to paste component: %s", e.what());
    }
}

void InspectorPanel::paste_component_as_new_from_clipboard(entt::entity entity) {
    // No type check: creates component if missing
    try {
        nlohmann::json json = nlohmann::json::parse(m_context.component_clipboard());
        SceneSerializer serializer(*m_context.registry());

        if (serializer.deserialize_component(entity, json)) {
            m_context.mark_dirty();
            engine::Logger::instance().info("Inspector", "Pasted component as new");
        }
    } catch (const std::exception& e) {
        engine::Logger::instance().error("Inspector", "Failed to paste component: %s", e.what());
    }
}

void InspectorPanel::ensure_component_order(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry || !registry->all_of<EntityInfo>(entity)) return;

    auto& info = registry->get<EntityInfo>(entity);
    if (!info.component_order.empty()) return;

    // Initialize component_order from current component list
    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();
    const auto& all_types = type_registry.all_types();

    for (const auto* type_info_ptr : all_types) {
        if (!type_info_ptr) continue;
        void* ptr = component_registry.get_component(*registry, entity, type_info_ptr->type_index());
        if (ptr) {
            info.component_order.push_back(type_info_ptr->name());
        }
    }
}

void InspectorPanel::move_component(entt::entity entity, size_t from_index, size_t to_index) {
    auto* registry = m_context.registry();
    if (!registry || !registry->all_of<EntityInfo>(entity)) return;

    ensure_component_order(entity);

    auto& info = registry->get<EntityInfo>(entity);
    if (from_index >= info.component_order.size() || to_index >= info.component_order.size()) return;

    // Move the element
    std::string moving = info.component_order[from_index];
    info.component_order.erase(info.component_order.begin() + from_index);
    info.component_order.insert(info.component_order.begin() + to_index, moving);

    m_context.mark_dirty();
}

std::string InspectorPanel::get_component_category(const std::string& type_name) {
    // Categorize components based on namespace
    if (type_name.find("::render::") != std::string::npos) {
        return "Rendering";
    } else if (type_name.find("::animation::") != std::string::npos) {
        return "Animation";
    } else if (type_name.find("::physics::") != std::string::npos) {
        return "Physics";
    } else if (type_name.find("engine::Transform") != std::string::npos) {
        return "Core";
    }
    return "Other";
}

} // namespace editor
