#pragma once

#include "Panel.h"
#include "engine/animation/AnimatorController.h"
#include "engine/animation/AnimatorParameter.h"
#include "engine/animation/AnimatorState.h"
#include "engine/animation/StateTransition.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace editor {

class EditorContext;

// Animator Controller editor panel for editing .animstate files
class AnimatorPanel : public Panel {
public:
    explicit AnimatorPanel(EditorContext& context);
    ~AnimatorPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    void open_controller(const std::string& path);
    void new_controller();

    bool save_controller();
    bool save_controller_as(const std::string& path);

    bool has_unsaved_changes() const { return m_has_unsaved_changes; }

    const std::string& current_path() const { return m_current_path; }

private:
    void render_toolbar();
    void render_parameters_panel();
    void render_node_editor();
    void render_properties_panel();
    void render_state_properties(engine::animation::AnimatorState& state);
    void render_transition_properties(engine::animation::StateTransition& transition);

    void setup_node_editor();
    void cleanup_node_editor();
    int get_node_id(const std::string& state_name) const;
    std::string get_state_name(int node_id) const;
    int allocate_node_id(const std::string& state_name);
    int allocate_link_id();

    void create_state(const std::string& name, float x, float y);
    void delete_state(const std::string& name);
    void rename_state(const std::string& old_name, const std::string& new_name);

    void create_transition(const std::string& from, const std::string& to);
    void delete_transition(size_t index);

    void create_parameter(const std::string& name, engine::animation::ParameterType type);
    void delete_parameter(const std::string& name);

    void render_condition_editor(engine::animation::TransitionCondition& condition, int index);
    std::vector<std::string> get_parameter_names_for_type(engine::animation::ParameterType type) const;

    void show_open_dialog();
    void show_save_as_dialog();

    void mark_dirty();

    EditorContext& m_context;


    engine::animation::AnimatorController m_controller;
    std::string m_current_path;
    bool m_has_unsaved_changes = false;

    bool m_editor_initialized = false;
    std::unordered_map<std::string, int> m_state_to_node_id;
    std::unordered_map<int, std::string> m_node_id_to_state;
    int m_next_node_id = 100;
    int m_next_link_id = 1000000;

    static constexpr int ANY_STATE_NODE_ID = 1;

    int m_selected_node_id = -1;
    int m_selected_link_id = -1;
    std::string m_selected_state;
    int m_selected_transition_index = -1;

    bool m_show_context_menu = false;
    float m_context_menu_x = 0.0f;
    float m_context_menu_y = 0.0f;

    bool m_show_new_state_dialog = false;
    char m_new_state_name[64] = "";
    std::string m_new_state_clip;
    bool m_show_clip_picker_for_new_state = false;

    bool m_show_new_param_dialog = false;
    char m_new_param_name[64] = "";
    int m_new_param_type = 0;

    float m_properties_width = 300.0f;
    float m_parameters_width = 200.0f;

    bool m_show_open_dialog = false;
    bool m_show_save_dialog = false;
    char m_file_path_buffer[512] = "";

    float m_pan_x = 50.0f;
    float m_pan_y = 50.0f;
    float m_zoom_level = 1.0f;
    static constexpr float MIN_ZOOM = 0.25f;
    static constexpr float MAX_ZOOM = 2.0f;

    std::unordered_set<int> m_positioned_nodes;

    float m_any_state_pos_x = 50.0f;
    float m_any_state_pos_y = 50.0f;
    bool m_any_state_positioned = false;

    struct NodeBounds {
        float x, y, width, height;
    };
    std::unordered_map<int, NodeBounds> m_node_bounds;

    bool m_dragging_node = false;
    int m_drag_node_id = -1;
    ImVec2 m_drag_offset{0, 0};

    bool m_dragging_transition = false;
    int m_drag_from_node_id = -1;
    ImVec2 m_drag_start_pos{0, 0};
    ImVec2 m_drag_current_pos{0, 0};

    bool m_waiting_for_transition_target = false;
    int m_transition_source_node_id = -1;

    bool m_show_node_context_menu = false;
    int m_context_menu_node_id = -1;

    enum class CardinalDirection { North, South, East, West };
    CardinalDirection get_connection_direction(int from_node, int to_node) const;
    ImVec2 get_connection_point(int node_id, CardinalDirection dir) const;
    void draw_transition_arrow(ImDrawList* draw_list, ImVec2 from, ImVec2 to, ImU32 color, float thickness, bool is_selected);
    void render_custom_transitions();
    int get_node_at_position(const ImVec2& pos) const;
    int get_transition_at_position(const ImVec2& pos) const;
};

}