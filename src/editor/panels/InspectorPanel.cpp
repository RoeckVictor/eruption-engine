#include "InspectorPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"

#include <imgui.h>

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
    ImGui::TextDisabled("No entity selected");
    ImGui::TextDisabled("");
    ImGui::TextDisabled("Select an entity in the Hierarchy");
    ImGui::TextDisabled("or Viewport to inspect it.");
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

        // Enabled checkbox
        if (ImGui::Checkbox("##Enabled", &info.enabled)) {
            m_context.mark_dirty();
        }
        ImGui::SameLine();

        // Entity name
        static char name_buffer[128];
        strncpy(name_buffer, info.name.c_str(), sizeof(name_buffer) - 1);
        name_buffer[sizeof(name_buffer) - 1] = '\0';

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##EntityName", name_buffer, sizeof(name_buffer))) {
            info.name = name_buffer;
            m_context.mark_dirty();
        }

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

    // Transform component
    if (registry->all_of<Transform>(entity)) {
        render_transform_component(entity);
    }

    // TODO: Render other components dynamically

    ImGui::Spacing();
    render_add_component_button(entity);
}

void InspectorPanel::render_transform_component(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry) return;

    auto& transform = registry->get<Transform>(entity);

    ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

    if (ImGui::CollapsingHeader("Transform", header_flags)) {
        ImGui::PushID("Transform");

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
        if (ImGui::Button("Reset")) {
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

void InspectorPanel::render_add_component_button(entt::entity entity) {
    auto* registry = m_context.registry();
    if (!registry) return;

    float width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Add Component", ImVec2(width, 0))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        static char search[64] = "";
        ImGui::InputTextWithHint("##ComponentSearch", "Search...", search, sizeof(search));
        ImGui::Separator();

        // TODO: List available components dynamically
        // For now, show some placeholders

        ImGui::TextDisabled("-- Physics --");
        if (ImGui::MenuItem("Rigid Body")) {
            // TODO: Add RigidBody component
        }
        if (ImGui::MenuItem("Pixel Body")) {
            // TODO: Add PixelBody component
        }

        ImGui::TextDisabled("-- Rendering --");
        if (ImGui::MenuItem("Sprite Renderer")) {
            // TODO: Add SpriteRenderer component
        }

        ImGui::TextDisabled("-- Gameplay --");
        if (ImGui::MenuItem("Script")) {
            // TODO: Add script component
        }

        ImGui::EndPopup();
    }
}

} // namespace editor
