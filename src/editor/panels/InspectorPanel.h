#pragma once

#include "Panel.h"
#include "editor/inspectors/RecordingContext.h"
#include <entt/entt.hpp>
#include <typeindex>
#include <string>

namespace engine::reflection {
    class TypeInfo;
}

namespace editor {

class EditorContext;

// Inspector panel for editing selected entity components
class InspectorPanel : public Panel {
public:
    explicit InspectorPanel(EditorContext& context);

    void on_gui() override;

private:
    void render_no_selection();
    void render_multi_selection();
    void render_entity_inspector(entt::entity entity);
    void render_scene_settings();
    void render_transform_component(entt::entity entity);
    void render_component_inspector(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr, size_t index = 0, size_t count = 0);
    void render_add_component_button(entt::entity entity);

    void add_component_to_entity(entt::entity entity, std::type_index type);
    void remove_component_from_entity(entt::entity entity, std::type_index type);
    std::string get_component_category(const std::string& type_name);

    void copy_component_to_clipboard(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr);
    void paste_component_from_clipboard(entt::entity entity, std::type_index type);
    void paste_component_as_new_from_clipboard(entt::entity entity);

    void move_component(entt::entity entity, size_t from_index, size_t to_index);
    void ensure_component_order(entt::entity entity);

    RecordingContext create_recording_context(entt::entity entity, const std::string& component_name = "");

    EditorContext& m_context;
};

}
