#pragma once

#include "Panel.h"
#include <entt/entt.hpp>

namespace editor {

class EditorContext;

/// Inspector panel for editing selected entity components.
class InspectorPanel : public Panel {
public:
    explicit InspectorPanel(EditorContext& context);

    void on_gui() override;

private:
    void render_no_selection();
    void render_multi_selection();
    void render_entity_inspector(entt::entity entity);
    void render_transform_component(entt::entity entity);
    void render_add_component_button(entt::entity entity);

    EditorContext& m_context;
};

} // namespace editor
