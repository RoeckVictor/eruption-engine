#pragma once

#include "Panel.h"
#include <entt/entt.hpp>
#include <string>

namespace editor {

class EditorContext;

/// Hierarchy panel showing the scene entity tree.
class HierarchyPanel : public Panel {
public:
    explicit HierarchyPanel(EditorContext& context);

    void on_gui() override;

private:
    void render_toolbar();
    void render_entity_tree();
    void render_entity_node(entt::entity entity, int depth = 0);

    EditorContext& m_context;
    char m_filter[128] = "";
    std::string m_filter_lower;  // Cached lowercase filter, updated once per frame
    entt::entity m_renaming_entity = entt::null;
    char m_rename_buffer[128] = "";
    bool m_focus_rename = false;  // Flag to set focus on next frame

    // Prefab creation dialog
    entt::entity m_pending_prefab_entity = entt::null;
    bool m_show_prefab_save_dialog = false;
    char m_prefab_name_buffer[256] = "";
    bool m_prefab_name_focus = false;
};

} // namespace editor
