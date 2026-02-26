#pragma once

#include "Panel.h"
#include <entt/entt.hpp>
#include <string>

namespace editor {

class EditorContext;

// Hierarchy panel showing the scene entity tree
class HierarchyPanel : public Panel {
public:
    explicit HierarchyPanel(EditorContext& context);

    void on_gui() override;

private:
    void render_toolbar();
    void render_world_hierarchy();
    void render_screen_hierarchy();
    void render_entity_node(entt::entity entity, int depth = 0, bool is_screen_space = false);

    EditorContext& m_context;
    char m_filter[128] = "";
    std::string m_filter_lower;
    entt::entity m_renaming_entity = entt::null;
    char m_rename_buffer[128] = "";
    bool m_focus_rename = false;

    bool m_world_section_open = true;
    bool m_screen_section_open = true;

    entt::entity m_pending_prefab_entity = entt::null;
    bool m_show_prefab_save_dialog = false;
    char m_prefab_name_buffer[256] = "";
    bool m_prefab_name_focus = false;
};

}
